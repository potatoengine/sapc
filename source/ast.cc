// sapc - by Sean Middleditch
// This is free and unencumbered software released into the public domain.
// See LICENSE.md for more details.

#include "ast.hh"
#include <iostream>
#include <cassert>

namespace sapc::ast {
    std::ostream& operator<<(std::ostream& os, TypeInfo const& type) {
        os << type.type;
        if (type.isPointer)
            os << '*';
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

        auto attrs_to_json = [&](std::vector<AttributeUsage> const& attributes) -> nlohmann::json {
            auto attrs_json = json::object();
            for (auto const& attr : attributes) {
                auto const it = module.typeMap.find(attr.name);
                assert(it != module.typeMap.end());
                assert(it->second < module.types.size());

                auto const& def = module.types[it->second];
                assert(def.isAttribute);

                auto args_json = json::object();
                for (size_t index = 0; index != attr.params.size(); ++index) {
                    assert(def.fields.size() == attr.params.size());

                    auto const& param = def.fields[index];
                    auto const& arg = attr.params[index];

                    assert(arg.type != Value::Type::None);
                    args_json[param.name] = json(arg);
                }

                attrs_json[def.name].push_back(std::move(args_json));
            }
            return attrs_json;
        };

        auto modules_json = json::array();
        for (auto const& module : module.imports)
            modules_json.push_back(module);
        doc["imports"] = std::move(modules_json);

        auto pragmas_json = json::array();
        for (auto const& pragma : module.pragmas)
            pragmas_json.push_back(pragma);
        doc["pragmas"] = std::move(pragmas_json);

        auto types_json = json::array();
        for (auto const& type : module.types) {
            auto type_json = json::object();
            type_json["name"] = type.name;
            type_json["imported"] = type.module != module.name;
            type_json["is_builtin"] = type.module == "$core";
            type_json["is_attribute"] = type.isAttribute;
            type_json["is_enumeration"] = type.isEnumeration;
            if (!type.base.empty())
                type_json["base"] = type.base;
            if (!type.attributes.empty())
                type_json["attributes"] = attrs_to_json(type.attributes);

            if (!type.isEnumeration) {
                auto fields = json::array();
                for (auto const& field : type.fields) {
                    auto field_json = json::object();
                    field_json["name"] = field.name;
                    field_json["type"] = field.type.type;
                    field_json["is_array"] = field.type.isArray;
                    field_json["is_pointer"] = field.type.isPointer;
                    if (field.init.type != Value::Type::None)
                        field_json["default"] = json(field.init);
                    if (!field.attributes.empty())
                        field_json["attributes"] = attrs_to_json(field.attributes);
                    fields.push_back(std::move(field_json));
                }
                type_json["fields"] = std::move(fields);
            }

            if (type.isEnumeration) {
                auto values = json::array();
                for (auto const& value : type.values) {
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
}
