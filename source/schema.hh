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
    struct TypeStruct;
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

    struct Member : Annotated {
        std::string name;
        Location location;
        Type const* type = nullptr;
    };

    struct EnumItem : Annotated {
        std::string name;
        Location location;
        long long value = 0;
        TypeEnum const* parent = nullptr;
    };

    struct Type : Annotated {
        enum Kind {
            Primitive,
            Struct,
            Union,
            Attribute,
            Array,
            Pointer,
            Enum,
            Alias,
            TypeId,
        };

        std::string name;
        std::string qualifiedName;
        Location location;
        Module const* owner = nullptr;
        Namespace const* scope = nullptr;
        Kind kind = Kind::Primitive;
    };

    struct TypeStruct : Type {
        Type const* baseType = nullptr;
        std::vector<std::unique_ptr<Field>> fields;
    };

    struct TypeUnion : Type {
        std::vector<std::unique_ptr<Member>> members;
    };

    struct TypeAttribute : Type {
        std::vector<std::unique_ptr<Field>> fields;
    };

    struct TypeArray : Type {
        Type const* of = nullptr;
    };

    struct TypePointer : Type {
        Type const* to = nullptr;
    };

    struct TypeEnum : Type {
        std::vector<std::unique_ptr<EnumItem>> items;
    };

    struct Constant : Annotated {
        std::string name;
        std::string qualifiedName;
        Location location;
        Module const* owner = nullptr;
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
