// sapc - by Sean Middleditch
// This is free and unencumbered software released into the public domain.
// See LICENSE.md for more details.

#include "ast.hh"

namespace sapc::ast {
    std::ostream& operator<<(std::ostream& os, Identifier const& id) {
        return os << id.id;
    }

    std::ostream& operator<<(std::ostream& os, QualifiedId const& qualId) {
        if (qualId.components.empty())
            return os;

        for (auto st = begin(qualId.components), it = st, en = end(qualId.components); it != en; ++it) {
            if (it != st)
                os << '.';
            os << *it;
        }

        return os;
    }

    std::ostream& operator<<(std::ostream& os, TypeRef const& ref) {
        switch (ref.kind) {
        case ast::TypeRef::Kind::Name:
            return os << ref.name;
        case ast::TypeRef::Kind::Array:
            return os << *ref.ref << "[]";
        case ast::TypeRef::Kind::Pointer:
            return os << *ref.ref << '*';
        default:
            return os << "???";
        }
    }
}
