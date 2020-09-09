// sapc - by Sean Middleditch
// This is free and unencumbered software released into the public domain.
// See LICENSE.md for more details.

#include "grammar.hh"
#include "compiler.hh"
#include "model.hh"
#include "lexer.hh"
#include "file_util.hh"

#include <charconv>
#include <sstream>
#include <fstream>

namespace sapc {
    bool parse(std::vector<Token> const& tokens, std::vector<std::filesystem::path> const& search, std::vector<std::string>& errors, std::vector<std::filesystem::path>& dependencies, Module& module) {
        size_t next = 0;

        struct Consume {
            size_t& next;
            std::vector<Token> const& tokens;

            bool operator()(TokenType type) {
                if (next >= tokens.size())
                    return false;

                if (tokens[next].type != type)
                    return false;

                ++next;
                return true;
            }

            bool operator()(TokenType type, std::string& out) {
                auto const index = next;
                if (!(*this)(type))
                    return false;

                out = tokens[index].dataString;
                return true;
            }

            bool operator()(TokenType type, long long& out) {
                auto const index = next;
                if (!(*this)(type))
                    return false;

                out = tokens[index].dataNumber;
                return true;
            }
        } consume{ next, tokens };

        auto resolve = [&](std::filesystem::path filename) ->  std::filesystem::path {
            if (filename.is_absolute())
                return filename;

            if (!module.filename.empty()) {
                auto tmp = module.filename.parent_path() / filename;
                if (std::filesystem::exists(tmp))
                    return tmp;
            }

            for (auto const& path : search) {
                auto tmp = path / filename;
                if (std::filesystem::exists(tmp))
                    return tmp;
            }

            return {};
        };

        auto fail = [&](std::string message, auto const&... args) {
            auto const& tok = next < tokens.size() ? tokens[next] : tokens.back();
            Location const loc{ module.filename, tok.pos.line, tok.pos.column };
            std::ostringstream buffer;
            ((buffer << loc << ": " << message) << ... << args);
            errors.push_back({ buffer.str() });
            return false;
        };

        auto pos = [&]() {
            auto const& tokPos = next > 0 ? tokens[next - 1].pos : tokens.front().pos;
            return Location{ module.filename, tokPos.line, tokPos.column };
        };

#if defined(expect)
#   undef expect
#endif
#define expect(type,...) \
        do{ \
            auto const _ttype = (type); \
            if (!consume(_ttype,##__VA_ARGS__)) { \
                std::ostringstream buf; \
                buf << "expected " << _ttype; \
                if (next < tokens.size()) \
                    buf << ", got " << tokens[next]; \
                return fail(buf.str()); \
            } \
        }while(false)

        auto consumeValue = [&](Value& out) {
            if (consume(TokenType::KeywordNull))
                out = Value{ Value::Type::Null };
            else if (consume(TokenType::KeywordFalse))
                out = Value{ Value::Type::Boolean, 0 };
            else if (consume(TokenType::KeywordTrue))
                out = Value{ Value::Type::Boolean, 1 };
            else if (consume(TokenType::String, out.dataString))
                out.type = Value::Type::String;
            else if (consume(TokenType::Number, out.dataNumber))
                out.type = Value::Type::Number;
            else if (consume(TokenType::Identifier, out.dataString))
                out.type = Value::Type::Enum;
            else
                return false;
            out.location = pos();
            return true;
        };

        auto parseAttributes = [&](std::vector<AttributeUsage>& attrs) -> bool {
            while (consume(TokenType::LeftBracket)) {
                do {
                    auto& attr = attrs.emplace_back();
                    expect(TokenType::Identifier, attr.name);
                    attr.location = pos();
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

        auto parseType = [&](TypeInfo& type) -> bool {
            expect(TokenType::Identifier, type.type);
            if (consume(TokenType::Asterisk))
                type.isPointer = true;
            if (consume(TokenType::LeftBracket)) {
                expect(TokenType::RightBracket);
                type.isArray = true;
            }
            return true;
        };

        auto pushType = [&](Type&& type) -> bool {
            assert(!type.name.empty());

            if (type.module.empty())
                type.module = module.name;

            auto const index = module.types.size();

            auto const it = module.typeMap.find(type.name);
            if (it != module.typeMap.end()) {
                auto const& other = module.types[it->second];

                // duplicate type definitions from the same location/module are allowed
                if (type.location == other.location && type.module == other.module)
                    return true;

                std::ostringstream buffer;
                buffer << type.location << "duplicate definition of type `" << type.name << '\'';
                errors.push_back(buffer.str());
                buffer.clear();
                buffer << other.location << "previous definition of type `" << other.name << '\'';
                errors.push_back(buffer.str());
                return false;
            }

            module.typeMap[type.name] = index;
            module.types.push_back(std::move(type));
            return true;
        };

        // expect module first
        expect(TokenType::KeywordModule);
        if (module.name.empty())
            expect(TokenType::Identifier, module.name);
        else
            expect(TokenType::Identifier);
        expect(TokenType::SemiColon);

        while (!consume(TokenType::EndOfFile)) {
            if (consume(TokenType::Unknown))
                return fail("unexpected input");

            if (consume(TokenType::KeywordImport)) {
                std::string import;
                expect(TokenType::Identifier, import);
                expect(TokenType::SemiColon);

                if (module.imports.count(import) != 0)
                    continue;

                module.imports.emplace(import);

                auto importPath = resolve(std::filesystem::path(import).replace_extension(".sap"));
                if (importPath.empty())
                    return fail("could not find module `", import, '\'');

                Module importedModule;
                if (!compile(importPath, search, errors, dependencies, importedModule))
                    return false;

                for (auto& type : importedModule.types)
                    pushType(std::move(type));

                continue;
            }

            if (consume(TokenType::KeywordInclude)) {
                std::string include;
                expect(TokenType::String, include);

                auto includePath = resolve(include);
                if (includePath.empty())
                    return fail("could not find include `", include, '\'');

                std::string contents;
                if (!loadText(includePath, contents))
                    return fail("failed to open `", includePath.string(), '\'');

                dependencies.push_back(includePath);

                std::vector<Token> includeTokens;
                tokenize(contents, includeTokens);
                if (!parse(includeTokens, search, errors, dependencies, module))
                    return false;

                continue;
            }

            if (consume(TokenType::KeywordPragma)) {
                std::string option;
                expect(TokenType::Identifier, option);
                expect(TokenType::SemiColon);

                module.pragmas.push_back(std::move(option));
                continue;
            }

            if (consume(TokenType::KeywordAttribute)) {
                auto attr = Type{};
                attr.isAttribute = true;

                if (!consume(TokenType::Identifier, attr.name))
                    return fail("expected identifier after `attribute'");

                attr.location = pos();

                if (consume(TokenType::LeftBrace)) {
                    while (!consume(TokenType::RightBrace)) {
                        auto& arg = attr.fields.emplace_back();
                        if (!parseType(arg.type))
                            return false;
                        arg.location = pos();
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

                pushType(std::move(attr));
                continue;
            }

            // optionally build up a list of attributes
            std::vector<AttributeUsage> attributes;
            if (!parseAttributes(attributes))
                return false;

            // parse regular type declarations
            if (consume(TokenType::KeywordType)) {
                auto type = Type{};

                type.attributes = std::move(attributes);
                expect(TokenType::Identifier, type.name);

                type.location = pos();

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

                pushType(std::move(type));
                continue;
            }

            // parse enumerations
            if (consume(TokenType::KeywordEnum)) {
                auto enum_ = Type{};
                enum_.isEnumeration = true;

                enum_.attributes = std::move(attributes);
                expect(TokenType::Identifier, enum_.name);

                enum_.location = pos();

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

                pushType(std::move(enum_));

                continue;
            }

            std::ostringstream buf;
            buf << "unexpected " << tokens[next];
            return fail(buf.str());
        }

#undef expect

        return true;
    }
}
