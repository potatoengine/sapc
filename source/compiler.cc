// sapc - by Sean Middleditch
// This is free and unencumbered software released into the public domain.
// See LICENSE.md for more details.

#include "compiler.hh"
#include "context.hh"
#include "ast.hh"
#include "lexer.hh"
#include "grammar.hh"
#include "schema.hh"
#include "log.hh"
#include "file_util.hh"
#include "overload.hh"

#include <unordered_map>
#include <unordered_set>
#include <cassert>

namespace fs = std::filesystem;

namespace sapc {
    using namespace std::literals;

    static constexpr auto typeIdName = "$sapc.typeid"sv;

    namespace {
        struct PathHash {
            using result_type = std::uint64_t;
            result_type operator()(fs::path const& path) const noexcept {
                return hash_value(path);
            }
        };

        struct State {
            std::unique_ptr<ast::ModuleUnit> unit;
            schema::Module* mod = nullptr;
            std::unordered_set<schema::Type const*> resolvedTypes;
        };

        struct Compiler {
            Context& ctx;
            Log& log;

            std::vector<State> state;

            schema::Type const* typeIdType = nullptr;
            schema::Module const* coreModule = nullptr;

            std::unordered_map<schema::Type const*, schema::Type const*> arrayTypeMap;
            std::unordered_map<schema::Type const*, schema::Type const*> pointerTypeMap;
            std::unordered_map<fs::path, schema::Module const*, PathHash> moduleMap;

            schema::Module const* compile(fs::path const& filename);

            void build(ast::StructDecl const& structDecl);
            void build(ast::AttributeDecl const& attrDecl);
            void build(ast::UnionDecl const& unionDecl);
            void build(ast::EnumDecl const& enumDecl);
            void build(ast::ConstantDecl const& constantDecl);
            void build(ast::ImportDecl const& impDecl);

            template <typename SchemaType>
            void build(SchemaType& type, ast::Field const& field);
            void build(schema::TypeUnion& type, ast::Member const& member);
            void build(schema::TypeEnum& type, ast::EnumItem const& item);

            void createCoreModule();

            schema::Type const* findType(ast::QualifiedId const& qualId, schema::Module const* scope);
            schema::Type const* findType(ast::QualifiedId const& qualId);
            schema::Type const* findType(ast::TypeRef const& ref);

            schema::Type const* resolveType(schema::Type const* type);
            schema::Type const* resolveType(ast::QualifiedId const& qualId);
            schema::Type const* resolveType(ast::TypeRef const& ref);

            schema::Type const* createArrayType(schema::Type const* of, Location const& loc);
            schema::Type const* createPointerType(schema::Type const* of, Location const& loc);

            schema::Type const* requireType(ast::TypeRef const& ref);

            schema::EnumItem const* findEnumItem(schema::TypeEnum const* enumType, std::string_view name);

            schema::Value translate(ast::Literal const& lit);
            std::unique_ptr<schema::Annotation> translate(ast::Annotation const& anno);
            auto translate(std::vector<ast::Annotation> const& annotations)->std::vector<std::unique_ptr<schema::Annotation>>;
        };
    }

    bool compile(Context& ctx, Log& log) {
        Compiler compiler{ ctx, log };
        ctx.rootModule = compiler.compile(ctx.targetFile);
        return ctx.rootModule != nullptr && log.countErrors == 0;
    }

    void Compiler::build(ast::StructDecl const& structDecl) {
        assert(!state.empty());
        assert(state.back().mod != nullptr);

        auto& mod = *state.back().mod;

        auto* const type = static_cast<schema::TypeStruct*>(ctx.types.emplace_back(std::make_unique<schema::TypeStruct>()).get());
        type->name = structDecl.name.id;
        type->kind = schema::Type::Kind::Struct;
        type->parent = &mod;
        type->location = structDecl.name.loc;
        if (structDecl.baseType != nullptr)
            type->baseType = requireType(*structDecl.baseType);
        type->annotations = translate(structDecl.annotations);

        for (ast::Field const& field : structDecl.fields)
            build(*type, field);

        mod.types.push_back(type);
    }

    void Compiler::build(ast::AttributeDecl const& attrDecl) {
        assert(!state.empty());
        assert(state.back().mod != nullptr);

        auto& mod = *state.back().mod;

        auto* const type = static_cast<schema::TypeAttribute*>(ctx.types.emplace_back(std::make_unique<schema::TypeAttribute>()).get());
        type->name = attrDecl.name.id;
        type->kind = schema::Type::Kind::Attribute;
        type->parent = &mod;
        type->location = attrDecl.name.loc;
        type->annotations = translate(attrDecl.annotations);

        for (ast::Field const& field : attrDecl.fields)
            build(*type, field);

        mod.types.push_back(type);
    }

