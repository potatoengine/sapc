// sapc - by Sean Middleditch
// This is free and unencumbered software released into the public domain.
// See LICENSE.md for more details.

#include "state.hh"
#include "sapc_parse.h"
#include "context.h"
#include "overloaded.hh"

#include <string_view>
#include <iostream>
#include <fstream>
#include <sstream>
#include <nlohmann/json.hpp>
#include <cassert>

namespace {
    struct Jsonify {
        sapc::ParseState const& ast;
        reID id = sapc::reNone;

        friend void to_json(nlohmann::json& j, Jsonify const& value) {
            switch (value.id.type) {
            case reTypeIdentifier:
            case reTypeString: j = value.ast.strings.strings[value.id.value]; break;
            case reTypeNumber: j = value.id.value; break;
            case reTypeBoolean: j = value.id.value ? true : false; break;
            case reTypeNull: j = nullptr; break;
            default: break;
            }
        }
    };
}

namespace sapc {
    std::ostream& operator<<(std::ostream& os, FieldType const& type) {
        os << type.typeName;
        if (type.isPointer)
            os << '*';
        if (type.isArray)
            os << "[]";
        return os;
    }

    std::ostream& operator<<(std::ostream& os, Error const& error) {
        if (error.loc.column >= 0)
            return os << error.loc.filename.string() << '(' << error.loc.line << ',' << error.loc.column << "): **error** " << error.message;
        else
            return os << error.loc.filename.string() << '(' << error.loc.line << "): **error** " << error.message;
    }
}

nlohmann::json sapc::ParseState::to_json() {
    using namespace nlohmann;

    auto doc = json::object();
    doc["$schema"] = "https://raw.githubusercontent.com/potatoengine/sapc/master/schema/sap-1.schema.json";
    doc["module"] = moduleName;

    auto attrs_to_json = [this](std::vector<Attribute> const& attributes) -> nlohmann::json {
        auto attrs_json = json::object();
        for (auto const& attr : attributes) {
            auto const it = typeMap.find(attr.name);
            assert(it != typeMap.end());
            assert(it->second < this->types.size());

            auto const& def = *this->types[it->second];
            assert(def.attribute);

            auto args_json = json::object();
            for (size_t index = 0; index != attr.arguments.size(); ++index) {
                assert(def.fields.size() == attr.arguments.size());

                auto const& param = def.fields[index];

                json value(Jsonify{ *this, attr.arguments[index] });
                args_json[param.name] = std::move(value);
            }

            attrs_json[def.name].push_back(std::move(args_json));
        }
        return attrs_json;
    };

    auto modules_json = json::array();
    for (auto const& module : imports)
        modules_json.push_back(module);
    doc["imports"] = std::move(modules_json);

    auto pragmas_json = json::array();
    for (auto const& pragma : pragmas)
        pragmas_json.push_back(pragma);
    doc["pragmas"] = std::move(pragmas_json);

    auto types_json = json::array();
    for (auto const& type : types) {
        auto type_json = json::object();
        type_json["name"] = type->name;
        type_json["imported"] = type->imported;
        type_json["is_builtin"] = type->builtin;
        type_json["is_attribute"] = type->attribute;
        type_json["is_enumeration"] = type->enumeration;
        if (!type->base.empty())
            type_json["base"] = type->base;
        if (!type->attributes.empty())
            type_json["attributes"] = attrs_to_json(type->attributes);

        if (!type->enumeration) {
            auto fields = json::array();
            for (auto const& field : type->fields) {
                auto field_json = json::object();
                field_json["name"] = field.name;
                field_json["type"] = field.type.typeName;
                field_json["is_array"] = field.type.isArray;
                field_json["is_pointer"] = field.type.isPointer;
                if (field.init != reNone)
                    field_json["default"] = Jsonify{ *this, field.init };
                if (!field.attributes.empty())
                    field_json["attributes"] = attrs_to_json(field.attributes);
                fields.push_back(std::move(field_json));
            }
            type_json["fields"] = std::move(fields);
        }

        if (type->enumeration) {
            auto values = json::array();
            for (auto const& value : type->values) {
                auto value_json = json::object();
                value_json["name"] = value.name;
                value_json["value"] = value.value;
                values.push_back(std::move(value_json));
            }
            type_json["values"] = std::move(values);
        }

        types_json.push_back(std::move(type_json));
    }
    doc["types"] = std::move(types_json);

    return doc;
}

