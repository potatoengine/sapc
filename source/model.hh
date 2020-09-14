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
        std::string dataName;
        std::string dataString;
        Location location;
    };

    struct Annotation {
        std::string name;
        std::vector<Value> arguments;
        Location location;
    };

    struct TypeField {
        std::string name;
        TypeInfo type;
        Value init;
        std::vector<Annotation> annotations;
        Location location;
    };

    struct Type {
        enum class Kind {
            Unknown,
            Struct,
            Enum,
            Union,
            Attribute,
            Opaque
        };

        Kind kind = Kind::Unknown;
        std::string name;
        std::string base;
        std::string module;
        std::vector<Annotation> annotations;
        std::vector<TypeField> fields;
        Location location;
    };

    struct Module {
        std::string name;
        std::filesystem::path filename;
        std::vector<Annotation> annotations;

        std::set<std::string> imports;

        std::vector<Type> types;

        std::unordered_map<std::string, size_t> typeMap;

        friend nlohmann::ordered_json to_json(Module const& module);
    };
}
