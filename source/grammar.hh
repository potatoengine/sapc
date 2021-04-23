// sapc - by Sean Middleditch
// This is free and unencumbered software released into the public domain.
// See LICENSE.md for more details.

#pragma once

#include "lexer.hh"

#include <memory>
#include <filesystem>
#include <functional>
#include <vector>

namespace sapc {
    struct Log;
    namespace ast {
        struct ModuleUnit;
        struct Identifier;
    }

    using ParserImportModuleCb = std::function<ast::ModuleUnit const* (ast::Identifier const& id, std::filesystem::path const& requestingFile)>;

    std::unique_ptr<ast::ModuleUnit> parse(std::filesystem::path const& filename, ParserImportModuleCb const& importCb, Log& log);
}