std::filesystem::path sapc::ParseState::resolveInclude(std::filesystem::path include) {
    if (include.is_relative()) {
        for (auto const& path : search) {
            auto tmp = path / include;
            if (std::filesystem::exists(tmp))
                return tmp;
        }
    }

    return include;
}

std::filesystem::path sapc::ParseState::resolveModule(std::string const& name) {
    std::filesystem::path filename{ name };
    filename.replace_extension("sap");

    for (auto const& path : search) {
        auto tmp = path / filename;
        if (std::filesystem::exists(tmp))
            return tmp;
    }

    return filename;
}

void sapc::ParseState::addBuiltinTypes() {
    static std::string const builtins[] = {
        "string", "bool",
        "i8", "i16", "i32", "i64",
        "u8", "u16", "u32", "u64",
        "f328", "f64"
    };

    for (auto const& builtin : builtins) {
        Type type;
        type.name = builtin;
        type.builtin = true;
        typeMap[builtin] = types.size();
        types.push_back(std::make_unique<Type>(std::move(type)));
    }
}

bool sapc::ParseState::compile(std::filesystem::path filename) {
    std::ifstream stream(filename);
    if (!stream) {
        error(location({ 0, 1 }), "failed to open '", filename.string(), '\'');
        return false;
    }

    std::ostringstream buffer;
    buffer << stream.rdbuf();
    stream.close();
    std::string contents = buffer.str();

    return compile(std::move(contents), std::move(filename));
}

bool sapc::ParseState::compile(std::string contents, std::filesystem::path filename) {
    addDependency(filename);
    pathStack.push_back(filename);
    search.push_back(filename.remove_filename());
    bool const result = build(std::move(contents));

    if (!result && errors.empty())
        error(location({ 0, 1 }), "unknown compile error");

    pathStack.pop_back();

    return !failed();
}

void sapc::ParseState::addDependency(std::filesystem::path filename) {
    auto const compare = [&filename](auto const& dep) { return std::filesystem::equivalent(dep, filename); };
    auto const it = find_if(begin(fileDependencies), end(fileDependencies), compare);
    if (it == end(fileDependencies))
        fileDependencies.push_back(std::move(filename));
}

void sapc::ParseState::addImport(std::string module_name) {
    if (find(begin(imports), end(imports), module_name) == end(imports))
        imports.push_back(std::move(module_name));
}

bool sapc::ParseState::build(std::string contents) {
    if (!parse(std::move(contents)))
        return false;

    if (!checkTypes())
        return false;

    if (!expandAttributes())
        return false;

    return true;
}

bool sapc::ParseState::parse(std::string contents) {
    reParseContext parse_ctx;
    parse_ctx.state = static_cast<reParseState*>(this);
    parse_ctx.source = contents.data();
    parse_ctx.length = contents.size();
    parse_ctx.errors = 0;
    parse_ctx.loc.position = 0;
    parse_ctx.loc.line_start = 0;
    parse_ctx.loc.line = 1;

    sapc_context_t* ctx = sapc_create(&parse_ctx);
    int const rs = sapc_parse(ctx, nullptr);
    sapc_destroy(ctx);

    return rs == 0;
}

bool sapc::ParseState::importModule(std::string module, reLoc loc) {
    auto path = resolveModule(module);

    addImport(module);
    addDependency(path);

    std::ifstream stream(path);
    if (!stream) {
        error(loc, "failed to open '", path.string(), '\'');
        return false;
    }

    std::ostringstream buffer;
    buffer << stream.rdbuf();
    stream.close();
    std::string contents = buffer.str();

    sapc::ParseState parser{ strings, search };
    parser.addBuiltinTypes();
    auto const parsed = parser.compile(std::move(contents), path);

    for (auto& error : parser.errors)
        errors.push_back(std::move(error));

    for (auto& path : parser.fileDependencies)
        addDependency(std::move(path));

    for (auto& mod_name : parser.imports)
        addImport(std::move(mod_name));

    if (!parsed)
        error(loc, "errors encountered during import '", path.string(), '\'');

    for (auto& type : parser.types) {
        if (type->builtin)
            continue;

        auto it = typeMap.find(type->name);
        if (it != typeMap.end()) {
            auto const& previous = types[it->second];

            if (previous->loc == type->loc)
                continue;

            error(loc, "imported type '", type->name, "' conflict");
            error(previous->loc, "conflicting definition here");
            continue;
        }

        // types imported to the target module are not available for name lookup in the importer
        if (!type->imported)
            typeMap[type->name] = types.size();

        type->imported = true;
        types.push_back(std::move(type));
    }

    return parsed;
}

