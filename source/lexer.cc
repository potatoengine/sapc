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
        case TokenType::Unknown: os << "unknown"; break;
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
        case TokenType::Dot: os << "."; break;
        case TokenType::Equal: os << "="; break;
        case TokenType::Colon: os << ":"; break;
        case TokenType::SemiColon: os << ";"; break;
        case TokenType::Asterisk: os << "*"; break;
        case TokenType::KeywordModule: os << "`module'"; break;
        case TokenType::KeywordImport: os << "`import'"; break;
        case TokenType::KeywordStruct: os << "`struct'"; break;
        case TokenType::KeywordUnion: os << "`union'"; break;
        case TokenType::KeywordAttribute: os << "`attribute'"; break;
        case TokenType::KeywordTypename: os << "`typename'"; break;
        case TokenType::KeywordConst: os << "`const'"; break;
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
        decltype(position) lineStart = 0;

        auto advance = [&, source](decltype(position) count = 1) {
            while (count-- > 0 && position < source.size()) {
                if (source[position++] == '\n') {
                    ++line;
                    lineStart = position;
                }
            }
        };

        auto const pos = [&](decltype(position) start) {
            return TokenPos{ line, static_cast<int>(start - lineStart) + 1 };
        };

        auto const match = [&](std::string_view input, bool only = false) {
            auto const remaining = source.size() - position;
            if (remaining < input.size())
                return false;

            auto const text = source.substr(position, input.size());
            if (text != input)
                return false;

            if (only && source.size() >= position + input.size() && isIdentChar(source[position + input.size()]))
                return false;

            advance(input.size());
            return true;
        };

        auto const error = [&](decltype(position) start) {
            tokens.push_back({ TokenType::Unknown, pos(start) });
            return false;
        };

        struct TokenMap {
            std::string_view text;
            TokenType token;
            bool keyword = false;
        };
        constexpr TokenMap tokenMap[] = {
            { "{", TokenType::LeftBrace },
            { "}", TokenType::RightBrace },
            { "(", TokenType::LeftParen },
            { ")", TokenType::RightParen },
            { "[", TokenType::LeftBracket },
            { "]", TokenType::RightBracket },
            { ",", TokenType::Comma },
            { ".", TokenType::Dot },
            { "=", TokenType::Equal },
            { ":", TokenType::Colon },
            { ";", TokenType::SemiColon },
            { "*", TokenType::Asterisk },
            { "module", TokenType::KeywordModule, true },
            { "import", TokenType::KeywordImport, true },
            { "attribute", TokenType::KeywordAttribute, true },
            { "typename", TokenType::KeywordTypename, true },
            { "const", TokenType::KeywordConst, true },
            { "struct", TokenType::KeywordStruct, true },
            { "union", TokenType::KeywordUnion, true },
            { "enum", TokenType::KeywordEnum, true },
            { "true", TokenType::KeywordTrue, true },
            { "false", TokenType::KeywordFalse, true },
            { "null", TokenType::KeywordNull, true },
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
            for (auto [input, token, keyword] : tokenMap) {
                if (match(input, keyword)) {
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
                auto const identifier = source.substr(start, position - start);
                tokens.push_back({ TokenType::Identifier, pos(start), 0, std::string{identifier} });
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
                    return error(start);

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
                                return error(start);
                            }
                        }
                        else
                            return error(start);
                    }
                    else {
                        buf << ch;
                    }
                }

                tokens.push_back({ TokenType::String, pos(start), 0, std::move(buf).str() });
                continue;
            }

            // unknown input
            return error(start);
        }

        return true;
    }
}