    void Compiler::build(ast::EnumDecl const& enumDecl) {
        assert(!state.empty());
        assert(state.back().mod != nullptr);

        auto& mod = *state.back().mod;

        auto* const type = static_cast<schema::TypeEnum*>(ctx.types.emplace_back(std::make_unique<schema::TypeEnum>()).get());
        type->name = enumDecl.name.id;
        type->kind = schema::Type::Enum;
        type->parent = &mod;
        type->location = enumDecl.name.loc;
        type->annotations = translate(enumDecl.annotations);

        for (ast::EnumItem const& item : enumDecl.items)
            build(*type, item);

        mod.types.push_back(type);
    }

    void Compiler::build(ast::ConstantDecl const& constantDecl) {
        assert(!state.empty());
        assert(state.back().mod != nullptr);

        auto& mod = *state.back().mod;

        auto* el = ctx.constants.emplace_back(std::make_unique<schema::Constant>()).get();
        mod.constants.push_back(el);
        el->name = constantDecl.name.id;
        el->location = constantDecl.name.loc;
        el->parent = &mod;
        el->type = requireType(*constantDecl.type);
        el->value = translate(constantDecl.value);
    }

    void Compiler::build(ast::UnionDecl const& unionDecl) {
        assert(!state.empty());
        assert(state.back().mod != nullptr);

        auto& mod = *state.back().mod;

        auto* const type = static_cast<schema::TypeUnion*>(ctx.types.emplace_back(std::make_unique<schema::TypeUnion>()).get());
        type->name = unionDecl.name.id;
        type->kind = schema::Type::Union;
        type->parent = &mod;
        type->location = unionDecl.name.loc;
        type->annotations = translate(unionDecl.annotations);

        for (ast::Member const& member : unionDecl.members)
            build(*type, member);

        mod.types.push_back(type);
    }

    template <typename SchemaType>
    void Compiler::build(SchemaType& type, ast::Field const& fieldDecl) {
        auto& field = type.fields.emplace_back(std::make_unique<schema::Field>());
        field->name = fieldDecl.name.id;
        field->location = fieldDecl.name.loc;
        field->type = requireType(*fieldDecl.type);
        field->annotations = translate(fieldDecl.annotations);
        if (fieldDecl.init)
            field->defaultValue = translate(*fieldDecl.init);
    }

    void Compiler::build(schema::TypeUnion& type, ast::Member const& memberDecl) {
        auto& member = type.members.emplace_back(std::make_unique<schema::Member>());
        member->name = memberDecl.name.id;
        member->location = memberDecl.name.loc;
        member->type = requireType(*memberDecl.type);
        member->annotations = translate(memberDecl.annotations);
    }

    void Compiler::build(schema::TypeEnum& type, ast::EnumItem const& itemDecl) {
        auto* const item = type.items.emplace_back(std::make_unique<schema::EnumItem>()).get();
        item->name = itemDecl.name.id;
        item->location = itemDecl.name.loc;
        item->parent = &type;
        item->value = itemDecl.value;
        item->annotations = translate(itemDecl.annotations);
    }

    void Compiler::build(ast::ImportDecl const& impDecl) {
        assert(!state.empty());
        assert(state.back().mod != nullptr);

        auto& mod = *state.back().mod;

        auto const parent = state.back().unit->filename.parent_path();
        auto const basename = fs::path{ impDecl.target.id }.replace_extension(".sap");
        auto const filename = resolveFile(basename, parent, ctx.searchPaths);
        if (filename.empty()) {
            log.error(impDecl.target.loc, impDecl.target.id, ": module not found");
            return;
        }

        auto it = moduleMap.find(filename);
        if (it != moduleMap.end())
            return;

        if (auto const* imp = compile(filename); imp != nullptr)
            mod.imports.push_back(imp);
    }

