// sapc - by Sean Middleditch
// This is free and unencumbered software released into the public domain.
// See LICENSE.md for more details.

#pragma once

#include "lexer.hh"

#include <memory>
#include <filesystem>
#include <vector>

namespace sapc {
    struct Log;
    namespace ast {
        struct ModuleUnit;
    }

    std::unique_ptr<ast::ModuleUnit> parse(std::filesystem::path const& filename, Log& log);
}
