// sapc - by Sean Middleditch
// This is free and unencumbered software released into the public domain.
// See LICENSE.md for more details.

#pragma once

#include <vector>
#include <string>
#include <filesystem>

namespace sapc {
    namespace ast {
        class Module;
    }

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

    struct ParseError {
        std::string message;
        TokenPos pos;
    };

    struct Parser {
        std::vector<Token> tokens;
        std::vector<ParseError> errors;

        bool tokenize(std::string_view source);
        bool parse(std::filesystem::path filename, ast::Module& out_module);
    };
}
