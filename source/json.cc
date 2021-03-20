// sapc - by Sean Middleditch
// This is free and unencumbered software released into the public domain.
// See LICENSE.md for more details.

#include "json.hh"
#include "schema.hh"

namespace sapc {
    template <typename JsonT>
    void to_json(JsonT& j, Location const& loc);

    namespace schema {
        template <typename JsonT>
        void to_json(JsonT& j, Value const& value);
        template <typename JsonT>
        void to_json(JsonT& j, std::vector<Value> const& value);
        template <typename JsonT>
        void to_json(JsonT& j, Type::Kind value);
        template <typename JsonT>
        void to_json(JsonT& j, Type const& type);
        template <typename JsonT>
        void to_json(JsonT& j, Annotation const& value);
        template <typename JsonT>
        void to_json(JsonT& j, std::vector<std::unique_ptr<Annotation>> const& values);
    }

    template <typename JsonT>
    void to_json(JsonT& j, Location const& loc) {
        j = JsonT::object();
        j["filename"] = loc.filename.string();
        if (loc.start.line > 0)
            j["line"] = loc.start.line;
        if (loc.start.column)
            j["column"] = loc.start.column;
        if (loc.end.line > 0 && loc.end.line != loc.start.line)
            j["lineEnd"] = loc.end.line;
        if (loc.end.line >= loc.start.line && loc.end.column != loc.start.column)
            j["columnEnd"] = loc.end.column;
    };

    template <typename JsonT>
    void schema::to_json(JsonT& j, std::vector<Value> const& value) {
        j = JsonT::array();
        for (Value const& val : value) {
            j.push_back(val);
        }
    }

    template <typename JsonT>
    void schema::to_json(JsonT& j, Value const& value) {
        std::visit([&j](auto const& val) {
            using T = std::remove_cv_t<std::remove_reference_t<decltype(val)>>;
            if constexpr (std::is_same_v<T, schema::Type const*>) {
                assert(val != nullptr);
                j = JsonT::object();
                j["kind"] = "typename";
                j["type"] = val->name;
            }
            else if constexpr (std::is_same_v<T, schema::EnumItem const*>) {
                assert(val != nullptr);
                j = JsonT::object();
                j["kind"] = "enum";
                j["type"] = val->parent->name;
                j["name"] = val->name;
                j["value"] = val->value;
            }
            else if constexpr (std::is_same_v<T, std::vector<schema::Value>>) {
                j = JsonT::array();
                for (auto const& elem : val)
                    j.push_back(elem);
            }
            else {
                j = val;
            }
            }, value.data);
    }

    nlohmann::ordered_json serializeToJson(schema::Module const& mod) {
        using JsonT = nlohmann::ordered_json;

        auto doc = JsonT::object();

        doc["$schema"] = "https://raw.githubusercontent.com/potatoengine/sapc/master/schema/sap-1.schema.json";

        auto mod_json = JsonT::object();
        mod_json["name"] = mod.name;
        mod_json["annotations"] = mod.annotations;
        auto imports_json = JsonT::array();
        for (auto const& imp : mod.imports)
            imports_json.push_back(imp->name);
        mod_json["imports"] = std::move(imports_json);
        doc["module"] = std::move(mod_json);

        auto types_json = JsonT::object();
        auto type_exports_json = JsonT::array();
        for (auto const* type : mod.types) {
            if (type->parent == &mod && type->kind != schema::Type::Kind::Array && type->kind != schema::Type::Kind::Pointer)
                type_exports_json.push_back(type->name);

            types_json[type->name.c_str()] = *type;
        }
        doc["types"] = std::move(types_json);

        auto constants_json = JsonT::object();
        auto constant_exports_json = JsonT::array();
        for (auto const* constant : mod.constants) {
            auto const_json = JsonT::object();
            const_json["name"] = constant->name;
            const_json["module"] = constant->parent->name;
            const_json["type"] = constant->type->name;
            const_json["value"] = constant->value;
            const_json["annotations"] = constant->annotations;
            const_json["location"] = constant->location;
            constants_json[constant->name.c_str()] = std::move(const_json);

            if (constant->parent == &mod)
                constant_exports_json.push_back(constant->name);
        }
        doc["constants"] = std::move(constants_json);

        auto exports = JsonT::object();
        exports["types"] = std::move(type_exports_json);
        exports["constants"] = std::move(constant_exports_json);
        doc["exports"] = std::move(exports);

        return doc;
    }

