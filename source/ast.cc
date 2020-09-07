// sapc - by Sean Middleditch
// This is free and unencumbered software released into the public domain.
// See LICENSE.md for more details.

#include "ast.hh"
#include <iostream>

namespace sapc::ast {
    std::ostream& operator<<(std::ostream& os, TypeInfo const& type) {
        os << type.type;
        if (type.isPointer)
            os << '*';
        if (type.isArray)
            os << "[]";
        return os;
    }
}
