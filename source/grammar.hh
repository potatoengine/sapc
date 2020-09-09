// sapc - by Sean Middleditch
// This is free and unencumbered software released into the public domain.
// See LICENSE.md for more details.

#pragma once

#include "lexer.hh"

#include <vector>
#include <string>
#include <filesystem>

namespace sapc {
    class Module;

    bool parse(std::vector<Token> const& tokens, std::vector<std::filesystem::path> const& search, std::vector<std::string>& errors, std::vector<std::filesystem::path>& dependencies, Module& module);
}
