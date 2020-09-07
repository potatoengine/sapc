// sapc - by Sean Middleditch
// This is free and unencumbered software released into the public domain.
// See LICENSE.md for more details.

#pragma once

#include "location.hh"

#include <string>
#include <vector>
#include <filesystem>

namespace sapc::ast {
    struct TypeInfo {
        std::string type;
        bool isPointer = false;
        bool isArray = false;
    };

    struct Value {
        enum class Type {
            Null,
            Boolean,
            Number,
            String,
            Enum
        } type = Type::Null;
        long long dataNumber;
        std::string dataString;
    };

    struct AttributeArgument {
        std::string name;
        TypeInfo type;
        Value init;
        Location location;
    };

    struct Attribute {
        std::string name;
        std::vector<AttributeArgument> arguments;
        Location location;
    };

    struct AttributeUsage {
        std::string name;
        std::vector<Value> params;
        Location location;
    };

    struct TypeField {
        std::string name;
        TypeInfo type;
        Value init;
        std::vector<AttributeUsage> attributes;
        Location location;
    };

    struct Type {
        std::string name;
        std::string base;
        std::vector<TypeField> fields;
        std::vector<AttributeUsage> attributes;
        Location location;
    };

    struct EnumValue {
        std::string name;
        long long value = 0;
    };

    struct Enum {
        std::string name;
        std::string base;
        std::vector<EnumValue> values;
        std::vector<AttributeUsage> attributes;
        Location location;
    };

    struct Module {
        std::string name;

        std::vector<std::string> imports;
        std::vector<std::string> includes;
        std::vector<std::string> pragmas;

        std::vector<Attribute> attributes;
        std::vector<Type> types;
        std::vector<Enum> enums;
    };
}
