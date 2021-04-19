// sapc - by Sean Middleditch
// This is free and unencumbered software released into the public domain.
// See LICENSE.md for more details.

#include "ast.hh"
#include "compiler.hh"
#include "context.hh"
#include "file_util.hh"
#include "grammar.hh"
#include "hash_util.hh"
#include "lexer.hh"
#include "log.hh"
#include "overload.hh"
#include "schema.hh"

#include <unordered_map>
#include <unordered_set>
#include <cassert>

namespace fs = std::filesystem;

namespace sapc {
    using namespace std::literals;

    static constexpr auto typeIdName = "$sapc.typeid"sv;
    static constexpr auto customTagName = "$sapc.customtag"sv;

    namespace {
        // std::span isn't in C++17
        struct QualIdSpan {
            /*implicit*/ QualIdSpan(ast::QualifiedId const& qualId) : first(qualId.components.data()), last(qualId.components.data() + qualId.components.size()) {}
            QualIdSpan(ast::Identifier const* start, ast::Identifier const* end) : first(start), last(end) {}
            QualIdSpan(ast::QualifiedId const& qualId, size_t skip) : first(qualId.components.data() + skip), last(qualId.components.data() + qualId.components.size()) {}

            bool empty() const noexcept { return first == last; }
            size_t size() const noexcept { return last - first; }

            ast::Identifier const* begin() const noexcept { return first; }
            ast::Identifier const* end() const noexcept { return last; }

            ast::Identifier const& front() const noexcept { return *first; }

            QualIdSpan skip(size_t count = 1) const noexcept { return { first + count, last }; }

            auto hash() const noexcept {
                size_t hash = 0;
                for (auto* it = first; it != last; ++it)
                    hash = hash_combine(it->id, hash);
                return hash;
            }

            ast::Identifier const* first = nullptr;
            ast::Identifier const* last = nullptr;

            friend bool operator==(QualIdSpan lhs, QualIdSpan rhs) noexcept {
                if (lhs.size() != rhs.size())
                    return false;

                if (lhs.first == rhs.first)
                    return true;

                for (auto const* lp = lhs.first, *rp = rhs.first; lp != lhs.last; ++lp, ++rp)
                    if (lp->id != rp->id)
                        return false;

                return true;
            }
        };

        struct QualIdSpanHash {
            using result_type = size_t;
            result_type operator()(QualIdSpan qualId) const noexcept {
                return qualId.hash();
            }
        };

        struct PathHash {
            using result_type = std::uint64_t;
            result_type operator()(fs::path const& path) const noexcept {
                return hash_value(path);
            }
        };

        struct Resolve {
            enum class Kind { Empty, Type, Constant, Namespace, EnumItem };

            Resolve() = default;
            explicit Resolve(schema::Type const* type) : kind(Kind::Type) { data.type = type; }
            explicit Resolve(schema::Constant const* constant) : kind(Kind::Constant) { data.constant = constant; }
            explicit Resolve(schema::Namespace const* ns) : kind(Kind::Namespace) { data.ns = ns; }
            explicit Resolve(schema::EnumItem const* enumItem) : kind(Kind::EnumItem) { data.enumItem = enumItem; }

            bool empty() const noexcept { return kind == Kind::Empty; }
            explicit operator bool() const noexcept { return kind != Kind::Empty; }

            Kind kind = Kind::Empty;
            union {
                schema::Type const* type = nullptr;
                schema::Constant const* constant;
                schema::Namespace const* ns;
                schema::EnumItem const* enumItem;
            } data;
        };

        struct State {
            std::unique_ptr<ast::ModuleUnit> unit;
            schema::Module* mod = nullptr;
            std::vector<schema::Namespace*> nsStack;
            std::unordered_set<schema::Type const*> importedTypes;
            std::unordered_map<QualIdSpan, Resolve, QualIdSpanHash> resolveCache;
        };

        struct Compiler {
            Context& ctx;
            Log& log;

            std::vector<State> state;

            schema::Type const* typeIdType = nullptr;
            schema::TypeAttribute const* customTagAttr = nullptr;
            schema::Module const* coreModule = nullptr;

