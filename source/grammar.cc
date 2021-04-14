// sapc - by Sean Middleditch
// This is free and unencumbered software released into the public domain.
// See LICENSE.md for more details.

#include "grammar.hh"
#include "compiler.hh"
#include "lexer.hh"
#include "location.hh"
#include "file_util.hh"
#include "ast.hh"
#include "log.hh"

#include <cassert>

namespace fs = std::filesystem;

#if defined(EXPECT)
#   undef EXPECT
#endif
#define EXPECT(first,...) do{ if (!mustConsume((first),##__VA_ARGS__)) return {}; }while(0)

namespace sapc {
    namespace {
        constexpr unsigned AllowNamespaces = 1 << 0;
        constexpr unsigned AllowTypes = 1 << 1;
        constexpr unsigned AllowModule = 1 << 2;
        constexpr unsigned AllowAttributes = 1 << 3;
        constexpr unsigned AllowImport = 1 << 4;
        constexpr unsigned AllowConstants = 1 << 5;

        constexpr unsigned ConfigModule = AllowNamespaces | AllowTypes | AllowModule | AllowAttributes | AllowImport | AllowConstants;
        constexpr unsigned ConfigNamespace = AllowNamespaces | AllowTypes | AllowConstants;

        struct Grammar {
            std::vector<Token> const& tokens;
            Log& log;
            fs::path const& filename;
            ast::ModuleUnit& module;
            size_t next = 0;
            std::vector<std::vector<std::unique_ptr<ast::Declaration>>*> scopeStack;

            inline Location pos();

            template <typename... T>
            bool fail(std::string message, T const&... args);

            inline bool match(TokenType type);

            inline bool consume(TokenType type);

            inline bool mustConsume(TokenType type);
            inline bool mustConsume(long long& out);
            inline bool mustConsume(ast::Identifier& out);
            inline bool mustConsume(ast::QualifiedId& out);
            inline bool mustConsume(ast::Literal& out);
            inline bool mustConsume(std::unique_ptr<ast::TypeRef>& type);
            inline bool mustConsume(std::vector<ast::Annotation>& annotations);

            inline bool parseFile();
            inline bool parseScope(Location& start, TokenType terminate, unsigned config);

            template <typename DeclT>
            DeclT& begin();
        };
    }

    std::unique_ptr<ast::ModuleUnit> parse(std::filesystem::path const& filename, Log& log) {
        std::string contents;
        if (!loadText(filename, contents)) {
            log.error(filename.string(), ": failed to open input");
            return nullptr;
        }

        std::vector<Token> tokens;
        if (!tokenize(contents, filename, tokens, log))
            return nullptr;

        auto mod = std::make_unique<ast::ModuleUnit>();
        mod->filename = filename;

        Grammar grammar{ tokens, log, filename, *mod };
        if (!grammar.parseFile())
            return nullptr;

        return mod;
    }

    bool Grammar::parseFile() {
        scopeStack.push_back(&module.decls);

        if (!parseScope(Location{ module.filename, {1, 0} }, TokenType::EndOfFile, ConfigModule))
            return false;

        if (module.name.empty())
            return fail("missing module declaration");

        return true;
    }

