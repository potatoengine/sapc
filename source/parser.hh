// sapc - by Sean Middleditch
// This is free and unencumbered software released into the public domain.
// See LICENSE.md for more details.

#pragma once

#include <vector>
#include <string>
#include <filesystem>

namespace sapc {
    class Module;

    enum class TokenType {
        Unknown,
        Identifier,
        String,
        Number,
        LeftBrace,
        RightBrace,
        LeftParen,
        RightParen,
        LeftBracket,
        RightBracket,
        Comma,
        Equal,
        Colon,
        SemiColon,
        Asterisk,
        KeywordModule,
        KeywordImport,
        KeywordInclude,
        KeywordPragma,
        KeywordType,
        KeywordAttribute,
        KeywordEnum,
        KeywordTrue,
        KeywordFalse,
        KeywordNull,
        EndOfFile
    };

    struct TokenPos {
        int line = 1;
        int column = 0;
    };

    struct Token {
        TokenType type = TokenType::Unknown;
        TokenPos pos;
        long long dataNumber = 0;
        std::string dataString;

        constexpr operator bool() const noexcept { return type != TokenType::Unknown; }
    };

    struct Parser {
        std::vector<Token> tokens;
        std::vector<std::string> errors;
        std::vector<std::filesystem::path> search;
        std::vector<std::filesystem::path> dependencies;

        bool compile(std::filesystem::path filename, Module& out_module);

        bool tokenize(std::string_view source);
        bool parse(Module& module);
        bool analyze(Module& module);
    };
}