            std::unordered_map<schema::Type const*, schema::Type const*> arrayTypeMap;
            std::unordered_map<schema::Type const*, schema::Type const*> pointerTypeMap;
            std::unordered_map<size_t, schema::Type const*> specializedTypeMap;
            std::unordered_map<fs::path, schema::Module const*, PathHash> moduleMap;
            std::unordered_map<std::string_view, ast::CustomTagDecl const*> customTagMap;

            schema::Module const* compile(fs::path const& filename);

            void build(ast::Declaration const& decl);
            void build(ast::NamespaceDecl const& nsDecl);
            void build(ast::StructDecl const& structDecl);
            void build(ast::AliasDecl const& aliasDecl);
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

            schema::Type const* makeAvailable(schema::Type const* type);

            void makeAvailableRecurse(schema::Type const& type);
            void makeAvailableRecurse(schema::Field const& field);
            void makeAvailableRecurse(schema::Member const& member);
            void makeAvailableRecurse(schema::Annotation const& annotation);
            void makeAvailableRecurse(schema::Value const& value);

            schema::Type const* resolveType(ast::TypeRef const& ref, schema::Type const* scope = nullptr);

            Resolve findLocal(QualIdSpan qualId, schema::Namespace const* scope);
            Resolve findLocal(QualIdSpan qualId, schema::Type const* scope);
            Resolve findGlobal(QualIdSpan qualId, schema::Namespace const* scope);
            Resolve findGlobal(QualIdSpan qualId, schema::Type const* scope);
            Resolve findGlobal(QualIdSpan qualId, schema::Module const* scope);
            Resolve resolve(QualIdSpan qualId, schema::Type const* scope = nullptr);

            schema::Type const* requireType(QualIdSpan qualId, schema::Type const* scope = nullptr);
            schema::Type const* requireType(ast::TypeRef const& ref, schema::Type const* scope = nullptr);

            schema::Type const* createArrayType(schema::Type const* of, Location const& loc);
            schema::Type const* createPointerType(schema::Type const* to, Location const& loc);
            schema::Type const* createSpecializedType(schema::Type const* gen, std::vector<schema::Type const*> const& typeParams, Location const& loc);

            schema::Value translate(ast::Literal const& lit);
            std::unique_ptr<schema::Annotation> translate(ast::Annotation const& anno);
            void translate(std::vector<std::unique_ptr<schema::Annotation>>& target, std::vector<ast::Annotation> const& annotations);

            std::string qualify(std::string_view name) const;
        };
    }

    bool compile(Context& ctx, Log& log) {
        Compiler compiler{ ctx, log };
        ctx.rootModule = compiler.compile(ctx.targetFile);
        return ctx.rootModule != nullptr && log.countErrors == 0;
    }

    void Compiler::build(ast::Declaration const& decl) {
        switch (decl.kind) {
        case ast::Declaration::Kind::Namespace:
            return build(static_cast<ast::NamespaceDecl const&>(decl));
        case ast::Declaration::Kind::Struct:
            return build(static_cast<ast::StructDecl const&>(decl));
        case ast::Declaration::Kind::Alias:
            return build(static_cast<ast::AliasDecl const&>(decl));
        case ast::Declaration::Kind::Union:
            return build(static_cast<ast::UnionDecl const&>(decl));
        case ast::Declaration::Kind::Attribute:
            return build(static_cast<ast::AttributeDecl const&>(decl));
        case ast::Declaration::Kind::Enum:
            return build(static_cast<ast::EnumDecl const&>(decl));
        case ast::Declaration::Kind::Constant:
            return build(static_cast<ast::ConstantDecl const&>(decl));
        case ast::Declaration::Kind::Import:
            return build(static_cast<ast::ImportDecl const&>(decl));
        case ast::Declaration::Kind::Module:
            for (auto& anno : static_cast<ast::ModuleDecl const&>(decl).annotations)
                state.back().mod->annotations.push_back(translate(anno));
            break;
        case ast::Declaration::Kind::CustomTag:
            customTagMap.insert({ static_cast<ast::CustomTagDecl const&>(decl).name.id, static_cast<ast::CustomTagDecl const*>(&decl) });
            break;
        default:
            assert(false && "Unsupported declaration");
        }
    }

