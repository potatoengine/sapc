// sapc - by Sean Middleditch
// This is free and unencumbered software released into the public domain.
// See LICENSE.md for more details.

#pragma once

#include "location.hh"

#include <filesystem>
#include <iosfwd>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace sapc::ast {
    struct Identifier {
        std::string id;
        Location loc;

        bool empty() const noexcept { return id.empty(); }

        friend std::ostream& operator<<(std::ostream& os, Identifier const& id);
    };

    struct Declaration {
        enum class Kind {
            Alias,
            Struct,
            Union,
            Attribute,
            Enum,
            Constant,
            Import,
            Module,
            Namespace,
            CustomTag,
        };

        virtual ~Declaration() = default;

        Kind kind = {};
    };

    struct QualifiedId {
        std::vector<Identifier> components;

        bool empty() const noexcept { return components.empty(); }

        friend std::ostream& operator<<(std::ostream& os, QualifiedId const& qualId);
    };

    struct Literal {
        Location loc;
        std::variant<
            std::nullptr_t,
            bool,
            long long,
            std::string,
            QualifiedId,
            std::vector<Literal>> data;
    };

    struct Annotation {
        QualifiedId name;
        std::vector<Literal> args;
    };

    struct TypeRef {
        enum class Kind {
            Pointer,
            Array,
            Name,
            Generic,
            TypeName,
        };

        Kind kind = Kind::Name;
        Location loc;
        QualifiedId name;
        std::unique_ptr<TypeRef> ref;
        std::vector<std::unique_ptr<TypeRef>> typeArgs;
        std::optional<long long> arraySize; // static array size

        friend std::ostream& operator<<(std::ostream& os, TypeRef const& ref);
    };

    struct Field {
        Identifier name;
        std::unique_ptr<TypeRef> type;
        std::vector<Annotation> annotations;
        std::optional<Literal> init;
    };

    struct CustomTagDecl : Declaration {
        CustomTagDecl() { kind = Kind::CustomTag; }

        Identifier name;
        Declaration::Kind tagKind = Kind::Struct; // Must be a declaration type
        std::vector<Annotation> annotations;
    };

    struct NamespaceDecl : Declaration {
        NamespaceDecl() { kind = Kind::Namespace; }

        Identifier name;
        std::vector<std::unique_ptr<Declaration>> decls;
    };

    struct AliasDecl : Declaration {
        AliasDecl() { kind = Kind::Alias; }

        Identifier name;
        std::string customTag;
        std::unique_ptr<TypeRef> targetType; // optional
        std::vector<Annotation> annotations;
    };

    struct StructDecl : Declaration {
        StructDecl() { kind = Kind::Struct; }

        Identifier name;
        std::string customTag;
        std::unique_ptr<TypeRef> baseType;
        std::vector<Field> fields;
        std::vector<Identifier> typeParams;
        std::vector<Annotation> annotations;
    };

    struct AttributeDecl : Declaration {
        AttributeDecl() { kind = Kind::Attribute; }

        Identifier name;
        std::vector<Field> fields;
        std::vector<Annotation> annotations;
    };

    struct UnionDecl : Declaration {
        UnionDecl() { kind = Kind::Union; }

        Identifier name;
        std::string customTag;
        std::vector<Field> fields;
        std::vector<Identifier> typeParams;
        std::vector<Annotation> annotations;
    };

    struct EnumItem {
        Identifier name;
        std::vector<Annotation> annotations;
        long long value = 0;
    };

    struct EnumDecl : Declaration {
        EnumDecl() { kind = Kind::Enum; }

        Identifier name;
        std::string customTag;
        std::unique_ptr<TypeRef> baseType;
        std::vector<EnumItem> items;
        std::vector<Annotation> annotations;
    };

    struct ConstantDecl : Declaration {
        ConstantDecl() { kind = Kind::Constant; }

        Identifier name;
        std::string customTag;
        std::unique_ptr<TypeRef> type;
        std::vector<Annotation> annotations;
        Literal value;
    };

    struct ImportDecl : Declaration {
        ImportDecl() { kind = Kind::Import; }

        Identifier target;
    };

    struct ModuleDecl : Declaration {
        ModuleDecl() { kind = Kind::Module; }

        Identifier name;
        std::vector<Annotation> annotations;
    };

    struct ModuleUnit {
        Identifier name;
        std::filesystem::path filename;
        std::vector<std::unique_ptr<Declaration>> decls;
    };
}
