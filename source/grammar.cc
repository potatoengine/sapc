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

namespace fs = std::filesystem;

namespace sapc {
    bool parse(std::vector<Token> const& tokens, std::vector<fs::path> const& search, std::vector<std::string>& errors, std::vector<fs::path>& dependencies, fs::path const& filename, Module& module) {
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

        auto resolve = [&](fs::path target) ->  fs::path {
            if (target.is_absolute())
                return filename;

            if (!filename.empty()) {
                auto tmp = filename.parent_path() / target;
                if (fs::exists(tmp))
                    return tmp;
            }

            for (auto const& path : search) {
                auto tmp = path / target;
                if (fs::exists(tmp))
                    return tmp;
            }

            return {};
        };

        auto error = [&](Location const& loc, std::string message, auto const&... args) {
            std::ostringstream buffer;
            ((buffer << loc << ": " << message) << ... << args);
            errors.push_back({ buffer.str() });
            return false;
        };

        auto fail = [&](std::string message, auto const&... args) {
            auto const& tok = next < tokens.size() ? tokens[next] : tokens.back();
            Location const loc{ filename, tok.pos.line, tok.pos.column };
            return error(loc, message, args...);
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
            if (!consume(_ttype,##__VA_ARGS__)) { \
                std::ostringstream buf; \
                buf << "expected " << _ttype; \
                if (next > 0) \
                    buf << " after " << tokens[next - 1]; \
                if (next < tokens.size()) \
                    buf << ", got " << tokens[next]; \
                return fail(buf.str()); \
            } \
        } while(false)

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

#define expectValue(out) \
        do{ \
            if (!consumeValue((out))) { \
                std::ostringstream buf; \
                buf << "expected value"; \
                if (next > 0) \
                    buf << " after " << tokens[next - 1]; \
                if (next < tokens.size()) \
                    buf << ", got " << tokens[next]; \
                return fail(buf.str()); \
            } \
        } while(false)

        auto parseAttributes = [&](std::vector<Attribute>& attrs) -> bool {
            while (consume(TokenType::LeftBracket)) {
                do {
                    auto& attr = attrs.emplace_back();
                    expect(TokenType::Identifier, attr.name);
                    attr.location = pos();
                    if (consume(TokenType::LeftParen)) {
                        if (!consume(TokenType::RightParen)) {
                            for (;;) {
                                auto& value = attr.params.emplace_back();
                                expectValue(value);
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

                error(type.location, "duplicate definition of type `", type.name, '\'');
                return error(other.location, "previous definition of type `", other.name, '\'');
            }

            module.typeMap[type.name] = index;
            module.types.push_back(std::move(type));
            return true;
        };

        while (!consume(TokenType::EndOfFile)) {
            if (consume(TokenType::Unknown)) {
                std::ostringstream buf;
                buf << "unexpected input";
                if (next > 0)
                    buf << " after " << tokens[next - 1];
                return fail(buf.str());
            }

            if (consume(TokenType::KeywordImport)) {
                std::string import;
                expect(TokenType::Identifier, import);
                expect(TokenType::SemiColon);

                if (module.imports.count(import) != 0)
                    continue;

                module.imports.emplace(import);

                auto importPath = resolve(fs::path(import).replace_extension(".sap"));
                if (importPath.empty())
                    return fail("could not find module `", import, '\'');

                Module importedModule;
                if (!compile(importPath, search, errors, dependencies, importedModule))
                    return false;

                for (auto& type : importedModule.types)
                    pushType(std::move(type));

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

                expect(TokenType::Identifier, attr.name);

                attr.location = pos();

                if (consume(TokenType::LeftBrace)) {
                    while (!consume(TokenType::RightBrace)) {
                        auto& arg = attr.fields.emplace_back();
                        if (!parseType(arg.type))
                            return false;
                        arg.location = pos();
                        expect(TokenType::Identifier, arg.name);
                        if (consume(TokenType::Equal))
                            expectValue(arg.init);
                        expect(TokenType::SemiColon);
                    }
                }
                else
                    expect(TokenType::SemiColon);

                pushType(std::move(attr));
                continue;
            }

            // optionally build up a list of attributes
            std::vector<Attribute> attributes;
            if (!parseAttributes(attributes))
                return false;

            // parse module
            if (consume(TokenType::KeywordModule)) {
                if (!module.name.empty())
                    return fail("module has already been declared");
                expect(TokenType::Identifier, module.name);
                expect(TokenType::SemiColon);

                module.attributes = std::move(attributes);
                continue;
            }

            // parse type declarations
            if (consume(TokenType::KeywordType)) {
                auto type = Type{};

                type.attributes = std::move(attributes);
                expect(TokenType::Identifier, type.name);

                type.location = pos();

                expect(TokenType::SemiColon);

                pushType(std::move(type));
                continue;
            }

            // parse struct declarations
            if (consume(TokenType::KeywordStruct)) {
                auto type = Type{};

                type.attributes = std::move(attributes);
                expect(TokenType::Identifier, type.name);

                type.location = pos();

                if (consume(TokenType::Colon))
                    expect(TokenType::Identifier, type.base);
                expect(TokenType::LeftBrace);
                while (!consume(TokenType::RightBrace)) {
                    auto& field = type.fields.emplace_back();
                    if (!parseAttributes(field.attributes))
                        return false;
                    if (!parseType(field.type))
                        return false;
                    field.location = pos();
                    expect(TokenType::Identifier, field.name);
                    if (consume(TokenType::Equal))
                        expectValue(field.init);
                    expect(TokenType::SemiColon);
                }

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

            return fail("unexpected ", tokens[next]);
        }

#undef expect
#undef expectValue

        if (module.name.empty())
            return fail("missing module declaration");

        return true;
    }
}
