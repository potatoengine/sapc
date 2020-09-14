// sapc - by Sean Middleditch
// This is free and unencumbered software released into the public domain.
// See LICENSE.md for more details.

#include "model.hh"
#include <iostream>
#include <cassert>

namespace sapc {
    std::ostream& operator<<(std::ostream& os, TypeInfo const& type) {
        os << type.type;
        if (type.isArray)
            os << "[]";
        return os;
    }

    template <typename JsonT>
    void to_json(JsonT& j, Value const& value) {
        switch (value.type) {
        case Value::Type::String: j = value.dataString; break;
        case Value::Type::Number: j = value.dataNumber; break;
        case Value::Type::Boolean: j = value.dataNumber ? true : false; break;
        case Value::Type::Null: j = nullptr; break;
        case Value::Type::Enum: j = JsonT{
            { "kind", "enum" },
            { "type", value.dataString.c_str() },
            { "name", value.dataName.c_str() },
            { "value", value.dataNumber }
        }; break;
        default: break;
        }
    }

    nlohmann::ordered_json to_json(Module const& module) {
        using json = nlohmann::ordered_json;

        auto doc = json::object();
        doc["$schema"] = "https://raw.githubusercontent.com/potatoengine/sapc/master/schema/sap-1.schema.json";
        doc["module"] = module.name;

        auto annotations_to_json = [&](std::vector<Annotation> const& annotations) -> nlohmann::json {
            auto annotations_json = json::object();
            for (auto const& annotation : annotations) {
                auto const it = module.typeMap.find(annotation.name);
                assert(it != module.typeMap.end());
                assert(it->second < module.types.size());

                auto const& def = module.types[it->second];
                assert(def.kind == Type::Kind::Attribute);

                auto args_json = json::object();
                for (size_t index = 0; index != annotation.arguments.size(); ++index) {
                    assert(def.fields.size() == annotation.arguments.size());

                    auto const& param = def.fields[index];
                    auto const& arg = annotation.arguments[index];

                    assert(arg.type != Value::Type::None);
                    args_json[param.name.c_str()] = json(arg);
                }

                annotations_json[def.name.c_str()] = std::move(args_json);
            }
            return annotations_json;
        };

        auto typeinfo_to_json = [&](TypeInfo const& type) -> json {
            if (!type.isArray)
                return type.type;

            auto type_json = json::object();
            type_json["kind"] = "array";
            type_json["of"] = type.type;
            return type_json;
        };

        auto loc_to_json = [&](Location const& loc) {
            auto loc_json = json::object();
            loc_json["filename"] = loc.filename.string();
            if (loc.line > 0)
                loc_json["line"] = loc.line;
            if (loc.column > 0)
                loc_json["column"] = loc.column;
            return loc_json;
        };

        auto cat_name = [&](Type const& type) {
            switch (type.kind) {
            case Type::Kind::Attribute: return "attribute";
            case Type::Kind::Enum: return "enum";
            case Type::Kind::Opaque: return "opaque";
            case Type::Kind::Struct: return "struct";
            case Type::Kind::Union: return "union";
            default: return "unknown";
            }
        };

        doc["annotations"] = annotations_to_json(module.annotations);

        auto modules_json = json::array();
        for (auto const& module : module.imports)
            modules_json.push_back(module);
        doc["imports"] = std::move(modules_json);

        auto types_json = json::object();
        auto exports_json = json::array();
        for (auto const& type : module.types) {
            if (type.module == module.name)
                exports_json.push_back(type.name);

            auto type_json = json::object();
            type_json["name"] = type.name;
            type_json["module"] = type.module;
            type_json["kind"] = cat_name(type);
            if (!type.base.empty())
                type_json["base"] = type.base;
            type_json["annotations"] = annotations_to_json(type.annotations);

            if (type.kind == Type::Kind::Enum) {
                auto names = json::array();
                auto values = json::object();
                for (auto const& value : type.fields) {
                    names.push_back(value.name);
                    values[value.name.c_str()] = value.init.dataNumber;
                }
                type_json["names"] = std::move(names);
                type_json["values"] = std::move(values);
            }
            else {
                auto fields = json::object();
                auto order = json::array();
                for (auto const& field : type.fields) {
                    auto field_json = json::object();
                    field_json["name"] = field.name;
                    field_json["type"] = typeinfo_to_json(field.type);
                    if (field.init.type != Value::Type::None)
                        field_json["default"] = json(field.init);
                    field_json["annotations"] = annotations_to_json(field.annotations);
                    field_json["location"] = loc_to_json(field.location);

                    order.push_back(field.name);
                    fields[field.name.c_str()] = std::move(field_json);
                }
                type_json["order"] = std::move(order);
                type_json["fields"] = std::move(fields);
            }

            type_json["location"] = loc_to_json(type.location);
            types_json[type.name.c_str()] = std::move(type_json);
        }
        doc["exports"] = std::move(exports_json);
        doc["types"] = std::move(types_json);

        return doc;
    }
}