    void Compiler::build(ast::NamespaceDecl const& nsDecl) {
        schema::Namespace* ns = ctx.namespaces.emplace_back(std::make_unique<schema::Namespace>()).get();

        ns->name = nsDecl.name.id;
        ns->qualifiedName = qualify(ns->name);
        ns->location = nsDecl.name.loc;
        ns->owner = state.back().mod;
        ns->scope = state.back().nsStack.back();

        state.back().mod->namespaces.push_back(ns);
        state.back().nsStack.back()->namespaces.push_back(ns);
        state.back().nsStack.push_back(ns);

        for (auto const& decl : nsDecl.decls)
            build(*decl);

        state.back().nsStack.pop_back();
    }

    void Compiler::build(ast::StructDecl const& structDecl) {
        assert(!state.empty());
        assert(!state.back().nsStack.empty());
        assert(state.back().mod != nullptr);

        auto& mod = *state.back().mod;

        auto* const type = static_cast<schema::TypeStruct*>(ctx.types.emplace_back(std::make_unique<schema::TypeStruct>()).get());
        type->name = structDecl.name.id;
        type->qualifiedName = qualify(type->name);
        type->kind = schema::Type::Kind::Struct;
        type->owner = &mod;
        type->scope = state.back().nsStack.back();
        type->location = structDecl.name.loc;
        if (structDecl.baseType != nullptr)
            type->baseType = requireType(*structDecl.baseType);
        translate(type->annotations, structDecl.annotations);

        if (!structDecl.customTag.empty()) {
            auto it = customTagMap.find(structDecl.customTag);
            assert(it != customTagMap.end());
            translate(type->annotations, it->second->annotations);

            auto& tagAnno = *type->annotations.emplace_back(std::make_unique<schema::Annotation>());
            tagAnno.type = makeAvailable(customTagAttr);
            tagAnno.location = { std::filesystem::absolute(__FILE__), {__LINE__ } };
            auto& tagValue = tagAnno.args.emplace_back();
            tagValue.data = structDecl.customTag;
            tagValue.location = { std::filesystem::absolute(__FILE__), {__LINE__ } };
        }

        // Build generics before fields, as fields might refer to a generic
        type->generics.reserve(structDecl.generics.size());
        for (ast::Identifier const& gen : structDecl.generics) {
            auto* const sub = static_cast<schema::TypeGeneric*>(ctx.types.emplace_back(std::make_unique<schema::TypeGeneric>()).get());
            sub->name = gen.id;
            sub->qualifiedName = type->qualifiedName + "." + gen.id;
            sub->kind = schema::Type::Kind::Generic;
            sub->owner = &mod;
            sub->scope = type->scope;
            sub->parent = type;
            type->generics.push_back(sub);

            mod.types.push_back(sub);
        }

        type->fields.reserve(structDecl.fields.size());
        for (ast::Field const& field : structDecl.fields)
            build(*type, field);

        mod.types.push_back(type);
        state.back().nsStack.back()->types.push_back(type);
    }

    void Compiler::build(ast::AliasDecl const& aliasDecl) {
        assert(!state.empty());
        assert(!state.back().nsStack.empty());
        assert(state.back().mod != nullptr);

        auto& mod = *state.back().mod;

        auto* const type = static_cast<schema::TypeAlias*>(ctx.types.emplace_back(std::make_unique<schema::TypeAlias>()).get());
        type->name = aliasDecl.name.id;
        type->qualifiedName = qualify(type->name);
        type->kind = schema::Type::Kind::Alias;
        type->owner = &mod;
        type->scope = state.back().nsStack.back();
        type->location = aliasDecl.name.loc;
        translate(type->annotations, aliasDecl.annotations);

        if (aliasDecl.targetType != nullptr)
            type->ref = requireType(*aliasDecl.targetType, type);

        mod.types.push_back(type);
        state.back().nsStack.back()->types.push_back(type);
    }

