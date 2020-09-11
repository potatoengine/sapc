// sapc - by Sean Middleditch
// This is free and unencumbered software released into the public domain.
// See LICENSE.md for more details.

#pragma once

#include <vector>
#include <string>
#include <filesystem>

namespace sapc {
    struct Module;

    bool compile(std::filesystem::path filename, std::vector<std::filesystem::path> const& search, std::vector<std::string>& errors, std::vector<std::filesystem::path>& dependencies, Module& out_module);
    void addCoreTypes(Module& module);
}
