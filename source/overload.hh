// sapc - by Sean Middleditch
// This is free and unencumbered software released into the public domain.
// See LICENSE.md for more details.

#pragma once

namespace sapc {
    using namespace std::literals;

    template<class... T> struct overload : T... { using T::operator()...; };
    template<class... T> overload(T...)->overload<T...>;
}