    schema::Module const* Compiler::compile(fs::path const& filename) {
        ctx.dependencies.push_back(filename);

        auto unit = parse(filename, log);
        if (!unit)
            return nullptr;

        createCoreModule();

        ctx.modules.push_back(std::make_unique<schema::Module>());
        auto* const mod = ctx.modules.back().get();
        mod->name = unit->name.id;
        mod->location = unit->name.loc;

        state.push_back(State{ move(unit), mod });

        for (auto const& decl : state.back().unit->decls) {
            if (decl->kind == ast::Declaration::Kind::Struct)
                build(*static_cast<ast::StructDecl*>(decl.get()));
            else if (decl->kind == ast::Declaration::Kind::Union)
                build(*static_cast<ast::UnionDecl*>(decl.get()));
            else if (decl->kind == ast::Declaration::Kind::Attribute)
                build(*static_cast<ast::AttributeDecl*>(decl.get()));
            else if (decl->kind == ast::Declaration::Kind::Enum)
                build(*static_cast<ast::EnumDecl*>(decl.get()));
            else if (decl->kind == ast::Declaration::Kind::Constant)
                build(*static_cast<ast::ConstantDecl*>(decl.get()));
            else if (decl->kind == ast::Declaration::Kind::Import)
                build(*static_cast<ast::ImportDecl*>(decl.get()));
            else if (decl->kind == ast::Declaration::Kind::Module)
                for (auto& anno : static_cast<ast::ModuleDecl*>(decl.get())->annotations)
                    mod->annotations.push_back(translate(anno));
            else
                assert(false && "Unknown declartion kind");
        }

        state.pop_back();

        return mod;
    }

    void Compiler::createCoreModule() {
        if (coreModule != nullptr)
            return;

        // add built-in types
        static constexpr std::string_view builtins[] = {
            "string"sv,
            "bool"sv,
            "byte"sv,
            "int"sv,
            "float"sv,
        };

        schema::Module* mod = ctx.modules.emplace_back(std::make_unique<schema::Module>()).get();
        coreModule = mod;
        mod->name = "$sapc";

        for (auto const& builtin : builtins) {
            auto* type = ctx.types.emplace_back(std::make_unique<schema::Type>()).get();
            mod->types.push_back(type);

            type->kind = schema::Type::Primitive;
            type->name = builtin;
            type->parent = coreModule;
        }

        {
            auto* type = ctx.types.emplace_back(std::make_unique<schema::Type>()).get();
            mod->types.push_back(type);
            typeIdType = type;

            type->kind = schema::Type::TypeId;
            type->name = typeIdName;
            type->parent = coreModule;
        }
    }

    schema::Type const* Compiler::findType(ast::QualifiedId const& qualId, schema::Module const* scope) {
        assert(!qualId.empty());
        assert(scope != nullptr);

        if (qualId.components.size() != 1) // FIXME: resolve enums?
            return nullptr;

        for (auto const* type : scope->types)
            if (type->name == qualId.components.front().id)
                return type;

        for (auto const* imp : scope->imports)
            for (auto const* type : imp->types)
                if (type->name == qualId.components.front().id)
                    return type;

        if (coreModule != nullptr)
            if (auto* type = findType(qualId, coreModule); type != nullptr)
                return type;

        return nullptr;
    }

    schema::Type const* Compiler::findType(ast::QualifiedId const& qualId) {
        assert(!qualId.empty());

        State& top = state.back();

        return findType(qualId, top.mod);
    }

    schema::Type const* Compiler::findType(ast::TypeRef const& ref) {
        switch (ref.kind) {
        case ast::TypeRef::Kind::TypeName:
            return typeIdType;
        case ast::TypeRef::Kind::Name:
            return findType(ref.name);
        case ast::TypeRef::Kind::Array:
            if (auto const* type = findType(*ref.ref); type != nullptr) {
                auto const it = arrayTypeMap.find(type);
                if (it != arrayTypeMap.end())
                    return it->second;
                return arrayTypeMap.emplace(type, createArrayType(type, ref.loc)).first->second;
            }
            return nullptr;
        case ast::TypeRef::Kind::Pointer:
            if (auto const* type = findType(*ref.ref); type != nullptr) {
                auto const it = pointerTypeMap.find(type);
                if (it != pointerTypeMap.end())
                    return it->second;
                return pointerTypeMap.emplace(type, createPointerType(type, ref.loc)).first->second;
            }
            return nullptr;
        default:
            return nullptr;
        }
    }

    schema::Type const* Compiler::resolveType(schema::Type const* type) {
        if (type == nullptr)
            return nullptr;

        assert(!state.empty());
        assert(state.back().mod != nullptr);

        State& top = state.back();

        if (type->parent != top.mod) {
            bool const resolved = top.resolvedTypes.find(type) != top.resolvedTypes.end();
            if (!resolved) {
                top.mod->types.push_back(type);
                top.resolvedTypes.insert(type);
            }
        }

        return type;
    }

    schema::Type const* Compiler::resolveType(ast::QualifiedId const& qualId) {
        return resolveType(findType(qualId));
    }

    schema::Type const* Compiler::resolveType(ast::TypeRef const& ref) {
        return resolveType(findType(ref));
    }