    void Compiler::build(ast::AttributeDecl const& attrDecl) {
        assert(!state.empty());
        assert(!state.back().nsStack.empty());
        assert(state.back().mod != nullptr);

        auto& mod = *state.back().mod;

        auto* const type = static_cast<schema::TypeAttribute*>(ctx.types.emplace_back(std::make_unique<schema::TypeAttribute>()).get());
        type->name = attrDecl.name.id;
        type->qualifiedName = qualify(type->name);
        type->kind = schema::Type::Kind::Attribute;
        type->owner = &mod;
        type->scope = state.back().nsStack.back();
        type->location = attrDecl.name.loc;
        translate(type->annotations, attrDecl.annotations);

        type->fields.reserve(attrDecl.fields.size());
        for (ast::Field const& field : attrDecl.fields)
            build(*type, field);

        mod.types.push_back(type);
        state.back().nsStack.back()->types.push_back(type);
    }

    void Compiler::build(ast::EnumDecl const& enumDecl) {
        assert(!state.empty());
        assert(!state.back().nsStack.empty());
        assert(state.back().mod != nullptr);

        auto& mod = *state.back().mod;

        auto* const type = static_cast<schema::TypeEnum*>(ctx.types.emplace_back(std::make_unique<schema::TypeEnum>()).get());
        type->name = enumDecl.name.id;
        type->qualifiedName = qualify(type->name);
        type->kind = schema::Type::Enum;
        type->owner = &mod;
        type->scope = state.back().nsStack.back();
        type->location = enumDecl.name.loc;
        translate(type->annotations, enumDecl.annotations);

        type->items.reserve(enumDecl.items.size());
        for (ast::EnumItem const& item : enumDecl.items)
            build(*type, item);

        mod.types.push_back(type);
        state.back().nsStack.back()->types.push_back(type);
    }

    void Compiler::build(ast::UnionDecl const& unionDecl) {
        assert(!state.empty());
        assert(!state.back().nsStack.empty());
        assert(state.back().mod != nullptr);

        auto& mod = *state.back().mod;

        auto* const type = static_cast<schema::TypeUnion*>(ctx.types.emplace_back(std::make_unique<schema::TypeUnion>()).get());
        type->name = unionDecl.name.id;
        type->qualifiedName = qualify(type->name);
        type->kind = schema::Type::Union;
        type->owner = &mod;
        type->scope = state.back().nsStack.back();
        type->location = unionDecl.name.loc;
        translate(type->annotations, unionDecl.annotations);

        type->members.reserve(unionDecl.members.size());
        for (ast::Member const& member : unionDecl.members)
            build(*type, member);

        mod.types.push_back(type);
        state.back().nsStack.back()->types.push_back(type);
    }

    void Compiler::build(ast::ConstantDecl const& constantDecl) {
        assert(!state.empty());
        assert(!state.back().nsStack.empty());
        assert(state.back().mod != nullptr);

        auto& mod = *state.back().mod;

        auto* constant = ctx.constants.emplace_back(std::make_unique<schema::Constant>()).get();
        constant->name = constantDecl.name.id;
        constant->qualifiedName = qualify(constant->name);
        constant->location = constantDecl.name.loc;
        constant->owner = &mod;
        constant->scope = state.back().nsStack.back();
        constant->type = requireType(*constantDecl.type);
        constant->value = translate(constantDecl.value);

        mod.constants.push_back(constant);
        state.back().nsStack.back()->constants.push_back(constant);
    }

    template <typename SchemaType>
    void Compiler::build(SchemaType& type, ast::Field const& fieldDecl) {
        auto& field = type.fields.emplace_back(std::make_unique<schema::Field>());
        field->name = fieldDecl.name.id;
        field->location = fieldDecl.name.loc;
        field->type = requireType(*fieldDecl.type, &type);
        translate(field->annotations, fieldDecl.annotations);
        if (fieldDecl.init)
            field->defaultValue = translate(*fieldDecl.init);
    }

    void Compiler::build(schema::TypeUnion& type, ast::Member const& memberDecl) {
        auto& member = type.members.emplace_back(std::make_unique<schema::Member>());
        member->name = memberDecl.name.id;
        member->location = memberDecl.name.loc;
        member->type = requireType(*memberDecl.type, &type);
        translate(member->annotations, memberDecl.annotations);
    }

