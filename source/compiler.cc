// sapc - by Sean Middleditch
// This is free and unencumbered software released into the public domain.
// See LICENSE.md for more details.

#include "compiler.hh"
#include "model.hh"
#include "lexer.hh"
#include "grammar.hh"
#include "analyze.hh"
#include "file_util.hh"

namespace sapc {
    bool compile(std::filesystem::path filename, std::vector<std::filesystem::path> const& search, std::vector<std::string>& errors, std::vector<std::filesystem::path>& dependencies, Module& out_module) {
        std::string contents;
        if (!loadText(filename, contents)) {
            std::ostringstream buffer;
            buffer << filename.string() << ": failed to open input";
            errors.push_back(buffer.str());
            return false;
        }

        out_module.filename = filename;
        dependencies.push_back(filename);

        addCoreTypes(out_module);

        std::vector<Token> tokens;
        tokenize(contents, tokens);
        if (!parse(tokens, search, errors, dependencies, filename, out_module))
            return false;
        return analyze(out_module, errors);
    }

    void addCoreTypes(Module& module) {
        using namespace std::literals;

        // add built-in types
        static constexpr std::string_view builtins[] = {
            "string"sv,
            "bool"sv,
            "byte"sv,
            "int"sv,
            "float"sv,
        };

        for (auto const& builtin : builtins) {
            auto& type = module.types.emplace_back();
            type.kind = Type::Kind::Opaque;
            type.name = builtin;
            type.module = "$core";
            module.typeMap[type.name] = module.types.size() - 1;
        }
    }
}
