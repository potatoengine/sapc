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

#if defined(EXPECT)
#   undef EXPECT
#endif
#define EXPECT(type,...) do{ if (!mustConsume((type),##__VA_ARGS__)) return false; }while(0)

#if defined(EXPECT_VALUE)
#   undef EXPECT_VALUE
#endif
#define EXPECT_VALUE(out) do{ if (!mustConsumeValue((out))) return false; }while(0)

namespace sapc {
    namespace {
        struct Grammar {
            std::vector<Token> const& tokens;
            std::vector<fs::path> const& search;
            std::vector<std::string>& errors;
            std::vector<fs::path>& dependencies;
            fs::path const& filename;
            Module& module;
            size_t next = 0;

            inline bool consume(TokenType type);
            inline bool consume(TokenType type, std::string& out);
            inline bool consume(TokenType type, long long& out);

            inline Location pos();

            template <typename... T>
            bool error(Location const& loc, std::string message, T const&... args);

            template <typename... T>
            bool fail(std::string message, T const&... args);

            inline bool mustConsume(TokenType type);
            template <typename T>
            bool mustConsume(TokenType type, T& out);
            inline bool mustConsumeFail(TokenType type);

            inline bool consumeValue(Value& out);
            inline bool mustConsumeValue(Value& out);

            inline fs::path resolve(fs::path target);

            inline bool parseAnnotations(std::vector<Annotation>& annotations);
            inline bool parseType(TypeInfo& type);
            inline bool parseFile();

            inline bool pushType(Type&& type);
            inline bool pushConstant(Constant&& constant);
        };
    }

    bool parse(std::vector<Token> const& tokens, std::vector<fs::path> const& search, std::vector<std::string>& errors, std::vector<fs::path>& dependencies, fs::path const& filename, Module& module) {
        Grammar grammar{ tokens, search, errors, dependencies, filename, module };
        return grammar.parseFile();
    }