    void Compiler::build(schema::TypeEnum& type, ast::EnumItem const& itemDecl) {
        auto* const item = type.items.emplace_back(std::make_unique<schema::EnumItem>()).get();
        item->name = itemDecl.name.id;
        item->location = itemDecl.name.loc;
        item->parent = &type;
        item->value = itemDecl.value;
        translate(item->annotations, itemDecl.annotations);
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
        ctx.namespaces.push_back(std::make_unique<schema::Namespace>());
        auto* const ns = ctx.namespaces.back().get();
        auto* const mod = ctx.modules.back().get();
        mod->name = unit->name.id;
        mod->location = unit->name.loc;
        mod->root = ns;
        ns->owner = mod;

        state.push_back(State{ move(unit), mod });
        state.back().nsStack.push_back(ctx.namespaces.back().get());

        for (auto const& decl : state.back().unit->decls)
            build(*decl);

        state.pop_back();

        return mod;
    }

    void Compiler::createCoreModule() {
        if (coreModule != nullptr)
            return;

        // add built-in types
        static constexpr std::string_view builtins[] = {
            "string"sv, // the first type must be string
            "bool"sv,
            "byte"sv,
            "int"sv,
            "float"sv,
        };

        schema::Namespace* ns = ctx.namespaces.emplace_back(std::make_unique<schema::Namespace>()).get();
        schema::Module* mod = ctx.modules.emplace_back(std::make_unique<schema::Module>()).get();
        mod->root = ns;
        coreModule = mod;
        mod->name = "$sapc";

        for (auto const& builtin : builtins) {
            auto* const type = ctx.types.emplace_back(std::make_unique<schema::Type>()).get();
            mod->types.push_back(type);
            ns->types.push_back(type);

            type->kind = schema::Type::Primitive;
            type->name = builtin;
            type->qualifiedName = builtin;
            type->owner = coreModule;
            type->scope = ns;
            type->location = { std::filesystem::absolute(__FILE__), {__LINE__ } };
        }

        {
            auto* const type = ctx.types.emplace_back(std::make_unique<schema::Type>()).get();
            mod->types.push_back(type);
            ns->types.push_back(type);
            typeIdType = type;

            type->kind = schema::Type::TypeId;
            type->name = typeIdName;
            type->qualifiedName = typeIdName;
            type->owner = coreModule;
            type->scope = ns;
            type->location = { std::filesystem::absolute(__FILE__), {__LINE__ } };
        }

        {
            auto* const type = static_cast<schema::TypeAttribute*>(ctx.types.emplace_back(std::make_unique<schema::TypeAttribute>()).get());
            mod->types.push_back(type);
            ns->types.push_back(type);
            customTagAttr = type;

            type->kind = schema::Type::Attribute;
            type->name = customTagName;
            type->qualifiedName = customTagName;
            type->owner = coreModule;
            type->scope = ns;
            type->location = { std::filesystem::absolute(__FILE__), {__LINE__ } };

            auto* const field = type->fields.emplace_back(std::make_unique<schema::Field>()).get();
            field->name = "tag";
            field->type = ns->types.front(); // the first type is always string
            field->location = { std::filesystem::absolute(__FILE__), {__LINE__ } };
        }
    }

    schema::Type const* Compiler::makeAvailable(schema::Type const* type) {
        if (type != nullptr)
            makeAvailableRecurse(*type);
        return type;
    }

    void Compiler::makeAvailableRecurse(schema::Type const& type) {
        schema::Module* const mod = state.back().mod;

        if (type.owner == mod)
            return;

        auto const rs = state.back().importedTypes.insert(&type);
        if (!rs.second)
            return;

        // add the type to the module
        mod->types.push_back(&type);

        for (auto const& anno : type.annotations)
            makeAvailableRecurse(*anno);

        if (type.kind == schema::Type::Kind::Struct) {
            auto const& typeStruct = static_cast<schema::TypeStruct const&>(type);
            makeAvailable(typeStruct.baseType);
            for (auto const& field : typeStruct.fields)
                makeAvailableRecurse(*field);
        }
        else if (type.kind == schema::Type::Kind::Attribute) {
            auto const& typeAttr = static_cast<schema::TypeAttribute const&>(type);
            for (auto const& field : typeAttr.fields)
                makeAvailableRecurse(*field);
        }
        else if (type.kind == schema::Type::Kind::Union) {
            auto const& typeUnion = static_cast<schema::TypeUnion const&>(type);
            for (auto const& member : typeUnion.members)
                makeAvailableRecurse(*member);
        }
        else if (type.kind == schema::Type::Kind::Alias) {
            auto const& typeAlias = static_cast<schema::TypeAlias const&>(type);
            makeAvailable(typeAlias.ref);
        }
        else if (type.kind == schema::Type::Kind::Pointer) {
            auto const& typePointer = static_cast<schema::TypePointer const&>(type);
            makeAvailable(typePointer.to);
        }
        else if (type.kind == schema::Type::Kind::Array) {
            auto const& typeArray = static_cast<schema::TypeArray const&>(type);
            makeAvailable(typeArray.of);
        }
        else if (type.kind == schema::Type::Kind::Specialized) {
            auto const& typeSpec = static_cast<schema::TypeSpecialized const&>(type);
            makeAvailable(typeSpec.ref);
            for (auto const* typeParam : typeSpec.typeParams)
                makeAvailableRecurse(*typeParam);
        }
    }

