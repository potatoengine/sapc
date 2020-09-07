// sapc - by Sean Middleditch
// This is free and unencumbered software released into the public domain.
// See LICENSE.md for more details.

#include "parser.hh"
#include "ast.hh"

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

    bool Parser::parse(std::filesystem::path filename, ast::Module& out_module) {
        size_t next = 0;

        out_module = ast::Module{};

        struct Consume {
            size_t& next;
            Parser& self;

            bool operator()(TokenType type) {
                if (next >= self.tokens.size())
                    return false;

                if (self.tokens[next].type != type)
                    return false;

                ++next;
                return true;
            }

            bool operator()(TokenType type, std::string& out) {
                auto const index = next;
                if (!(*this)(type))
                    return false;

                out = self.tokens[index].dataString;
                return true;
            }

            bool operator()(TokenType type, long long& out) {
                auto const index = next;
                if (!(*this)(type))
                    return false;

                out = self.tokens[index].dataNumber;
                return true;
            }
        } consume{ next, *this };

        auto fail = [&, this](std::string message) {
            auto const& tok = next < tokens.size() ? tokens[next] : tokens.back();
            auto const loc = Location{ filename, tok.pos.line, tok.pos.column };
            std::ostringstream buffer;
            buffer << loc << ':' << message;
            errors.push_back({ buffer.str() });
            return false;
        };

        auto pos = [&]() {
            auto const& tokPos = next > 0 ? tokens[next - 1].pos : tokens.front().pos;
            return Location{ filename, tokPos.line, tokPos.column };
        };

#if defined(expect)
#   undef expect
#endif
#define expect(type,...) \
        do{ \
            auto const _ttype = (type); \
            if (!consume(_ttype, __VA_ARGS__)) { \
                std::ostringstream buf; \
                buf << "expected " << _ttype; \
                if (next < tokens.size()) \
                    buf << ", got " << tokens[next]; \
                return fail(buf.str()); \
            } \
        }while(false)

        auto consumeValue = [&](ast::Value& out) {
            if (consume(TokenType::KeywordNull))
                out = ast::Value{ ast::Value::Type::Null };
            else if (consume(TokenType::KeywordFalse))
                out = ast::Value{ ast::Value::Type::Boolean, 0 };
            else if (consume(TokenType::KeywordTrue))
                out = ast::Value{ ast::Value::Type::Boolean, 1 };
            else if (consume(TokenType::String, out.dataString))
                out.type = ast::Value::Type::String;
            else if (consume(TokenType::Number, out.dataNumber))
                out.type = ast::Value::Type::Number;
            else if (consume(TokenType::Identifier, out.dataString))
                out.type = ast::Value::Type::Enum;
            else
                return false;
            return true;
        };

        auto parseAttributes = [&, this](std::vector<ast::AttributeUsage>& attrs) -> bool {
            while (consume(TokenType::LeftBracket)) {
                do {
                    auto& attr = attrs.emplace_back();
                    attr.location = pos();
                    expect(TokenType::Identifier, attr.name);
                    if (consume(TokenType::LeftParen)) {
                        if (!consume(TokenType::RightParen)) {
                            for (;;) {
                                auto& value = attr.params.emplace_back();
                                if (!consumeValue(value))
                                    fail("expected value");
                                if (!consume(TokenType::Comma))
                                    break;

                            }
                            expect(TokenType::RightParen);
                        }
                    }
                } while (consume(TokenType::Comma));
                expect(TokenType::RightBracket);
            }
            return true;
        };

        auto parseType = [&, this](ast::TypeInfo& type) -> bool {
            expect(TokenType::Identifier, type.type);
            if (consume(TokenType::Asterisk))
                type.isPointer = true;
            if (consume(TokenType::LeftBracket)) {
                expect(TokenType::RightBracket);
                type.isArray = true;
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
        expect(TokenType::Identifier, out_module.name);
        expect(TokenType::SemiColon);

        while (!consume(TokenType::EndOfFile)) {
            if (consume(TokenType::Unknown))
                return fail("unexpected input");

            if (consume(TokenType::KeywordImport)) {
                std::string import;
                expect(TokenType::Identifier, import);
                expect(TokenType::SemiColon);

                out_module.imports.push_back(std::move(import));
                continue;
            }

            if (consume(TokenType::KeywordInclude)) {
                std::string include;
                expect(TokenType::String, include);

                out_module.includes.push_back(std::move(include));
                continue;
            }

            if (consume(TokenType::KeywordPragma)) {
                std::string option;
                expect(TokenType::Identifier, option);
                expect(TokenType::SemiColon);

                out_module.pragmas.push_back(std::move(option));
                continue;
            }

            if (consume(TokenType::KeywordAttribute)) {
                auto& attr = out_module.attributes.emplace_back();
                attr.location = pos();

                if (!consume(TokenType::Identifier, attr.name))
                    return fail("expected identifier after `attribute'");
                if (consume(TokenType::LeftBrace)) {
                    while (!consume(TokenType::RightBrace)) {
                        auto& arg = attr.arguments.emplace_back();
                        if (!parseType(arg.type))
                            return false;
                        expect(TokenType::Identifier, arg.name);
                        if (consume(TokenType::Equal)) {
                            if (!consumeValue(arg.init))
                                return fail("expected value after `='");
                        }
                        expect(TokenType::SemiColon);
                    }
                }
                else
                    expect(TokenType::SemiColon);

                continue;
            }

            // optionally build up a list of attributes
            std::vector<ast::AttributeUsage> attributes;
            if (!parseAttributes(attributes))
                return false;

            // parse regular type declarations
            if (consume(TokenType::KeywordType)) {
                auto& type = out_module.types.emplace_back();
                type.location = pos();

                type.attributes = std::move(attributes);
                expect(TokenType::Identifier, type.name);
                if (consume(TokenType::Colon))
                    expect(TokenType::Identifier, type.base);
                if (consume(TokenType::LeftBrace)) {
                    while (!consume(TokenType::RightBrace)) {
                        auto& field = type.fields.emplace_back();
                        if (!parseAttributes(field.attributes))
                            return false;
                        if (!parseType(field.type))
                            return false;
                        field.location = pos();
                        expect(TokenType::Identifier, field.name);
                        if (consume(TokenType::Equal)) {
                            if (!consumeValue(field.init))
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
                auto& enum_ = out_module.enums.emplace_back();
                enum_.location = pos();

                enum_.attributes = std::move(attributes);
                expect(TokenType::Identifier, enum_.name);
                if (consume(TokenType::Colon))
                    expect(TokenType::Identifier, enum_.base);
                expect(TokenType::LeftBrace);
                long long nextValue = 0;
                for (;;) {
                    auto& value = enum_.values.emplace_back();
                    expect(TokenType::Identifier, value.name);
                    if (consume(TokenType::Equal)) {
                        expect(TokenType::Number, value.value);
                        nextValue = value.value + 1;
                    }
                    else
                        value.value = nextValue++;
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

        return analyze(out_module);
#undef expect
    }

    bool Parser::analyze(ast::Module& module) {
        bool valid = true;

        auto error = [&, this](Location const& loc, std::string error, auto const&... args) {
            std::ostringstream buffer;
            ((buffer << loc << ": " << error) << ... << args);
            errors.push_back(buffer.str());
            valid = false;
        };

        // build a map of type names to types
        for (size_t index = 0; index != module.types.size(); ++index)
            module.typeMap[module.types[index].name] = index;

        // ensure all types are valid
        for (auto const& type : module.types) {
            //if (type.imported)
            //    continue;

            if (!type.base.empty() && module.typeMap.find(type.base) == module.typeMap.end()) {
                error(type.location, "unknown type '", type.base, '\'');
            }

            for (auto& field : type.fields) {
                auto typeIt = module.typeMap.find(field.type.type);
                if (typeIt == module.typeMap.end()) {
                    error(field.location, "unknown type '", field.type, '\'');
                    continue;
                }
                auto const& fieldType = module.types[typeIt->second];

                //if (fieldType.enumeration && field.init.type != reTypeNone) {
                //    if (field.init.type != reTypeIdentifier) {
                //        error(field.loc, "enumeration type '", field.type, "' may only be initialized by enumerants");
                //        ok = false;
                //        continue;
                //    }

                //    auto findEnumerant = [&fieldType](std::string const& name) -> EnumValue const* {
                //        for (auto const& value : fieldtype.values) {
                //            if (value.name == name)
                //                return &value;
                //        }
                //        return nullptr;
                //    };

                //    auto const& enumerantName = strings.strings[field.init.value];
                //    auto const* enumValue = findEnumerant(enumerantName);
                //    if (findEnumerant(enumerantName) == nullptr) {
                //        error(field.loc, "enumerant '", enumerantName, "' not found in enumeration '", fieldtype.name, '\'');
                //        error(fieldtype.loc, "enumeration '", fieldtype.name, "' defined here");
                //        continue;
                //    }

                //    field.init = { reTypeNumber, static_cast<int>(enumValue->value) };
                //}
                //else if (field.init.type == reTypeIdentifier) {
                //    error(field.loc, "only enumeration types can be initialized by enumerants");
                //    ok = false;
                //    continue;
                //}
            }
        }

        return valid;
    }
}