    bool Grammar::parseFile() {
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
                EXPECT(TokenType::Identifier, import);
                EXPECT(TokenType::SemiColon);

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

            if (consume(TokenType::KeywordAttribute)) {
                auto attr = Type{};
                attr.kind = Type::Kind::Attribute;

                EXPECT(TokenType::Identifier, attr.name);

                attr.location = pos();

                if (consume(TokenType::LeftBrace)) {
                    while (!consume(TokenType::RightBrace)) {
                        auto& arg = attr.fields.emplace_back();
                        if (consume(TokenType::KeywordTypename))
                            arg.type.isTypeName = true;
                        else if (!parseType(arg.type))
                            return false;
                        arg.location = pos();
                        EXPECT(TokenType::Identifier, arg.name);
                        if (consume(TokenType::Equal))
                            EXPECT_VALUE(arg.init);
                        EXPECT(TokenType::SemiColon);
                    }
                }
                else
                    EXPECT(TokenType::SemiColon);

                pushType(std::move(attr));
                continue;
            }

            // optionally build up a list of annotations
            std::vector<Annotation> annotations;
            if (!parseAnnotations(annotations))
                return false;

            // parse module
            if (consume(TokenType::KeywordModule)) {
                if (!module.name.empty())
                    return fail("module has already been declared");
                EXPECT(TokenType::Identifier, module.name);
                EXPECT(TokenType::SemiColon);

                module.annotations = std::move(annotations);
                continue;
            }

            // parse constants
            if (consume(TokenType::KeywordConst)) {
                auto constant = Constant{};

                constant.annotations = std::move(annotations);

                if (!parseType(constant.type))
                    return false;

                EXPECT(TokenType::Identifier, constant.name);

                constant.location = pos();

                EXPECT(TokenType::Equal);
                EXPECT_VALUE(constant.init);
                EXPECT(TokenType::SemiColon);

                pushConstant(std::move(constant));
                continue;
            }

            // parse unions
            if (consume(TokenType::KeywordUnion)) {
                auto union_ = Type{};
                union_.kind = Type::Kind::Union;

                union_.annotations = std::move(annotations);
                EXPECT(TokenType::Identifier, union_.name);

                union_.location = pos();

                EXPECT(TokenType::LeftBrace);
                while (!consume(TokenType::RightBrace)) {
                    auto& field = union_.fields.emplace_back();
                    if (!parseType(field.type))
                        return false;
                    EXPECT(TokenType::Identifier, field.name);
                    field.location = pos();
                    EXPECT(TokenType::SemiColon);
                }

                if (union_.fields.empty())
                    return error(union_.location, "union `", union_.name, "' must have at least one field");

                pushType(std::move(union_));
                continue;
            }

            // parse aggregate declarations
            if (consume(TokenType::KeywordStruct)) {
                auto type = Type{};
                type.kind = Type::Kind::Struct;

                type.annotations = std::move(annotations);
                EXPECT(TokenType::Identifier, type.name);

                type.location = pos();

                if (consume(TokenType::SemiColon)) {
                    type.kind = Type::Kind::Opaque;
                }
                else {
                    if (consume(TokenType::Colon))
                        EXPECT(TokenType::Identifier, type.base);
                    EXPECT(TokenType::LeftBrace);
                    while (!consume(TokenType::RightBrace)) {
                        auto& field = type.fields.emplace_back();
                        if (!parseAnnotations(field.annotations))
                            return false;
                        if (!parseType(field.type))
                            return false;
                        EXPECT(TokenType::Identifier, field.name);
                        field.location = pos();
                        if (consume(TokenType::Equal))
                            EXPECT_VALUE(field.init);
                        EXPECT(TokenType::SemiColon);
                    }
                }

                pushType(std::move(type));
                continue;
            }

            // parse enumerations
            if (consume(TokenType::KeywordEnum)) {
                auto enum_ = Type{};
                enum_.kind = Type::Kind::Enum;

                enum_.annotations = std::move(annotations);
                EXPECT(TokenType::Identifier, enum_.name);

                enum_.location = pos();

                if (consume(TokenType::Colon))
                    EXPECT(TokenType::Identifier, enum_.base);
                EXPECT(TokenType::LeftBrace);
                long long nextValue = 0;
                for (;;) {
                    auto& value = enum_.fields.emplace_back();
                    EXPECT(TokenType::Identifier, value.name);
                    value.location = pos();
                    value.init.type = Value::Type::Number;
                    if (consume(TokenType::Equal)) {
                        EXPECT(TokenType::Number, value.init.dataNumber);
                        nextValue = value.init.dataNumber + 1;
                    }
                    else
                        value.init.dataNumber = nextValue++;
                    if (!consume(TokenType::Comma))
                        break;
                }
                EXPECT(TokenType::RightBrace);

                pushType(std::move(enum_));

                continue;
            }

            return fail("unexpected ", tokens[next]);
        }

        if (module.name.empty())
            return fail("missing module declaration");