    void Compiler::makeAvailableRecurse(schema::Field const& field) {
        makeAvailableRecurse(*field.type);
        if (field.defaultValue)
            makeAvailableRecurse(*field.defaultValue);
        for (auto const& anno : field.annotations)
            makeAvailableRecurse(*anno);
    }

    void Compiler::makeAvailableRecurse(schema::Member const& member) {
        makeAvailableRecurse(*member.type);
        for (auto const& anno : member.annotations)
            makeAvailableRecurse(*anno);
    }

    void Compiler::makeAvailableRecurse(schema::Annotation const& annotation) {
        makeAvailableRecurse(*annotation.type);
        for (auto const& arg : annotation.args)
            makeAvailableRecurse(arg);
    }

    void Compiler::makeAvailableRecurse(schema::Value const& value) {
        std::visit([this](auto const& value) {
            if constexpr (std::is_same_v<schema::Type const*, std::remove_cv_t<decltype(value)>>)
                makeAvailable(value);
            }, value.data);
    }

    schema::Type const* Compiler::resolveType(ast::TypeRef const& ref, schema::Type const* scope) {
        switch (ref.kind) {
        case ast::TypeRef::Kind::TypeName:
            return makeAvailable(typeIdType);
        case ast::TypeRef::Kind::Name:
            if (auto const rs = resolve(ref.name, scope); rs.kind == Resolve::Kind::Type)
                return makeAvailable(rs.data.type);
            else
                return nullptr;
        case ast::TypeRef::Kind::Array:
            if (auto const* type = resolveType(*ref.ref, scope); type != nullptr)
                return createArrayType(type, ref.loc);
            return nullptr;
        case ast::TypeRef::Kind::Pointer:
            if (auto const* type = resolveType(*ref.ref, scope); type != nullptr)
                return createPointerType(type, ref.loc);
            return nullptr;
        case ast::TypeRef::Kind::Generic:
            if (auto const* type = resolveType(*ref.ref, scope); type != nullptr) {
                std::vector<schema::Type const*> typeParams;
                typeParams.reserve(ref.typeParams.size());
                for (auto const& refParam : ref.typeParams) {
                    auto const* typeParam = requireType(*refParam, scope);
                    if (typeParam == nullptr)
                        return nullptr;
                    typeParams.push_back(typeParam);
                }
                return createSpecializedType(type, typeParams, ref.loc);
            }
            return nullptr;
        default:
            return nullptr;
        }
    }

    Resolve Compiler::findLocal(QualIdSpan qualId, schema::Type const* scope) {
        assert(!qualId.empty());
        assert(scope != nullptr);

        // We don't have nested types yet
        if (qualId.size() != 1)
            return {};

        if (scope->kind == schema::Type::Kind::Enum) {
            auto const& typeEnum = *static_cast<schema::TypeEnum const*>(scope);
            for (auto const& item : typeEnum.items)
                if (item->name == qualId.front().id)
                    return Resolve{ item.get() };
        }
        else if (scope->kind == schema::Type::Kind::Struct) {
            auto const& typeStruct = *static_cast<schema::TypeStruct const*>(scope);
            for (auto const* gen : typeStruct.generics)
                if (gen->name == qualId.front().id)
                    return Resolve{ gen };
        }

        return {};
    }