    schema::Type const* Compiler::createArrayType(schema::Type const* of, Location const& loc) {
        assert(of != nullptr);

        auto& top = state.back();

        auto* arr = static_cast<schema::TypeArray*>(ctx.types.emplace_back(std::make_unique<schema::TypeArray>()).get());
        top.mod->types.push_back(arr);

        arr->name = of->name;
        arr->name += "[]";
        arr->of = of;
        arr->kind = schema::Type::Array;
        arr->parent = top.mod;
        arr->location = loc;
        return arr;
    }

    schema::Type const* Compiler::createPointerType(schema::Type const* to, Location const& loc) {
        assert(to != nullptr);

        auto& top = state.back();

        auto* ptr = static_cast<schema::TypePointer*>(ctx.types.emplace_back(std::make_unique<schema::TypePointer>()).get());
        top.mod->types.push_back(ptr);

        ptr->name = to->name;
        ptr->name += "*";
        ptr->to = to;
        ptr->kind = schema::Type::Pointer;
        ptr->parent = top.mod;
        ptr->location = loc;
        return ptr;
    }

    schema::Type const* Compiler::requireType(ast::TypeRef const& ref) {
        if (auto type = resolveType(ref); type != nullptr)
            return type;

        log.error(ref.loc, ref, ": type not found");
        return nullptr;
    }

    schema::EnumItem const* Compiler::findEnumItem(schema::TypeEnum const* enumType, std::string_view name) {
        assert(enumType != nullptr);
        for (auto const& item : enumType->items)
            if (item->name == name)
                return item.get();
        return nullptr;
    }

    schema::Value Compiler::translate(ast::Literal const& lit) {
        schema::Value value;
        value.location = lit.loc;
        std::visit([&value, &lit, this](auto const& val) {
            using T = std::remove_cv_t<std::remove_reference_t<decltype(val)>>;
            if constexpr (std::is_same_v<T, ast::QualifiedId>) {
                schema::Type const* type = resolveType(val);
                if (type == nullptr) {
                    ast::QualifiedId id = val;
                    id.components.pop_back();
                    type = resolveType(id);
                    if (type == nullptr) {
                        log.error(lit.loc, val, ": type not found");
                    }
                    else if (type->kind != schema::Type::Kind::Enum) {
                        log.error(lit.loc, id, ": type does not name an enum");
                    }
                    else if (schema::EnumItem const* const item = findEnumItem(static_cast<schema::TypeEnum const*>(type), val.components.back().id); item != nullptr) {
                        value.data = item;
                    }
                    else {
                        log.error(lit.loc, val, ": enumeration value not found");
                        log.info(type->location, type->name, ": enumeration type defined here");
                    }
                }
                else
                    value.data = type;
            }
            else if constexpr (std::is_same_v<T, std::vector<ast::Literal>>) {
                auto arr = std::vector<schema::Value>();
                for (auto const& elem : val)
                    arr.push_back(translate(elem));
                value.data = std::move(arr);
            }
            else {
                value.data = val;
            }
            }, lit.data);
        return value;
    }

    std::unique_ptr<schema::Annotation> Compiler::translate(ast::Annotation const& anno) {
        auto result = std::make_unique<schema::Annotation>();

        result->type = findType(anno.name);
        result->location = anno.name.components.front().loc;
        result->location.merge(anno.name.components.back().loc);

        if (result->type == nullptr) {
            log.error(result->location, anno.name, ": attribute not found");
            return result;
        }
        if (result->type->kind != schema::Type::Kind::Attribute) {
            log.error(result->location, anno.name, ": annotation type is not an attribute");
            log.info(result->type->location, result->type->name, ": type declared here");
            return result;
        }

        auto const& attrType = static_cast<schema::TypeAttribute const&>(*result->type);

        if (anno.args.size() > attrType.fields.size()) {
            log.error(result->location, "too many arguments for attribute ", attrType.name, "; got ", anno.args.size(), ", expected ", attrType.fields.size());
            log.info(attrType.location, attrType.name, ": attribute declared here");
            return result;
        }

        for (size_t index = 0; index != attrType.fields.size(); ++index) {
            if (index < anno.args.size())
                result->args.push_back(translate(anno.args[index]));
            else if (attrType.fields[index]->defaultValue)
                result->args.push_back(*attrType.fields[index]->defaultValue);
            else {
                log.error(result->location, "missing parameter ", attrType.fields[index]->name);
                result->args.emplace_back();
            }
        }

        assert(result->args.size() == attrType.fields.size());
        return result;
    }

    auto Compiler::translate(std::vector<ast::Annotation> const& annotations) -> std::vector<std::unique_ptr<schema::Annotation>> {
        std::vector<std::unique_ptr<schema::Annotation>> results;
        for (auto const& anno : annotations)
            results.push_back(translate(anno));
        return results;
    }
}
