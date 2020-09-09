// sapc - by Sean Middleditch
// This is free and unencumbered software released into the public domain.
// See LICENSE.md for more details.

#include "parser.hh"
#include "model.hh"
#include "lexer.hh"

#include <charconv>
#include <sstream>
#include <fstream>

namespace sapc {
    static bool loadText(std::filesystem::path const& filename, std::string& out_text) {
        // open file and read contents
        std::ifstream stream(filename);
        if (!stream)
            return false;

        std::ostringstream buffer;
        buffer << stream.rdbuf();
        stream.close();
        out_text = buffer.str();
        return true;
    }

    static std::filesystem::path resolvePath(std::vector<std::filesystem::path> const& search, std::filesystem::path filename) {
        if (filename.is_absolute())
            return filename;

        for (auto const& path : search) {
            auto tmp = path / filename;
            if (std::filesystem::exists(tmp))
                return tmp;
        }

        return {};
    }

    bool Parser::compile(std::filesystem::path filename, Module& out_module) {
        std::string contents;
        if (!loadText(filename, contents)) {
            std::ostringstream buffer;
            buffer << filename.string() << ": failed to open input";
            errors.push_back(buffer.str());
            return false;
        }

        out_module.filename = filename;
        dependencies.push_back(filename);

        search.push_back(filename.parent_path());

        // add built-in types
        static std::string const builtins[] = {
            "string", "bool",
            "i8", "i16", "i32", "i64",
            "u8", "u16", "u32", "u64",
            "f328", "f64"
        };

        for (auto const& builtin : builtins) {
            auto& type = out_module.types.emplace_back();
            type.name = builtin;
            type.module = "$core";
            out_module.typeMap[builtin] = out_module.types.size() - 1;
        }

        tokenize(contents, tokens);
        if (!parse(out_module))
            return false;
        return analyze(out_module);
    }

