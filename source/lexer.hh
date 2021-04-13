// sapc - by Sean Middleditch
// This is free and unencumbered software released into the public domain.
// See LICENSE.md for more details.

#pragma once

#include "location.hh"

#include <filesystem>
#include <iosfwd>
#include <string>
#include <vector>

namespace sapc {
    struct Log;

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
        KeywordNamespace,
        KeywordUsing,
        KeywordClass,
        KeywordAbstract,
        KeywordFinal,
        KeywordUse,
        EndOfFile
    };
    std::ostream& operator<<(std::ostream& os, TokenType type);

    struct Token {
        TokenType type = TokenType::Unknown;
        Position pos;
        long long dataNumber = 0;
        std::string dataString;

        constexpr operator bool() const noexcept { return type != TokenType::Unknown; }
        friend std::ostream& operator<<(std::ostream& os, Token const& tok) { return os << tok.type; }
    };

    bool tokenize(std::string_view source, std::filesystem::path const& filename, std::vector<Token>& tokens, Log& log);
}
