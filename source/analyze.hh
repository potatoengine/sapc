// sapc - by Sean Middleditch
// This is free and unencumbered software released into the public domain.
// See LICENSE.md for more details.

#pragma once

#include <vector>
#include <string>

namespace sapc {
    class Module;

    bool analyze(Module& module, std::vector<std::string>& errors);
}
