// sapc - by Sean Middleditch
// This is free and unencumbered software released into the public domain.
// See LICENSE.md for more details.

#pragma once

#include "location.hh"

#include <string>
#include <vector>
#include <unordered_map>
#include <set>
#include <filesystem>
#include <iosfwd>

#include <nlohmann/json.hpp>

namespace sapc {
    struct TypeInfo {
        std::string type;
        bool isArray = false;

        friend std::ostream& operator<<(std::ostream& os, TypeInfo const& type);
    };

    struct Value {
        enum class Type {
            None,
            Null,
            Boolean,
            Number,
            String,
            Enum
        } type = Type::None;
        long long dataNumber;
        std::string dataString;
        Location location;

        friend void to_json(nlohmann::json& j, Value const& value);
    };

    struct Attribute {
        std::string name;
        std::vector<Value> params;
        Location location;
    };

    struct TypeField {
        std::string name;
        TypeInfo type;
        Value init;
        std::vector<Attribute> attributes;
        Location location;
    };

    struct EnumValue {
        std::string name;
        long long value = 0;
        Location location;
    };

    struct Type {
        std::string name;
        std::string base;
        std::string module;
        std::vector<TypeField> fields;
        std::vector<EnumValue> values;
        std::vector<Attribute> attributes;
        bool isAttribute = false;
        bool isEnumeration = false;
        Location location;
    };

    struct Module {
        std::string name;
        std::filesystem::path filename;
        std::vector<Attribute> attributes;

        std::set<std::string> imports;

        std::vector<Type> types;

        std::unordered_map<std::string, size_t> typeMap;

        friend nlohmann::json to_json(Module const& module);
    };
}
