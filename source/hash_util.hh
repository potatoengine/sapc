// sapc - by Sean Middleditch
// This is free and unencumbered software released into the public domain.
// See LICENSE.md for more details.

#pragma once

#include <functional>

namespace sapc {
    // https://stackoverflow.com/questions/2590677/how-do-i-combine-hash-values-in-c0x
    template <typename T, typename H = std::hash<T>>
    constexpr auto hash_combine(T const& val, typename H::result_type seed) noexcept -> decltype(seed)
    {
        return seed ^ (H{}(val)) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }
}
