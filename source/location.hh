// sapc - by Sean Middleditch
// This is free and unencumbered software released into the public domain.
// See LICENSE.md for more details.

#pragma once

#include <filesystem>
#include <iosfwd>

namespace sapc {
    struct Location {
        std::filesystem::path filename;
        int line = 0;
        int column = -1;

        bool operator==(Location const& rhs) const { return filename == rhs.filename && line == rhs.line && column == rhs.column; }
        friend std::ostream& operator<<(std::ostream& os, Location const& loc);
    };
}