        return true;
    }


    bool Grammar::consume(TokenType type) {
        if (next >= tokens.size())
            return false;

        if (tokens[next].type != type)
            return false;

        ++next;
        return true;
    }

    bool Grammar::consume(TokenType type, std::string& out) {
        auto const index = next;
        if (!consume(type))
            return false;

        out = tokens[index].dataString;
        return true;
    }

    bool Grammar::consume(TokenType type, long long& out) {
        auto const index = next;
        if (!consume(type))
            return false;

        out = tokens[index].dataNumber;
        return true;
    }

    Location Grammar::pos() {
        auto const& tokPos = next > 0 ? tokens[next - 1].pos : tokens.front().pos;
        return Location{ filename, tokPos.line, tokPos.column };
    }

    template <typename... T>
    bool Grammar::error(Location const& loc, std::string message, T const&... args) {
        std::ostringstream buffer;
        ((buffer << loc << ": " << message) << ... << args);
        errors.push_back({ buffer.str() });
        return false;
    };

    template <typename... T>
    bool Grammar::fail(std::string message, T const&... args) {
        auto const& tok = next < tokens.size() ? tokens[next] : tokens.back();
        Location const loc{ filename, tok.pos.line, tok.pos.column };
        return error(loc, message, args...);
    };

    bool Grammar::mustConsume(TokenType type) {
        if (consume(type))
            return true;
        return mustConsumeFail(type);
    }

    template <typename T>
    bool Grammar::mustConsume(TokenType type, T& out) {
        if (consume(type, out))
            return true;
        return mustConsumeFail(type);
    }

    bool Grammar::mustConsumeFail(TokenType type) {
        std::ostringstream buf;
        buf << "expected " << type;
        if (next > 0)
            buf << " after " << tokens[next - 1];
        if (next < tokens.size())
            buf << ", got " << tokens[next];
        return fail(buf.str());
    }

    bool Grammar::mustConsumeValue(Value& out) {
        if (consumeValue(out))
            return true;

        std::ostringstream buf;
        buf << "expected value";
        if (next > 0)
            buf << " after " << tokens[next - 1];
        if (next < tokens.size())
            buf << ", got " << tokens[next];
        return fail(buf.str());
    }

    bool Grammar::consumeValue(Value& out) {
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
        else if (consume(TokenType::Identifier, out.dataString)) {
            if (consume(TokenType::Dot)) {
                EXPECT(TokenType::Identifier, out.dataName);
                out.type = Value::Type::Enum;
            }
            else {
                out.type = Value::Type::TypeName;
            }
        }
        else if (consume(TokenType::LeftBrace)) {
            out.type = Value::Type::List;
            if (!consume(TokenType::RightBrace)) {
                for (;;) {
                    Value val;
                    EXPECT_VALUE(val);
                    out.dataList.push_back(std::move(val));
                    if (!consume(TokenType::Comma))
                        break;
                }
                EXPECT(TokenType::RightBrace);
            }
        }
        else
            return false;
        out.location = pos();
        return true;
    }

    fs::path Grammar::resolve(fs::path target) {
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
    }

    bool Grammar::parseAnnotations(std::vector<Annotation>& annotations) {
        while (consume(TokenType::LeftBracket)) {
            do {
                Annotation annotation;
                EXPECT(TokenType::Identifier, annotation.name);
                annotation.location = pos();

                auto const otherIt = find_if(begin(annotations), end(annotations), [&](Annotation const& other) { return other.name == annotation.name; });
                if (otherIt != end(annotations)) {
                    error(annotation.location, "duplicate annotation `", annotation.name, '\'');
                    return error(otherIt->location, "previous annotation here");
                }

                if (consume(TokenType::LeftParen)) {
                    if (!consume(TokenType::RightParen)) {
                        for (;;) {
                            auto& value = annotation.arguments.emplace_back();
                            EXPECT_VALUE(value);
                            if (!consume(TokenType::Comma))
                                break;

                        }
                        EXPECT(TokenType::RightParen);
                    }
                }

                annotations.push_back(std::move(annotation));
            } while (consume(TokenType::Comma));
            EXPECT(TokenType::RightBracket);
        }
        return true;
    };

    bool Grammar::parseType(TypeInfo& type) {
        EXPECT(TokenType::Identifier, type.type);
        if (consume(TokenType::Asterisk))
            type.isPointer = true;
        if (consume(TokenType::LeftBracket)) {
            EXPECT(TokenType::RightBracket);
            type.isArray = true;
        }
        return true;
    }

    bool Grammar::pushType(Type&& type) {
        assert(type.kind != Type::Kind::Unknown);
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
    }

    bool Grammar::pushConstant(Constant&& constant) {
        assert(!constant.name.empty());

        if (constant.module.empty())
            constant.module = module.name;

        auto const index = module.constants.size();

        auto const it = module.constantMap.find(constant.name);
        if (it != module.constantMap.end()) {
            auto const& other = module.constants[it->second];

            // duplicate type definitions from the same location/module are allowed
            if (constant.location == other.location && constant.module == other.module)
                return true;

            error(constant.location, "duplicate definition of constant `", constant.name, '\'');
            return error(other.location, "previous definition of constant `", other.name, '\'');
        }

        module.constantMap[constant.name] = index;
        module.constants.push_back(std::move(constant));
        return true;
    };
}