    bool Grammar::parseScope(Location& start, TokenType terminate, unsigned config) {
        std::vector<ast::Annotation> annotations;

        while (!consume(terminate)) {
            if (consume(TokenType::EndOfFile)) {
                log.error(pos(), "unexpected end of file");
                log.info(start, "unclosed scope started here");
                return false;
            }

            if (consume(TokenType::Unknown)) {
                std::ostringstream buf;
                buf << "unexpected input";
                if (next > 0)
                    buf << " after " << tokens[next - 1];
                return fail(buf.str());
            }

            if ((config & AllowImport) != 0 && consume(TokenType::KeywordImport)) {
                auto& imp = begin<ast::ImportDecl>();

                EXPECT(imp.target);
                EXPECT(TokenType::SemiColon);

                continue;
            }

            if ((config & AllowAttributes) != 0 && consume(TokenType::KeywordAttribute)) {
                auto& attr = begin<ast::AttributeDecl>();

                EXPECT(attr.name);

                if (consume(TokenType::LeftBrace)) {
                    while (!consume(TokenType::RightBrace)) {
                        auto& field = attr.fields.emplace_back();
                        EXPECT(field.type);
                        EXPECT(field.name);
                        if (consume(TokenType::Equal)) {
                            ast::Literal lit;
                            EXPECT(lit);
                            field.init = std::move(lit);
                        }
                        EXPECT(TokenType::SemiColon);
                    }
                }
                else
                    EXPECT(TokenType::SemiColon);

                continue;
            }

            // optionally build up a list of annotations
            annotations.clear();
            while (match(TokenType::LeftBracket))
                EXPECT(annotations);

            // parse namespace
            if ((config & AllowNamespaces) != 0 && consume(TokenType::KeywordNamespace)) {
                auto& nsDecl = begin<ast::NamespaceDecl>();

                EXPECT(nsDecl.name);
                EXPECT(TokenType::LeftBrace);

                scopeStack.push_back(&nsDecl.decls);
                if (!parseScope(nsDecl.name.loc, TokenType::RightBrace, ConfigNamespace))
                    return false;
                scopeStack.pop_back();

                continue;
            }

            // parse module
            if ((config & AllowModule) != 0 && consume(TokenType::KeywordModule)) {
                auto& modDecl = begin<ast::ModuleDecl>();

                EXPECT(modDecl.name);
                if (module.name.empty())
                    module.name = modDecl.name;
                EXPECT(TokenType::SemiColon);

                modDecl.annotations = std::move(annotations);
                continue;
            }

            // parse constants
            if ((config & AllowConstants) != 0 && consume(TokenType::KeywordConst)) {
                auto& constDecl = begin<ast::ConstantDecl>();
                constDecl.annotations = std::move(annotations);

                EXPECT(constDecl.type);
                EXPECT(constDecl.name);
                EXPECT(TokenType::Equal);
                EXPECT(constDecl.value);
                EXPECT(TokenType::SemiColon);

                continue;
            }

            // parse unions
            if ((config & AllowTypes) != 0 && consume(TokenType::KeywordUnion)) {
                auto& unionDecl = begin<ast::UnionDecl>();

                unionDecl.annotations = std::move(annotations);
                EXPECT(unionDecl.name);

                EXPECT(TokenType::LeftBrace);
                while (!consume(TokenType::RightBrace)) {
                    auto& member = unionDecl.members.emplace_back();

                    while (match(TokenType::LeftBracket))
                        EXPECT(member.annotations);

                    EXPECT(member.type);
                    EXPECT(member.name);
                    EXPECT(TokenType::SemiColon);
                }

                continue;
            }

            // parse struct declarations
            if ((config & AllowTypes) != 0 && consume(TokenType::KeywordStruct)) {
                auto& structDecl = begin<ast::StructDecl>();

                structDecl.annotations = std::move(annotations);
                EXPECT(structDecl.name);

                if (consume(TokenType::Colon))
                    EXPECT(structDecl.baseType);

                if (!consume(TokenType::SemiColon)) {
                    EXPECT(TokenType::LeftBrace);
                    while (!consume(TokenType::RightBrace)) {
                        auto& field = structDecl.fields.emplace_back();

                        while (match(TokenType::LeftBracket))
                            EXPECT(field.annotations);

                        EXPECT(field.type);
                        EXPECT(field.name);
                        if (consume(TokenType::Equal)) {
                            ast::Literal lit;
                            EXPECT(lit);
                            field.init = std::move(lit);
                        }
                        EXPECT(TokenType::SemiColon);
                    }
                }

                continue;
            }

            // parse enumerations
            if ((config & AllowTypes) != 0 && consume(TokenType::KeywordEnum)) {
                auto& enumDecl = begin<ast::EnumDecl>();

                enumDecl.annotations = std::move(annotations);
                EXPECT(enumDecl.name);

                if (consume(TokenType::Colon))
                    EXPECT(enumDecl.baseType);

                EXPECT(TokenType::LeftBrace);
                long long nextValue = 0;
                for (;;) {
                    auto& item = enumDecl.items.emplace_back();
                    EXPECT(item.name);
                    item.value = nextValue++;
                    if (consume(TokenType::Equal)) {
                        EXPECT(item.value);
                        nextValue = item.value + 1;
                    }
                    if (!consume(TokenType::Comma))
                        break;
                }
                EXPECT(TokenType::RightBrace);

                continue;
            }

            return fail("unexpected ", tokens[next]);
        }

        return true;
    }

    bool Grammar::match(TokenType type) {
        if (next >= tokens.size())
            return false;

        if (tokens[next].type != type)
            return false;

        return true;
    }

    bool Grammar::consume(TokenType type) {
        if (!match(type))
            return false;

        ++next;
        return true;
    }

    Location Grammar::pos() {
        auto const& tokPos = next > 0 ? tokens[next - 1].pos : tokens.front().pos;
        return Location{ filename, { tokPos.line, tokPos.column } };
    }

    template <typename... T>
    bool Grammar::fail(std::string message, T const&... args) {
        auto const& tok = next < tokens.size() ? tokens[next] : tokens.back();
        Location const loc{ filename, { tok.pos.line, tok.pos.column } };
        return log.error(loc, message, args...);
    };

    bool Grammar::mustConsume(TokenType type) {
        if (consume(type))
            return true;

        std::ostringstream buf;
        buf << "expected " << type;
        if (next > 0)
            buf << " after " << tokens[next - 1];
        if (next < tokens.size())
            buf << ", got " << tokens[next];
        return fail(buf.str());
    }

