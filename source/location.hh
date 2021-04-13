// sapc - by Sean Middleditch
// This is free and unencumbered software released into the public domain.
// See LICENSE.md for more details.

#pragma once

#include <filesystem>
#include <iosfwd>

namespace sapc {
    struct Position {
        int line = 0;
        int column = 0;

        bool operator==(Position const& rhs) const { return line == rhs.line && column == rhs.column; }
        bool operator!=(Position const& rhs) const { return line != rhs.line || column != rhs.column; }
    };

    struct Location {
        std::filesystem::path filename;
        Position start;
        Position end;

        bool operator==(Location const& rhs) const { return filename == rhs.filename && start == rhs.start && end == rhs.end; }
        friend std::ostream& operator<<(std::ostream& os, Location const& loc);

        Location& merge(Location const& rhs) {
            return merge(rhs.start).merge(rhs.end);
        }
        Location& merge(Position const& rhs);
    };
}