    Resolve Compiler::findLocal(QualIdSpan qualId, schema::Namespace const* scope) {
        assert(!qualId.empty());
        assert(scope != nullptr);

        if (qualId.size() == 1) {
            for (auto const* ns : scope->namespaces)
                if (ns->name == qualId.front().id)
                    return Resolve{ ns };

            for (auto const* type : scope->types)
                if (type->name == qualId.front().id)
                    return Resolve{ makeAvailable(type) };

            for (auto const* constant : scope->constants)
                if (constant->name == qualId.front().id)
                    return Resolve{ constant };
        }
        else {
            for (auto const* ns : scope->namespaces)
                if (ns->name == qualId.front().id)
                    if (auto const rs = findLocal(qualId.skip(1), ns))
                        return rs;

            for (auto const* scopeType : scope->types)
                if (scopeType->name == qualId.front().id)
                    if (auto const rs = findLocal(qualId.skip(1), scopeType))
                        return makeAvailable(scopeType), rs;
        }

        return {};
    }

    Resolve Compiler::findGlobal(QualIdSpan qualId, schema::Namespace const* scope) {
        assert(!qualId.empty());
        assert(scope != nullptr);

        if (auto const rs = findLocal(qualId, scope))
            return rs;

        if (scope->scope != nullptr)
            return findGlobal(qualId, scope->scope);
        else
            return findGlobal(qualId, scope->owner);
    }

    Resolve Compiler::findGlobal(QualIdSpan qualId, schema::Type const* scope) {
        assert(!qualId.empty());
        assert(scope != nullptr);

        if (auto const rs = findLocal(qualId, scope))
            return rs;

        if (scope->scope != nullptr)
            return findGlobal(qualId, scope->scope);
        else
            return findGlobal(qualId, scope->owner);
    }

    Resolve Compiler::findGlobal(QualIdSpan qualId, schema::Module const* scope) {
        assert(!qualId.empty());
        assert(scope != nullptr);

        if (auto const rs = findLocal(qualId, scope->root))
            return rs;

        for (auto const* imp : scope->imports)
            if (auto const rs = findLocal(qualId, imp->root))
                return rs;

        if (coreModule != nullptr)
            if (auto const rs = findLocal(qualId, coreModule->root))
                return rs;

        return {};
    }

    Resolve Compiler::resolve(QualIdSpan qualId, schema::Type const* scope) {
        auto it = state.back().resolveCache.find(qualId);
        if (it != state.back().resolveCache.end())
            return it->second;

        Resolve rs;
        if (scope != nullptr)
            rs = findGlobal(qualId, scope);
        else
            rs = findGlobal(qualId, state.back().mod);

        if (rs.kind != Resolve::Kind::Empty)
            state.back().resolveCache.insert({ qualId, rs });

        if (rs.kind == Resolve::Kind::Type)
            return Resolve{ makeAvailable(rs.data.type) };

        return rs;
    }

    schema::Type const* Compiler::requireType(QualIdSpan qualId, schema::Type const* scope) {
        assert(!qualId.empty());

        auto const rs = resolve(qualId, scope);
        if (!rs)
            return nullptr;

        if (rs.kind == Resolve::Kind::Type)
            return rs.data.type;

        log.error(qualId.front().loc, ": does not name a type");
        return nullptr;
    }

    schema::Type const* Compiler::requireType(ast::TypeRef const& ref, schema::Type const* scope) {
        if (auto type = resolveType(ref, scope); type != nullptr)
            return type;

        log.error(ref.loc, ref, ": type not found");
        return nullptr;
    }

    schema::Type const* Compiler::createArrayType(schema::Type const* of, Location const& loc) {
        assert(of != nullptr);

        {
            auto const it = arrayTypeMap.find(of);
            if (it != arrayTypeMap.end())
                return it->second;
        }

        auto& top = state.back();

        auto* arr = static_cast<schema::TypeArray*>(ctx.types.emplace_back(std::make_unique<schema::TypeArray>()).get());
        top.mod->types.push_back(arr);

        arr->name = of->name;
        arr->name += "[]";
        arr->qualifiedName = of->qualifiedName;
        arr->qualifiedName += "[]";
        arr->of = of;
        arr->kind = schema::Type::Array;
        arr->owner = top.mod;
        arr->scope = of->scope;
        arr->location = loc;

        return arrayTypeMap.emplace(of, arr).first->second;
    }

