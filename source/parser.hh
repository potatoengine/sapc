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

    struct Parser {
        std::vector<Token> tokens;
        std::vector<std::string> errors;
        std::vector<std::filesystem::path> search;
        std::vector<std::filesystem::path> dependencies;

        bool compile(std::filesystem::path filename, Module& out_module);
    };
}
