// sapc - by Sean Middleditch
// This is free and unencumbered software released into the public domain.
// See LICENSE.md for more details.

#include "parser.hh"
#include "model.hh"
#include "lexer.hh"
#include "grammar.hh"
#include "analyze.hh"
#include "file_util.hh"

namespace sapc {
    bool Parser::compile(std::filesystem::path filename, Module& out_module) {
        std::string contents;
        if (!loadText(filename, contents)) {
            std::ostringstream buffer;
            buffer << filename.string() << ": failed to open input";
            errors.push_back(buffer.str());
            return false;
        }

        out_module.filename = filename;
        dependencies.push_back(filename);

        search.push_back(filename.parent_path());

        // add built-in types
        static std::string const builtins[] = {
            "string", "bool",
            "i8", "i16", "i32", "i64",
            "u8", "u16", "u32", "u64",
            "f328", "f64"
        };

        for (auto const& builtin : builtins) {
            auto& type = out_module.types.emplace_back();
            type.name = builtin;
            type.module = "$core";
            out_module.typeMap[builtin] = out_module.types.size() - 1;
        }

        tokenize(contents, tokens);
        if (!parse(tokens, search, errors, dependencies, out_module))
            return false;
        return analyze(out_module, errors);
    }
}
