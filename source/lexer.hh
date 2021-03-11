// sapc - by Sean Middleditch
// This is free and unencumbered software released into the public domain.
// See LICENSE.md for more details.

#pragma once

#include <vector>
#include <string>
#include <iosfwd>

namespace sapc {
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
        Dot,
        Equal,
        Colon,
        SemiColon,
        Asterisk,
        KeywordModule,
        KeywordImport,
        KeywordStruct,
        KeywordUnion,
        KeywordAttribute,
        KeywordTypename,
        KeywordConst,
        KeywordEnum,
        KeywordTrue,
        KeywordFalse,
        KeywordNull,
        EndOfFile
    };
    std::ostream& operator<<(std::ostream& os, TokenType type);

    struct TokenPos {
        int line = 0;
        int column = 0;
    };

    struct Token {
        TokenType type = TokenType::Unknown;
        TokenPos pos;
        long long dataNumber = 0;
        std::string dataString;

        constexpr operator bool() const noexcept { return type != TokenType::Unknown; }
        friend std::ostream& operator<<(std::ostream& os, Token const& tok) { return os << tok.type; }
    };

    bool tokenize(std::string_view source, std::vector<Token>& tokens);
}
