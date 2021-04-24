// sapc - by Sean Middleditch
// This is free and unencumbered software released into the public domain.
// See LICENSE.md for more details.

#pragma once

#include "location.hh"

#include <ostream>
#include <string>
#include <string_view>
#include <vector>

namespace sapc {
    struct Log {
        std::vector<std::string> lines;
        int countErrors = 0;

        template <typename... T>
        bool error(Location const& loc, T const&... args) {
            std::ostringstream buffer;
            ((buffer << loc << ": error C2000: ") << ... << args);
            lines.push_back(buffer.str());
            ++countErrors;
            return false; // convenience
        }

        template <typename... T>
        void warn(Location const& loc, T const&... args) {
            std::ostringstream buffer;
            ((buffer << loc << ": warning C4000: ") << ... << args);
            lines.push_back(buffer.str());
        }

        template <typename... T>
        void info(Location const& loc, T const&... args) {
            std::ostringstream buffer;
            ((buffer << loc << ": info C4000: ") << ... << args);
            lines.push_back(buffer.str());
        }
    };
}
