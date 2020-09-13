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

    void to_json(nlohmann::json& j, Value const& value) {
        switch (value.type) {
        case Value::Type::String: j = value.dataString; break;
        case Value::Type::Number: j = value.dataNumber; break;
        case Value::Type::Boolean: j = value.dataNumber ? true : false; break;
        case Value::Type::Null: j = nullptr; break;
        default: break;
        }
    }

    nlohmann::json to_json(Module const& module) {
        using namespace nlohmann;

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
                assert(def.category == Type::Category::Attribute);

                auto args_json = json::object();
                for (size_t index = 0; index != annotation.arguments.size(); ++index) {
                    assert(def.fields.size() == annotation.arguments.size());

                    auto const& param = def.fields[index];
                    auto const& arg = annotation.arguments[index];

                    assert(arg.type != Value::Type::None);
                    args_json[param.name] = json(arg);
                }

                annotations_json[def.name].push_back(std::move(args_json));
            }
            return annotations_json;
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

        doc["attributes"] = annotations_to_json(module.annotations);

        auto modules_json = json::array();
        for (auto const& module : module.imports)
            modules_json.push_back(module);
        doc["imports"] = std::move(modules_json);

        auto types_json = json::array();
        for (auto const& type : module.types) {
            auto type_json = json::object();
            type_json["name"] = type.name;
            type_json["imported"] = type.module != module.name;
            type_json["is_builtin"] = type.module == "$core";
            type_json["is_attribute"] = type.category == Type::Category::Attribute;
            type_json["is_enumeration"] = type.category == Type::Category::Enum;
            type_json["is_union"] = type.category == Type::Category::Union;
            if (!type.base.empty())
                type_json["base"] = type.base;
            if (!type.annotations.empty())
                type_json["attributes"] = annotations_to_json(type.annotations);
            type_json["location"] = loc_to_json(type.location);

            if (type.category == Type::Category::Enum) {
                auto values = json::array();
                for (auto const& value : type.fields) {
                    auto value_json = json::object();
                    value_json["name"] = value.name;
                    value_json["value"] = value.init.dataNumber;
                    values.push_back(std::move(value_json));
                }
                type_json["values"] = std::move(values);
            }
            else if (type.category == Type::Category::Union) {
                auto types = json::array();
                for (auto const& unionType : type.fields) {
                    auto union_type_json = json::object();
                    union_type_json["name"] = unionType.type.type;
                    union_type_json["is_array"] = unionType.type.isArray;
                    types.push_back(std::move(union_type_json));
                }
                type_json["types"] = std::move(types);
            }
            else {
                auto fields = json::array();
                for (auto const& field : type.fields) {
                    auto field_json = json::object();
                    field_json["name"] = field.name;
                    field_json["type"] = field.type.type;
                    field_json["is_array"] = field.type.isArray;
                    if (field.init.type != Value::Type::None)
                        field_json["default"] = json(field.init);
                    if (!field.annotations.empty())
                        field_json["attributes"] = annotations_to_json(field.annotations);
                    field_json["location"] = loc_to_json(field.location);
                    fields.push_back(std::move(field_json));
                }
                type_json["fields"] = std::move(fields);
            }

            types_json.push_back(std::move(type_json));
        }
        doc["types"] = std::move(types_json);

        return doc;
    }
}
