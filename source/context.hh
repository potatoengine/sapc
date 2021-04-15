// sapc - by Sean Middleditch
// This is free and unencumbered software released into the public domain.
// See LICENSE.md for more details.

#pragma once

#include "schema.hh"

#include <memory>
#include <vector>

namespace sapc {
    struct Context {
        std::filesystem::path targetFile;
        std::vector<std::filesystem::path> searchPaths;
        std::vector<std::filesystem::path> dependencies;

        std::vector<std::unique_ptr<schema::Module>> modules;
        std::vector<std::unique_ptr<schema::Type>> types;
        std::vector<std::unique_ptr<schema::Constant>> constants;
        std::vector<std::unique_ptr<schema::Namespace>> namespaces;

        schema::Module const* rootModule = nullptr;
    };
}
