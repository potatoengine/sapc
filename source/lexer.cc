// sapc - by Sean Middleditch
// This is free and unencumbered software released into the public domain.
// See LICENSE.md for more details.

#include "lexer.hh"

#include <charconv>
#include <sstream>

namespace sapc {
    static const Token unknown = { TokenType::Unknown };

    static auto isIdentStartChar(char ch) -> bool { return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_'; };
    static auto isIdentChar(char ch) -> bool { return isIdentStartChar(ch) || (ch >= '0' && ch <= '9'); };
    static auto isDigit(char ch) -> bool { return ch >= '0' && ch <= '9'; };
    static auto isWhiteSpace(char ch) -> bool { return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n'; }

    std::ostream& operator<<(std::ostream& os, TokenType type) {
        switch (type) {
        case TokenType::Unknown: os << ""; break;
        case TokenType::Identifier: os << "identifier"; break;
        case TokenType::String: os << "string"; break;
        case TokenType::Number: os << "number"; break;
        case TokenType::LeftBrace: os << "{"; break;
        case TokenType::RightBrace: os << "}"; break;
        case TokenType::LeftParen: os << "("; break;
        case TokenType::RightParen: os << ")"; break;
        case TokenType::LeftBracket: os << "["; break;
        case TokenType::RightBracket: os << "]"; break;
        case TokenType::Comma: os << ","; break;
        case TokenType::Equal: os << "="; break;
        case TokenType::Colon: os << ":"; break;
        case TokenType::SemiColon: os << ";"; break;
        case TokenType::Asterisk: os << "*"; break;
        case TokenType::KeywordModule: os << "`module'"; break;
        case TokenType::KeywordImport: os << "`import'"; break;
        case TokenType::KeywordInclude: os << "`include'"; break;
        case TokenType::KeywordPragma: os << "`pragma'"; break;
        case TokenType::KeywordType: os << "`type'"; break;
        case TokenType::KeywordAttribute: os << "`attribute'"; break;
        case TokenType::KeywordEnum: os << "`enum'"; break;
        case TokenType::KeywordTrue: os << "`true'"; break;
        case TokenType::KeywordFalse: os << "`false'"; break;
        case TokenType::KeywordNull: os << "`null'"; break;
        case TokenType::EndOfFile: os << "end of file"; break;
        default: os << "[unknown-token-type]"; break;
        }
        return os;
    }

    bool tokenize(std::string_view source, std::vector<Token>& tokens) {
        decltype(source.size()) position = 0;
        int line = 1;
        int lineStart = 0;

        auto advance = [&position, &line, &lineStart, source](decltype(position) count = 1) {
            while (count-- > 0 && position < source.size()) {
                if (source[position++] == '\n') {
                    ++line;
                    lineStart = position;
                }
            }
        };

        auto const pos = [&position, &line, &lineStart](decltype(position) start) {
            return TokenPos{ line, static_cast<int>(position - lineStart) };
        };

        auto const match = [&position, &source, &advance](std::string_view input) {
            auto const remaining = source.size() - position;
            if (remaining < input.size())
                return false;

            auto const text = source.substr(position, input.size());
            if (text != input)
                return false;

            advance(input.size());
            return true;
        };

        auto const fail = [&](decltype(position) start) {
            tokens.push_back({ TokenType::Unknown, pos(start) });
            return false;
        };

        struct TokenMap {
            std::string_view text;
            TokenType token;
        };
        constexpr TokenMap tokenMap[] = {
            { "{", TokenType::LeftBrace },
            { "}", TokenType::RightBrace },
            { "(", TokenType::LeftParen },
            { ")", TokenType::RightParen },
            { "[", TokenType::LeftBracket },
            { "]", TokenType::RightBracket },
            { ",", TokenType::Comma },
            { "=", TokenType::Equal },
            { ":", TokenType::Colon },
            { ";", TokenType::SemiColon },
            { "*", TokenType::Asterisk },
            { "module", TokenType::KeywordModule },
            { "import", TokenType::KeywordImport },
            { "include", TokenType::KeywordInclude },
            { "pragma", TokenType::KeywordPragma },
            { "attribute", TokenType::KeywordAttribute },
            { "type", TokenType::KeywordType },
            { "enum", TokenType::KeywordEnum },
            { "true", TokenType::KeywordTrue },
            { "false", TokenType::KeywordFalse },
            { "null", TokenType::KeywordNull },
        };

        // parse until end of file
        for (;;) {
            // consume all whitespace
            while (position < source.size() && isWhiteSpace(source[position]))
                advance();

            auto const start = position;

            // if we hit EOF, termine loop
            if (position == source.size()) {
                tokens.push_back({ TokenType::EndOfFile, pos(start) });
                break;
            }

            // check for line-comments
            if (source[position] == '#' || match("//")) {
                bool eol = false;
                while (!eol && position < source.size()) {
                    if (source[position] == '\n')
                        eol = true;
                    advance();
                }
                continue;
            }

            // check for block-comments
            if (match("/*")) {
                bool end = false;
                while (!end && position < source.size()) {
                    if (match("*/"))
                        end = true;
                    advance();
                }
                continue;
            }

            // check static inputs
            bool matched = false;
            for (auto [input, token] : tokenMap) {
                if (match(input)) {
                    tokens.push_back({ token, pos(start) });
                    matched = true;
                    break;
                }
            }
            if (matched)
                continue;

            // see if we had an identifier (keywords will have been matched previously)
            if (isIdentStartChar(source[position])) {
                advance();
                while (position < source.size() && isIdentChar(source[position]))
                    advance();
                tokens.push_back({ TokenType::Identifier, pos(start), 0, std::string{source.substr(start, position - start)} });
                continue;
            }

            // number
            bool const isNegative = source[position] == '-';
            if (isNegative || isDigit(source[position])) {
                advance();
                while (position < source.size() && isDigit(source[position]))
                    advance();

                // only a negative sign is not a complete number
                if (isNegative && position - start <= 1)
                    return fail(start);

                Token token;
                token.type = TokenType::Number;
                token.pos = pos(start);
                std::from_chars(source.data() + start, source.data() + position, token.dataNumber);
                tokens.push_back(token);
                continue;
            }

            // string
            if (source[position] == '"') {
                advance();
                std::ostringstream buf;
                while (position < source.size()) {
                    auto const ch = source[position];
                    advance();

                    if (ch == '"') {
                        break;
                    }
                    else if (ch == '\\') {
                        if (position < source.size()) {
                            auto const esc = source[position];
                            advance();
                            switch (esc) {
                            case 'n':
                                buf << '\n';
                                break;
                            case '\\':
                                buf << '\\';
                                break;
                            default:
                                return fail(start);
                            }
                        }
                        else
                            return fail(start);
                    }
                    else {
                        buf << ch;
                    }
                }

                tokens.push_back({ TokenType::String, pos(start), 0, std::move(buf).str() });
                continue;
            }

            // unknown input
            return fail(start);
        }

        return true;
    }
}
