// sapc - by Sean Middleditch
// This is free and unencumbered software released into the public domain.
// See LICENSE.md for more details.

#include "location.hh"
#include <iostream>

namespace sapc {
    std::ostream& operator<<(std::ostream& os, Location const& loc) {
        os << loc.filename.string();
        if (loc.line > 0 && loc.column > 0)
            os << '(' << loc.line << ',' << loc.column << ')';
        else if (loc.line > 0)
            os << '(' << loc.line << ')';
        return os;
    }
}