    bool Parser::parse(Module& out_module) {
        size_t next = 0;

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

        auto fail = [&, this](std::string message, auto const&... args) {
            auto const& tok = next < tokens.size() ? tokens[next] : tokens.back();
            auto const loc = Location{ out_module.filename, tok.pos.line, tok.pos.column };
            std::ostringstream buffer;
            buffer << loc << ": " << message;
            (buffer << ... << args);
            errors.push_back({ buffer.str() });
            return false;
        };

        auto pos = [&]() {
            auto const& tokPos = next > 0 ? tokens[next - 1].pos : tokens.front().pos;
            return Location{ out_module.filename, tokPos.line, tokPos.column };
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
            return true;
        };

        auto parseAttributes = [&, this](std::vector<AttributeUsage>& attrs) -> bool {
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

        auto parseType = [&, this](TypeInfo& type) -> bool {
            expect(TokenType::Identifier, type.type);
            if (consume(TokenType::Asterisk))
                type.isPointer = true;
            if (consume(TokenType::LeftBracket)) {
                expect(TokenType::RightBracket);
                type.isArray = true;
            }
            return true;
        };

        auto pushType = [&, this](Type&& type) -> bool {
            assert(!type.name.empty());

            if (type.module.empty())
                type.module = out_module.name;

            auto const index = out_module.types.size();

            auto const it = out_module.typeMap.find(type.name);
            if (it != out_module.typeMap.end()) {
                auto const& other = out_module.types[it->second];

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

            out_module.typeMap[type.name] = index;
            out_module.types.push_back(std::move(type));
            return true;
        };

        // expect module first
        expect(TokenType::KeywordModule);
        if (out_module.name.empty())
            expect(TokenType::Identifier, out_module.name);
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

                if (out_module.imports.count(import) != 0)
                    continue;

                out_module.imports.emplace(import);

                auto importPath = resolvePath(search, std::filesystem::path(import).replace_extension(".sap"));
                if (importPath.empty())
                    return fail("could not find module `", import, '\'');

                Parser parser;
                Module importedModule;
                parser.search = search;
                if (!parser.compile(importPath, importedModule)) {
                    errors.insert(end(errors), begin(parser.errors), end(parser.errors));
                    return false;
                }

                for (auto& type : importedModule.types)
                    pushType(std::move(type));

                for (auto const& dependency : parser.dependencies)
                    dependencies.push_back(dependency);

                continue;
            }

            if (consume(TokenType::KeywordInclude)) {
                std::string include;
                expect(TokenType::String, include);

                auto includePath = resolvePath(search, include);
                if (includePath.empty())
                    return fail("could not find `", include, '\'');

                std::string contents;
                if (!loadText(includePath, contents))
                    return fail("failed to open `", includePath.string(), '\'');

                dependencies.push_back(includePath);

                Parser parser;
                parser.search = search;
                tokenize(contents, parser.tokens);
                if (!parser.parse(out_module)) {
                    errors.insert(end(errors), begin(parser.errors), end(parser.errors));
                    return false;
                }

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
                auto attr = Type{};
                attr.location = pos();
                attr.isAttribute = true;

                if (!consume(TokenType::Identifier, attr.name))
                    return fail("expected identifier after `attribute'");
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
                auto& type = Type{};
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

                pushType(std::move(type));
                continue;
            }

            // parse enumerations
            if (consume(TokenType::KeywordEnum)) {
                auto& enum_ = Type{};
                enum_.location = pos();
                enum_.isEnumeration = true;

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

    bool Parser::analyze(Module& module) {
        bool valid = true;

        auto error = [&, this](Location const& loc, std::string error, auto const&... args) {
            std::ostringstream buffer;
            ((buffer << loc << ": " << error) << ... << args);
            errors.push_back(buffer.str());
            valid = false;
        };

        // ensure all types are valid
        for (auto& type : module.types) {
            if (!type.base.empty() && module.typeMap.find(type.base) == module.typeMap.end()) {
                error(type.location, "unknown type `", type.base, '\'');
            }

            for (auto& field : type.fields) {
                auto typeIt = module.typeMap.find(field.type.type);
                if (typeIt == module.typeMap.end()) {
                    error(field.location, "unknown type `", field.type, '\'');
                    continue;
                }
                auto const& fieldType = module.types[typeIt->second];

                if (fieldType.isEnumeration && field.init.type != Value::Type::None) {
                    if (field.init.type != Value::Type::Enum) {
                        error(field.location, "enumeration type `", field.type, "' may only be initialized by enumerants");
                        continue;
                    }

                    auto findEnumerant = [&](std::string const& name) -> EnumValue const* {
                        for (auto const& value : fieldType.values) {
                            if (value.name == name)
                                return &value;
                        }
                        return nullptr;
                    };

                    auto const* enumValue = findEnumerant(field.init.dataString);
                    if (enumValue == nullptr) {
                        error(field.location, "enumerant `", field.init.dataString, "' not found in enumeration '", fieldType.name, '\'');
                        error(fieldType.location, "enumeration `", fieldType.name, "' defined here");
                        continue;
                    }

                    field.init.type = Value::Type::Number;
                    field.init.dataNumber = enumValue->value;
                }
                else if (field.init.type == Value::Type::Enum)
                    error(field.location, "only enumeration types can be initialized by enumerants");
            }
        }

        // expand attribute parameters
        auto expand = [&, this](AttributeUsage& attr) {
            auto const it = module.typeMap.find(attr.name);
            if (it == module.typeMap.end()) {
                error(attr.location, "unknown attribute `", attr.name, '\'');
                return;
            }

            auto const argc = attr.params.size();
            auto const& attrType = module.types[it->second];
            auto const& params = attrType.fields;

            if (!attrType.isAttribute) {
                error(attr.location, "attribute type `", attr.name, "' is declared as a regular type (not attribute) at ", attrType.location);
                return;
            }

            if (argc > params.size()) {
                error(attr.location, "too many arguments to attribute `", attr.name, '\'');
                return;
            }

            for (size_t index = argc; index != params.size(); ++index) {
                auto const& param = params[index];

                if (param.init.type == Value::Type::None)
                    error(attr.location, "missing required argument `", param.name, "' to attribute `", attr.name, '\'');
                else
                    attr.params.push_back(param.init);
            }
        };

        for (auto& type : module.types) {
            for (auto& attr : type.attributes)
                expand(attr);

            for (auto& field : type.fields) {
                for (auto& attr : field.attributes)
                    expand(attr);
            }
        }

        return valid;
    }
}