    bool Grammar::mustConsume(long long& out) {
        auto const index = next;
        if (consume(TokenType::Number)) {
            out = tokens[index].dataNumber;
            return true;
        }

        std::ostringstream buf;
        buf << "expected number";
        if (next > 0)
            buf << " after " << tokens[next - 1];
        if (next < tokens.size())
            buf << ", got " << tokens[next];
        return fail(buf.str());
    }

    bool Grammar::mustConsume(ast::Literal& out) {
        if (consume(TokenType::KeywordNull)) {
            out.loc = pos();
            out.data = nullptr;
        }
        else if (consume(TokenType::KeywordFalse)) {
            out.loc = pos();
            out.data = false;
        }
        else if (consume(TokenType::KeywordTrue)) {
            out.loc = pos();
            out.data = true;
        }
        else if (consume(TokenType::String)) {
            out.loc = pos();
            out.data = tokens[next - 1].dataString;
        }
        else if (consume(TokenType::Number)) {
            out.loc = pos();
            out.data = tokens[next - 1].dataNumber;
        }
        else if (match(TokenType::Identifier)) {
            ast::QualifiedId id;
            EXPECT(id);
            out.loc = id.components.front().loc;
            out.data = std::move(id);
        }
        else if (consume(TokenType::LeftBrace)) {
            out.loc = pos();
            std::vector<ast::Literal> values;
            if (!consume(TokenType::RightBrace)) {
                for (;;) {
                    EXPECT(values.emplace_back());
                    if (!consume(TokenType::Comma))
                        break;
                }
                EXPECT(TokenType::RightBrace);
            }
            out.data = std::move(values);
        }
        else {
            std::ostringstream buf;
            buf << "expected literal";
            if (next > 0)
                buf << " after " << tokens[next - 1];
            if (next < tokens.size())
                buf << ", got " << tokens[next];
            return fail(buf.str());
        }
        return true;
    }

    bool Grammar::mustConsume(ast::Identifier& out) {
        auto const index = next;
        if (!consume(TokenType::Identifier))
            return false;

        out.id = tokens[index].dataString;
        out.loc = pos();
        return true;
    }

    bool Grammar::mustConsume(ast::QualifiedId& id) {
        id.components.clear();
        EXPECT(id.components.emplace_back());

        while (consume(TokenType::Dot)) {
            EXPECT(id.components.emplace_back());
        }

        return true;
    }

    bool Grammar::mustConsume(std::unique_ptr<ast::TypeRef>& type) {
        if (consume(TokenType::KeywordTypename)) {
            auto tref = std::make_unique<ast::TypeRef>();
            tref->kind = ast::TypeRef::Kind::TypeName;
            tref->loc = pos();
            type = std::move(tref);
            return true;
        }

        {
            auto name = std::make_unique<ast::TypeRef>();
            name->kind = ast::TypeRef::Kind::Name;
            EXPECT(name->name);
            name->loc = name->name.components.front().loc;
            name->loc.merge(name->name.components.back().loc);
            type = std::move(name);
        }

        if (consume(TokenType::Asterisk)) {
            auto ptr = std::make_unique<ast::TypeRef>();
            ptr->kind = ast::TypeRef::Kind::Pointer;
            ptr->loc = type->loc;
            ptr->loc.merge(pos());
            ptr->ref = std::move(type);
            type = std::move(ptr);
        }

        if (consume(TokenType::LeftBracket)) {
            auto arr = std::make_unique<ast::TypeRef>();
            arr->kind = ast::TypeRef::Kind::Array;
            arr->loc = type->loc;
            arr->loc.merge(pos());
            arr->ref = std::move(type);
            type = std::move(arr);
            EXPECT(TokenType::RightBracket);
        }
        return true;
    }

    bool Grammar::mustConsume(std::vector<ast::Annotation>& annotations) {
        EXPECT(TokenType::LeftBracket);

        do { // comma-separated annotations
            auto& annotation = annotations.emplace_back();
            EXPECT(annotation.name);

            if (consume(TokenType::LeftParen)) {
                if (!consume(TokenType::RightParen)) {
                    for (;;) {
                        auto& value = annotation.args.emplace_back();
                        EXPECT(value);
                        if (!consume(TokenType::Comma))
                            break;

                    }
                    EXPECT(TokenType::RightParen);
                }
            }
        } while (consume(TokenType::Comma));

        EXPECT(TokenType::RightBracket);

        return true;
    }

    template <typename DeclT>
    DeclT& Grammar::begin() {
        assert(!scopeStack.empty());

        auto decl = std::make_unique<DeclT>();
        auto* const ret = decl.get();

        scopeStack.back()->push_back(move(decl));

        return *ret;
    }
}