    template <typename JsonT>
    void schema::to_json(JsonT& j, Type::Kind value) {
        switch (value) {
        case Type::Kind::Primitive: j = "primitive"; break;
        case Type::Kind::Attribute: j = "attribute"; break;
        case Type::Kind::Enum: j = "enum"; break;
        case Type::Kind::Alias: j = "alias"; break;
        case Type::Kind::Struct: j = "struct"; break;
        case Type::Kind::Union: j = "union"; break;
        case Type::Kind::TypeId: j = "typename"; break;
        case Type::Kind::Array: j = "array"; break;
        case Type::Kind::Pointer: j = "pointer"; break;
        default: j = "unknown"; break;
        }
    };

    template <typename JsonT>
    void schema::to_json(JsonT& type_json, Type const& type) {
        assert(type.parent != nullptr);

        type_json = JsonT::object();

        type_json["name"] = type.name;
        type_json["module"] = type.parent->name;
        type_json["kind"] = type.kind;
        type_json["annotations"] = type.annotations;

        if (type.kind == Type::Kind::Enum) {
            auto& typeEnum = static_cast<TypeEnum const&>(type);
            auto names = JsonT::array();
            auto values = JsonT::object();
            for (auto const& item : typeEnum.items) {
                names.push_back(item->name);
                values[item->name.c_str()] = item->value;
            }
            type_json["names"] = std::move(names);
            type_json["values"] = std::move(values);
        }
        else if (type.kind == Type::Kind::Struct) {
            auto& typeStruct = static_cast<TypeStruct const&>(type);

            if (typeStruct.baseType != nullptr)
                type_json["base"] = typeStruct.baseType->name;

            auto fields = JsonT::object();
            auto order = JsonT::array();
            for (auto const& field : typeStruct.fields) {
                auto field_json = JsonT::object();
                field_json["name"] = field->name;
                field_json["type"] = field->type->name;
                if (field->defaultValue)
                    field_json["default"] = *field->defaultValue;
                field_json["annotations"] = field->annotations;
                field_json["location"] = field->location;

                order.push_back(field->name);
                fields[field->name.c_str()] = std::move(field_json);
            }
            type_json["order"] = std::move(order);
            type_json["fields"] = std::move(fields);
        }
        else if (type.kind == Type::Kind::Union) {
            auto& typeUnion = static_cast<TypeUnion const&>(type);

            auto members = JsonT::object();
            auto order = JsonT::array();
            for (auto const& member : typeUnion.members) {
                auto member_json = JsonT::object();
                member_json["name"] = member->name;
                member_json["type"] = member->type->name;
                member_json["annotations"] = member->annotations;
                member_json["location"] = member->location;

                order.push_back(member->name);
                members[member->name.c_str()] = std::move(member_json);
            }
            type_json["order"] = std::move(order);
            type_json["members"] = std::move(members);
        }
        else if (type.kind == Type::Kind::Attribute) {
            auto& typeAttr = static_cast<TypeStruct const&>(type);

            auto fields = JsonT::object();
            auto order = JsonT::array();
            for (auto const& field : typeAttr.fields) {
                auto field_json = JsonT::object();
                field_json["name"] = field->name;
                field_json["type"] = field->type->name;
                if (field->defaultValue)
                    field_json["default"] = *field->defaultValue;
                field_json["annotations"] = field->annotations;
                field_json["location"] = field->location;

                order.push_back(field->name);
                fields[field->name.c_str()] = std::move(field_json);
            }
            type_json["order"] = std::move(order);
            type_json["fields"] = std::move(fields);
        }
        else if (type.kind == Type::Kind::Array) {
            auto& typeArray = static_cast<TypeArray const&>(type);

            type_json["of"] = typeArray.of->name;
        }
        else if (type.kind == Type::Kind::Pointer) {
            auto& typePointer = static_cast<TypePointer const&>(type);

            type_json["to"] = typePointer.to->name;
        }

        type_json["location"] = type.location;
    }

    template <typename JsonT>
    void schema::to_json(JsonT& j, Annotation const& value) {
        j = JsonT::object();

        j["type"] = value.type->name;
        j["location"] = value.location;

        assert(value.type->kind == Type::Kind::Attribute);

        auto const& attr = static_cast<TypeAttribute const&>(*value.type);
        assert(attr.fields.size() == value.args.size());

        auto args_json = JsonT::object();
        for (size_t index = 0; index != value.args.size(); ++index) {
            auto const& param = *attr.fields[index];
            auto const& arg = value.args[index];

            args_json[param.name.c_str()] = arg;
        }

        j["args"] = std::move(args_json);
    }

    template <typename JsonT>
    void schema::to_json(JsonT& j, std::vector<std::unique_ptr<Annotation>> const& values) {
        j = JsonT::array();
        for (auto const& val : values)
            j.push_back(*val);
    }
}
