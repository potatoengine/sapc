// sapc - by Sean Middleditch
// This is free and unencumbered software released into the public domain.
// See LICENSE.md for more details.

#include "location.hh"
#include <iostream>

namespace sapc {
    std::ostream& operator<<(std::ostream& os, Location const& loc) {
        os << loc.filename.string();
        if (loc.start.line > 0 && loc.start.column > 0) {
            os << '(';
            os << loc.start.line << ',' << loc.start.column;
            if (loc.start != loc.end && loc.end.line != 0)
                os << ',' << loc.end.line << ',' << loc.end.column;
            os << ')';
        }
        else if (loc.start.line > 0) {
            os << '(' << loc.start.line << ')';
        }
        return os;
    }

    Location& Location::merge(Position const& rhs) {
        if (rhs.line == 0)
            return *this;

        if (start.line == 0 || rhs.line < start.line)
            start = rhs;
        else if (start.line == rhs.line && rhs.column < start.column)
            start.column = rhs.column;

        if (end.line == 0 || rhs.line > end.line)
            end = rhs;
        else if (end.line == rhs.line && rhs.column > end.column)
            end.column = rhs.column;

        return *this;
    }
}
