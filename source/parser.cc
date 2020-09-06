// sapc - by Sean Middleditch
// This is free and unencumbered software released into the public domain.
// See LICENSE.md for more details.

#include "parser.hh"
#include <charconv>
#include <sstream>
#include <fstream>

namespace sapc {
    static const Token unknown = { TokenType::Unknown };

    static auto isIdentStartChar(char ch) -> bool { return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_'; };
    static auto isIdentChar(char ch) -> bool { return isIdentStartChar(ch) || (ch >= '0' && ch <= '9'); };
    static auto isDigit(char ch) -> bool { return ch >= '0' && ch <= '9'; };
    static auto isWhiteSpace(char ch) -> bool { return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n'; }

    static std::ostream& operator<<(std::ostream& os, Token const& tok) {
        switch (tok.type) {
        case TokenType::Unknown: os << ""; break;
        case TokenType::Identifier: os << "identifier"; break;
        case TokenType::String: os << "string"; break;
        case TokenType::Number: os << "number"; break;
        case TokenType::LeftBrace: os << "{"; break;
        case TokenType::RightBrace: os << "}"; break;
        case TokenType::LeftParen: os << "("; break;
        case TokenType::RightParent: os << ")"; break;
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

    bool Parser::tokenize(std::string_view source) {
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

        auto const fail = [&position, &pos, this](decltype(position) start) {
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
            { ")", TokenType::RightParent },
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
                tokens.push_back({ TokenType::Identifier, pos(start), 0, std::string{source.substr(start)} });
                continue;
            }

            // number
            bool const isNegative = source[position] == '-';
            if (isNegative || isDigit(source[position])) {
                advance();
                while (position < source.size() && isDigit(source[position]))
                    advance();

                // only a negative sign is not a complete number
                if (position - start <= 1)
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

    bool Parser::parse(std::filesystem::path filename) {
        size_t next = 0;
        auto consume = [&next, this](TokenType type) {
            if (next >= tokens.size())
                return false;

            if (tokens[next].type != type)
                return false;

            ++next;
            return true;
        };

        auto fail = [&next, this](std::string_view error) {
            std::ostringstream buf;
            buf << error;
            if (next < tokens.size()) {
                buf << " at " << tokens[next];
                errors.push_back({ std::move(buf).str(), tokens[next].pos });
            }
            else
                errors.push_back({ std::move(buf).str() });
            return false;
        };

        auto consumeValue = [&consume, &fail]() {
            if (consume(TokenType::KeywordFalse) || consume(TokenType::KeywordTrue))
                return true;
            if (consume(TokenType::String))
                return true;
            if (consume(TokenType::Number))
                return true;
            if (consume(TokenType::KeywordNull))
                return true;
            if (consume(TokenType::Identifier))
                return true;
            return false;
        };

        // open file and read contents
        std::ifstream stream(filename);
        if (!stream)
            return fail("failed to open input");

        std::ostringstream buffer;
        buffer << stream.rdbuf();
        stream.close();
        std::string contents = buffer.str();

        tokenize(contents);

        // expect module first
        if (!consume(TokenType::KeywordModule))
            return fail("expected `module' as first declaration");
        if (!consume(TokenType::Identifier))
            return fail("expected identifier after `module'");
        if (!consume(TokenType::SemiColon))
            return fail("expected ; after module identifier");

        while (!consume(TokenType::EndOfFile)) {
            if (consume(TokenType::Unknown))
                return fail("unexpected input");

            if (consume(TokenType::KeywordAttribute)) {
                if (!consume(TokenType::Identifier))
                    return fail("expected identifier after `attribute'");
                if (consume(TokenType::LeftBrace)) {
                    while (!consume(TokenType::RightBrace)) {
                        if (consume(TokenType::Identifier)) {
                            if (!consume(TokenType::Identifier))
                                return fail("expected field name after type");
                            if (consume(TokenType::Equal))
                                if (!consumeValue())
                                    return fail("expected identifier after `='");
                            if (!consume(TokenType::SemiColon))
                                return fail("expected `;'");
                        }
                        else
                            return fail("unexpected token");
                    }
                }
                else if (!consume(TokenType::SemiColon))
                    return fail("expected `{' or `;' after attribute identifier");
            }
            else if (consume(TokenType::KeywordType)) {

            }
            else
                return fail("unexpected token");
        }

        return true;

    }
}
