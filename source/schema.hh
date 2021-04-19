// sapc - by Sean Middleditch
// This is free and unencumbered software released into the public domain.
// See LICENSE.md for more details.

#pragma once

#include "location.hh"

#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <variant>

namespace sapc::schema {
    struct Module;
    struct Type;
    struct TypeAggregate;
    struct TypeGeneric;
    struct TypeEnum;
    struct EnumItem;
    struct Annotation;
    struct Constant;
    struct Namespace;

    struct Annotated {
        std::vector<std::unique_ptr<Annotation>> annotations;
    };

    struct Value {
        Location location;
        std::variant<
            std::nullptr_t,
            bool,
            long long,
            std::string,
            Type const*,
            EnumItem const*,
            std::vector<Value>> data;
    };

    struct Annotation {
        Type const* type = nullptr;
        Location location;
        std::vector<Value> args;
    };

    struct Field : Annotated {
        std::string name;
        Location location;
        Type const* type = nullptr;
        std::optional<Value> defaultValue;
    };

    struct EnumItem : Annotated {
        std::string name;
        Location location;
        long long value = 0;
        TypeEnum const* parent = nullptr;
    };

    struct Type : Annotated {
        enum Kind {
            Simple,
            Struct,
            Generic,
            Specialized,
            Union,
            Attribute,
            Array,
            Pointer,
            Enum,
            Alias,
            TypeId,
        };

        Kind kind = Kind::Simple;
        std::string name;
        std::string qualifiedName;
        Location location;
        Namespace const* scope = nullptr;
        Type const* refType = nullptr; // arrays, pointers, aliases, specialized
        std::vector<Type const*> generics; // placeholders for generic types; type params for specialized types
    };

    struct TypeAggregate : Type {
        std::vector<std::unique_ptr<Field>> fields;
    };

    struct TypeEnum : Type {
        std::vector<std::unique_ptr<EnumItem>> items;
    };

    struct Constant : Annotated {
        std::string name;
        std::string qualifiedName;
        Location location;
        Namespace const* scope = nullptr;
        Type const* type = nullptr;
        Value value;
    };

    struct Namespace {
        std::string name;
        std::string qualifiedName;
        Location location;
        Module const* owner = nullptr;
        Namespace const* scope = nullptr;
        std::vector<Type const*> types;
        std::vector<Constant const*> constants;
        std::vector<Namespace const*> namespaces;
    };

    struct Module : Annotated {
        std::string name;
        Location location;
        Namespace const* root = nullptr;
        std::vector<Module const*> imports;
        std::vector<Type const*> types;
        std::vector<Constant const*> constants;
        std::vector<Namespace const*> namespaces;
    };

    struct Schema {
        std::vector<std::unique_ptr<Module>> modules;
        std::vector<std::unique_ptr<Type>> types;
        std::vector<std::unique_ptr<Constant>> constants;
        std::vector<std::unique_ptr<Namespace>> namespaces;
        Module const* root = nullptr;
    };
}