    schema::Type const* Compiler::createPointerType(schema::Type const* to, Location const& loc) {
        assert(to != nullptr);

        {
            auto const it = pointerTypeMap.find(to);
            if (it != pointerTypeMap.end())
                return it->second;
        }

        auto& top = state.back();

        auto* ptr = static_cast<schema::TypePointer*>(ctx.types.emplace_back(std::make_unique<schema::TypePointer>()).get());
        top.mod->types.push_back(ptr);

        ptr->name = to->name;
        ptr->name += "*";
        ptr->qualifiedName = to->qualifiedName;
        ptr->qualifiedName += "*";
        ptr->to = to;
        ptr->kind = schema::Type::Pointer;
        ptr->owner = top.mod;
        ptr->scope = to->scope;
        ptr->location = loc;

        return pointerTypeMap.emplace(to, ptr).first->second;
    }

    schema::Type const* Compiler::createSpecializedType(schema::Type const* gen, std::vector<schema::Type const*> const& typeParams, Location const& loc) {
        assert(gen != nullptr);

        size_t specHash = (std::hash<std::string>{}(gen->qualifiedName));
        for (auto const* param : typeParams)
            specHash = hash_combine(param->qualifiedName, specHash);

        {
            auto const it = specializedTypeMap.find(specHash);
            if (it != specializedTypeMap.end())
                return it->second;
        }

        auto& top = state.back();

        auto* spec = static_cast<schema::TypeSpecialized*>(ctx.types.emplace_back(std::make_unique<schema::TypeSpecialized>()).get());
        top.mod->types.push_back(spec);

        std::string genName = "<";
        for (auto const* param : typeParams)
            genName += param->qualifiedName;
        genName += '>';

        spec->name = gen->name;
        spec->name += genName;
        spec->qualifiedName = gen->qualifiedName;
        spec->qualifiedName += genName;
        spec->ref = gen;
        spec->kind = schema::Type::Specialized;
        spec->owner = top.mod;
        spec->scope = gen->scope;
        spec->location = loc;

        spec->typeParams.reserve(typeParams.size());
        for (auto const* param : typeParams)
            spec->typeParams.push_back(param);

        return specializedTypeMap.emplace(specHash, spec).first->second;
    }

    schema::Value Compiler::translate(ast::Literal const& lit) {
        schema::Value value;
        value.location = lit.loc;
        std::visit([&value, &lit, this](auto const& val) {
            using T = std::remove_cv_t<std::remove_reference_t<decltype(val)>>;
            if constexpr (std::is_same_v<T, ast::QualifiedId>) {
                auto const rs = resolve(val);
                if (rs.kind == Resolve::Kind::Empty)
                    log.error(lit.loc, val, ": not found");
                else if (rs.kind == Resolve::Kind::Type)
                    value.data = rs.data.type;
                else if (rs.kind == Resolve::Kind::EnumItem)
                    value.data = rs.data.enumItem;
                else if (rs.kind == Resolve::Kind::Constant)
                    value = rs.data.constant->value;
                else if (rs.kind == Resolve::Kind::Namespace) {
                    log.error(lit.loc, val, ": names a namespace, must name a type, constant, or enumeration");
                    log.info(rs.data.ns->location, rs.data.ns->name, ": namespace declared here");
                }
                else
                    assert(false && "Unknown resolve result");
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

        result->type = requireType(anno.name);
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

    void Compiler::translate(std::vector<std::unique_ptr<schema::Annotation>>& target, std::vector<ast::Annotation> const& annotations) {
        for (auto const& anno : annotations)
            target.push_back(translate(anno));
    }

    std::string Compiler::qualify(std::string_view name) const {
        assert(!state.empty());
        assert(!state.back().nsStack.empty());

        schema::Namespace const* const scope = state.back().nsStack.back();
        assert(scope != nullptr);

        if (scope->name.empty())
            return std::string{ name };

        std::string qualified = scope->qualifiedName;
        qualified += '.';
        qualified += name;
        return qualified;
    }
}