bool sapc::ParseState::includeFile(std::filesystem::path filename, reLoc loc) {
    auto path = resolveInclude(filename);

    addDependency(path);

    std::ifstream stream(path);
    if (!stream) {
        error(loc, "could not open file '", path.string(), '\'');
        return false;
    }

    std::ostringstream buffer;
    buffer << stream.rdbuf();
    stream.close();
    std::string text = buffer.str();

    pathStack.push_back(path);
    bool const result = parse(std::move(text));
    pathStack.pop_back();

    if (!result)
        error(loc, "errors encountered during include '", path.string(), '\'');

    return result;
}

bool sapc::ParseState::checkTypes() {
    bool ok = true;

    for (auto const& type : types) {
        if (type->imported)
            continue;

        if (!type->base.empty() && typeMap.find(type->base) == typeMap.end()) {
            error(type->loc, "unknown type '", type->base, '\'');
            ok = false;
        }

        for (auto& field : type->fields) {
            auto typeIt = typeMap.find(field.type.typeName);
            if (typeIt == typeMap.end()) {
                error(field.loc, "unknown type '", field.type, '\'');
                ok = false;
                continue;
            }
            auto const& fieldType = types[typeIt->second];

            if (fieldType->enumeration && field.init.type != reTypeNone) {
                if (field.init.type != reTypeIdentifier) {
                    error(field.loc, "enumeration type '", field.type, "' may only be initialized by enumerants");
                    ok = false;
                    continue;
                }

                auto findEnumerant = [&fieldType](std::string const& name) -> EnumValue const* {
                    for (auto const& value : fieldType->values) {
                        if (value.name == name)
                            return &value;
                    }
                    return nullptr;
                };

                auto const& enumerantName = strings.strings[field.init.value];
                auto const* enumValue = findEnumerant(enumerantName);
                if (findEnumerant(enumerantName) == nullptr) {
                    error(field.loc, "enumerant '", enumerantName, "' not found in enumeration '", fieldType->name, '\'');
                    error(fieldType->loc, "enumeration '", fieldType->name, "' defined here");
                    continue;
                }

                field.init = { reTypeNumber, static_cast<int>(enumValue->value) };
            }
            else if (field.init.type == reTypeIdentifier) {
                error(field.loc, "only enumeration types can be initialized by enumerants");
                ok = false;
                continue;
            }
        }
    }

    return ok;
}

bool sapc::ParseState::expandAttributes() {
    auto expand = [this](Attribute& attr) {
        auto const it = typeMap.find(attr.name);
        if (it == typeMap.end()) {
            error(attr.loc, "unknown attribute '", attr.name, '\'');
            return false;
        }

        auto const argc = attr.arguments.size();
        auto const& def = types[it->second];
        auto const& params = def->fields;

        if (!def->attribute) {
            error(attr.loc, "attribute type '", attr.name, "' is declared as a regular type (not attribute) at ", def->loc.filename.string(), '(', def->loc.line, ')');
            return false;
        }

        attr.type = def.get();

        if (argc > params.size()) {
            error(attr.loc, "too many arguments to attribute '", attr.name, '\'');
            return false;
        }

        bool ok = true;
        for (size_t index = argc; index != params.size(); ++index) {
            auto const& param = params[index];

            if (param.init.type == reTypeNone) {
                error(attr.loc, "missing required argument '", param.name, "' to attribute '", attr.name, '\'');
                ok = false;
            }

            attr.arguments.push_back(param.init);
        }
        return ok;
    };

    bool ok = true;
    for (auto& type : types) {
        for (auto& attr : type->attributes) {
            ok = expand(attr) && ok;
        }

        for (auto& field : type->fields) {
            for (auto& attr : field.attributes)
                ok = expand(attr) && ok;
        }
    }

    return ok;
}
