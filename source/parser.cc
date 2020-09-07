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

    static std::ostream& operator<<(std::ostream& os, TokenType type) {
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

    static std::ostream& operator<<(std::ostream& os, Token const& tok) {
        return os << tok.type;
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

    bool Parser::parse(std::filesystem::path filename) {
        size_t next = 0;

        auto consume = [&, this](TokenType type) {
            if (next >= tokens.size())
                return false;

            if (tokens[next].type != type)
                return false;

            ++next;
            return true;
        };

        auto fail = [&, this](std::string message) {
            auto const& tok = next < tokens.size() ? tokens[next] : tokens.back();
            errors.push_back({ message, tok.pos });
            return false;
        };

#if defined(expect)
#   undef expect
#endif
#define expect(type) \
        do{ \
            auto const _ttype = (type); \
            if (!consume(_ttype)) { \
                std::ostringstream buf; \
                buf << "expected " << _ttype; \
                if (next < tokens.size()) \
                    buf << ", got " << tokens[next]; \
                return fail(buf.str()); \
            } \
        }while(false)

        auto consumeValue = [&]() {
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

        auto parseAttributes = [&, this]() -> bool {
            while (consume(TokenType::LeftBracket)) {
                do {
                    expect(TokenType::Identifier);
                    if (consume(TokenType::LeftParen)) {
                        bool first = true;
                        while (!consume(TokenType::RightParen)) {
                            if (!first)
                                expect(TokenType::Comma);
                            first = false;
                            if (!consumeValue())
                                fail("expected value");
                        }
                    }
                } while (consume(TokenType::Comma));
                expect(TokenType::RightBracket);
            }
            return true;
        };

        auto parseType = [&, this]() -> bool {
            expect(TokenType::Identifier);
            consume(TokenType::Asterisk);
            if (consume(TokenType::LeftBracket)) {
                expect(TokenType::RightBracket);
            }
            return true;
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
        expect(TokenType::KeywordModule);
        expect(TokenType::Identifier);
        expect(TokenType::SemiColon);

        while (!consume(TokenType::EndOfFile)) {
            if (consume(TokenType::Unknown))
                return fail("unexpected input");

            if (consume(TokenType::KeywordImport)) {
                expect(TokenType::Identifier);
                expect(TokenType::SemiColon);
                continue;
            }

            if (consume(TokenType::KeywordInclude)) {
                expect(TokenType::String);
                continue;
            }

            if (consume(TokenType::KeywordPragma)) {
                expect(TokenType::Identifier);
                expect(TokenType::SemiColon);
                continue;
            }

            if (consume(TokenType::KeywordAttribute)) {
                if (!consume(TokenType::Identifier))
                    return fail("expected identifier after `attribute'");
                if (consume(TokenType::LeftBrace)) {
                    while (!consume(TokenType::RightBrace)) {
                        if (consume(TokenType::Identifier)) {
                            expect(TokenType::Identifier);
                            if (consume(TokenType::Equal)) {
                                if (!consumeValue())
                                    return fail("expected value after `='");
                            }
                            expect(TokenType::SemiColon);
                        }
                        else
                            return fail("unexpected token");
                    }
                }
                else
                    expect(TokenType::SemiColon);

                continue;
            }

            // optionally build up a list of attributes
            if (!parseAttributes())
                return false;

            // parse regular type declarations
            if (consume(TokenType::KeywordType)) {
                expect(TokenType::Identifier);
                if (consume(TokenType::Colon))
                    expect(TokenType::Identifier);
                if (consume(TokenType::LeftBrace)) {
                    while (!consume(TokenType::RightBrace)) {
                        if (!parseAttributes())
                            return false;
                        if (!parseType())
                            return false;
                        expect(TokenType::Identifier);
                        if (consume(TokenType::Equal)) {
                            if (!consumeValue())
                                return fail("expected value after `='");
                        }
                        expect(TokenType::SemiColon);
                    }
                }
                else
                    expect(TokenType::SemiColon);

                continue;
            }

            // parse enumerations
            if (consume(TokenType::KeywordEnum)) {
                expect(TokenType::Identifier);
                if (consume(TokenType::Colon))
                    expect(TokenType::Identifier);
                expect(TokenType::LeftBrace);
                for (;;) {
                    expect(TokenType::Identifier);
                    if (consume(TokenType::Equal))
                        expect(TokenType::Number);
                    if (!consume(TokenType::Comma))
                        break;
                }
                expect(TokenType::RightBrace);

                continue;
            }

            std::ostringstream buf;
            buf << "unexpected " << tokens[next];
            return fail(buf.str());
        }

        return true;
    }

#undef expect
}
