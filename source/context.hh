// sapc - by Sean Middleditch
// This is free and unencumbered software released into the public domain.
// See LICENSE.md for more details.

#pragma once

#include "file_util.hh"

#include <memory>
#include <unordered_map>
#include <vector>

namespace sapc {
    namespace ast {
        struct ModuleUnit;
    }

    namespace schema {
        struct Constant;
        struct Module;
        struct Namespace;
        struct Type;
    }

    struct Context {
        std::filesystem::path targetFile;
        std::vector<std::filesystem::path> searchPaths;
        std::vector<std::filesystem::path> dependencies;

        std::vector<std::unique_ptr<ast::ModuleUnit>> asts;
        std::vector<std::unique_ptr<schema::Module>> modules;
        std::vector<std::unique_ptr<schema::Type>> types;
        std::vector<std::unique_ptr<schema::Constant>> constants;
        std::vector<std::unique_ptr<schema::Namespace>> namespaces;

        std::unordered_map<std::filesystem::path, ast::ModuleUnit const*, PathHash> astMap;
        std::unordered_map<std::filesystem::path, schema::Module const*, PathHash> moduleMap;

        schema::Module const* rootModule = nullptr;
    };
}
