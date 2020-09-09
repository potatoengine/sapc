// sapc - by Sean Middleditch
// This is free and unencumbered software released into the public domain.
// See LICENSE.md for more details.

#pragma once

#include <string_view>

namespace sapc {
    constexpr bool starts_with(std::string_view str, std::string_view prefix) noexcept {
        return str.size() >= prefix.size() && str.substr(0, prefix.size()) == prefix;
    }

    constexpr std::string_view trim(std::string_view str) noexcept {
        auto const start = str.find_first_not_of(" \t\n");
        if (start == std::string_view::npos)
            return str;
        auto const end = str.find_last_not_of(" \t\n");
        return str.substr(start, (end + 1) - start);
    }
}
