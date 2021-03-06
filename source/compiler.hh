// sapc - by Sean Middleditch
// This is free and unencumbered software released into the public domain.
// See LICENSE.md for more details.

#pragma once

#include <vector>
#include <string>
#include <filesystem>

namespace sapc {
    struct Log;
    struct Context;

    bool compile(Context& ctx, Log& log);
}
