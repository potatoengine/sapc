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

#include <algorithm>
#include <cassert>
#include <unordered_map>
#include <utility>

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
        constexpr unsigned AllowCustom = 1 << 6;

        constexpr unsigned ConfigModule = AllowNamespaces | AllowTypes | AllowModule | AllowAttributes | AllowImport | AllowConstants | AllowCustom;
        constexpr unsigned ConfigNamespace = AllowNamespaces | AllowTypes | AllowConstants;

        struct Grammar {
            std::vector<Token> const& tokens;
            Log& log;
            fs::path const& filename;
            ParserImportModuleCb const& importCallback;
            ast::ModuleUnit& module;
            size_t next = 0;
            std::vector<std::vector<std::unique_ptr<ast::Declaration>>*> scopeStack;
            std::unordered_map<std::string_view, ast::CustomTagDecl const*> customTags;
            std::vector<ast::Annotation> annotations;

            inline Location pos();

            template <typename... T>
            bool fail(std::string message, T const&... args);

            inline bool match(TokenType type);

            inline bool consume(TokenType type);
            inline bool consume(std::initializer_list<TokenType> select);

            inline bool mustConsume(TokenType type);
            inline bool mustConsume(std::initializer_list<TokenType> select);
            inline bool mustConsume(long long& out);
            inline bool mustConsume(ast::Identifier& out);
            inline bool mustConsume(ast::QualifiedId& out);
            inline bool mustConsume(ast::Literal& out);
            inline bool mustConsume(std::unique_ptr<ast::TypeRef>& type);
            inline bool mustConsume(std::vector<ast::Annotation>& annotations);

            inline bool parseFile();
            inline bool parseScope(Location const& start, TokenType terminate, unsigned config);
            inline bool parseStruct(std::string_view customTag = {});
            inline bool parseUnion(std::string_view customTag = {});
            inline bool parseAlias(std::string_view customTag = {});
            inline bool parseEnum(std::string_view customTag = {});
            inline bool parseConstant(std::string_view customTag = {});
            inline bool parseCustom(std::string_view tag);

            inline bool hasCustomTag(std::string_view tag, ast::Declaration::Kind kind) const;
            inline void processCustomTag(ast::CustomTagDecl const& customDecl);

            template <typename DeclT>
            DeclT& begin();
        };
    }

    std::unique_ptr<ast::ModuleUnit> parse(std::filesystem::path const& filename, ParserImportModuleCb const& importCb, Log& log) {
        assert(!filename.empty());
        assert(importCb);

        std::string contents;
        if (!loadText(filename, contents)) {
            log.error({ filename }, "failed to open input");
            return nullptr;
        }

        std::vector<Token> tokens;
        if (!tokenize(contents, filename, tokens, log))
            return nullptr;

        auto mod = std::make_unique<ast::ModuleUnit>();
        mod->filename = filename;

        Grammar grammar{ tokens, log, filename, importCb, *mod };
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

    bool Grammar::parseScope(Location const& start, TokenType terminate, unsigned config) {
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
                auto& impDecl = begin<ast::ImportDecl>();

                EXPECT(impDecl.target);
                EXPECT(TokenType::SemiColon);

                // Parse the imported module, which we need in order to find custom decls
                auto const* const imported = importCallback(impDecl.target, filename);
                if (imported != nullptr)
                    for (auto const& decl : imported->decls)
                        if (decl->kind == ast::Declaration::Kind::CustomTag)
                            processCustomTag(*static_cast<ast::CustomTagDecl const*>(decl.get()));

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

            // parse custom tags
            if ((config & AllowCustom) != 0 && consume(TokenType::KeywordUse)) {
                auto& customTagDecl = begin<ast::CustomTagDecl>();
                customTagDecl.annotations = std::move(annotations);

                EXPECT(customTagDecl.name);
                EXPECT(TokenType::Colon);

                EXPECT((std::initializer_list<TokenType>{
                    TokenType::KeywordStruct,
                        TokenType::KeywordEnum,
                        TokenType::KeywordUnion,
                        TokenType::KeywordUsing,
                        TokenType::KeywordConst,
                }));

                TokenType const type = tokens[next - 1].type;

                if (type == TokenType::KeywordStruct)
                    customTagDecl.tagKind = ast::Declaration::Kind::Struct;
                else if (type == TokenType::KeywordEnum)
                    customTagDecl.tagKind = ast::Declaration::Kind::Enum;
                else if (type == TokenType::KeywordUnion)
                    customTagDecl.tagKind = ast::Declaration::Kind::Union;
                else if (type == TokenType::KeywordUsing)
                    customTagDecl.tagKind = ast::Declaration::Kind::Alias;
                else if (type == TokenType::KeywordConst)
                    customTagDecl.tagKind = ast::Declaration::Kind::Constant;

                EXPECT(TokenType::SemiColon);

                processCustomTag(customTagDecl);

                continue;
            }

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
                if (!parseConstant())
                    return false;
                continue;
            }

            // parse unions
            if ((config & AllowTypes) != 0 && consume(TokenType::KeywordUnion)) {
                if (!parseUnion())
                    return false;
                continue;
            }

            // parse alias declarations
            if ((config & AllowTypes) != 0 && consume(TokenType::KeywordUsing)) {
                if (!parseAlias())
                    return false;
                continue;
            }

            // parse struct declarations
            if ((config & AllowTypes) != 0 && consume(TokenType::KeywordStruct)) {
                if (!parseStruct())
                    return false;
                continue;
            }

            // parse enumerations
            if ((config & AllowTypes) != 0 && consume(TokenType::KeywordEnum)) {
                if (!parseEnum())
                    return false;
                continue;
            }

            // parse custom tags
            if (consume(TokenType::Identifier)) {
                auto const tag = tokens[next - 1].dataString;
                if (!parseCustom(tag))
                    return false;

                continue;
            }

            return fail("unexpected ", tokens[next]);
        }

        return true;
    }

    bool Grammar::parseStruct(std::string_view customTag) {
        auto& structDecl = begin<ast::StructDecl>();
        structDecl.customTag = customTag;

        structDecl.annotations = std::move(annotations);
        EXPECT(structDecl.name);

        if (consume(TokenType::LeftAngle)) {
            EXPECT(structDecl.typeParams.emplace_back());

            while (consume(TokenType::Comma))
                EXPECT(structDecl.typeParams.emplace_back());

            EXPECT(TokenType::RightAngle);
        }

        if (consume(TokenType::Colon))
            EXPECT(structDecl.baseType);

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

        return true;
    }

    bool Grammar::parseUnion(std::string_view customTag) {
        auto& unionDecl = begin<ast::UnionDecl>();
        unionDecl.customTag = customTag;

        unionDecl.annotations = std::move(annotations);
        EXPECT(unionDecl.name);

        if (consume(TokenType::LeftAngle)) {
            EXPECT(unionDecl.typeParams.emplace_back());

            while (consume(TokenType::Comma))
                EXPECT(unionDecl.typeParams.emplace_back());

            EXPECT(TokenType::RightAngle);
        }

        EXPECT(TokenType::LeftBrace);
        while (!consume(TokenType::RightBrace)) {
            auto& field = unionDecl.fields.emplace_back();

            while (match(TokenType::LeftBracket))
                EXPECT(field.annotations);

            EXPECT(field.type);
            EXPECT(field.name);
            EXPECT(TokenType::SemiColon);
        }

        return true;
    }

    bool Grammar::parseAlias(std::string_view customTag) {
        auto& aliasDecl = begin<ast::AliasDecl>();
        aliasDecl.customTag = customTag;

        aliasDecl.annotations = std::move(annotations);
        EXPECT(aliasDecl.name);

        if (consume(TokenType::Equal))
            EXPECT(aliasDecl.targetType);

        EXPECT(TokenType::SemiColon);

        return true;
    }

    bool Grammar::parseEnum(std::string_view customTag) {
        auto& enumDecl = begin<ast::EnumDecl>();
        enumDecl.customTag = customTag;

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

        return true;
    }

    bool Grammar::parseConstant(std::string_view customTag) {
        auto& constDecl = begin<ast::ConstantDecl>();
        constDecl.customTag = customTag;

        constDecl.annotations = std::move(annotations);

        EXPECT(constDecl.type);
        EXPECT(constDecl.name);
        EXPECT(TokenType::Equal);
        EXPECT(constDecl.value);
        EXPECT(TokenType::SemiColon);

        return true;
    }


    bool Grammar::parseCustom(std::string_view tag) {
        if (hasCustomTag(tag, ast::Declaration::Kind::Struct))
            return parseStruct(tag);
        else if (hasCustomTag(tag, ast::Declaration::Kind::Enum))
            return parseEnum(tag);
        else if (hasCustomTag(tag, ast::Declaration::Kind::Union))
            return parseUnion(tag);
        else if (hasCustomTag(tag, ast::Declaration::Kind::Alias))
            return parseAlias(tag);
        else if (hasCustomTag(tag, ast::Declaration::Kind::Constant))
            return parseConstant(tag);

        return fail("unexpected identifier `", tag, '`');
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

    bool Grammar::consume(std::initializer_list<TokenType> select) {
        for (auto const tok : select)
            if (consume(tok))
                return true;
        return false;
    }

    Location Grammar::pos() {
        auto const& tokPos = next > 0 ? tokens[next - 1].pos : tokens.front().pos;
        return Location{ filename, { tokPos.line, tokPos.column } };
    }

    template <typename... T>
    bool Grammar::fail(std::string message, T const&... args) {
        auto const& tok = next < tokens.size() ? tokens[next] : tokens.back();
        Location const loc{ filename, tok.start, tok.end };
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

    bool Grammar::mustConsume(std::initializer_list<TokenType> select) {
        if (consume(select))
            return true;

        std::ostringstream buf;
        buf << "expected ";
        if (select.size() > 1)
            buf << "one of ";
        for (auto const tok : select)
            buf << tok << ' ';
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
        if (!consume(TokenType::Identifier)) {
            std::ostringstream buf;
            buf << "expected identifier";
            if (next > 0)
                buf << " after " << tokens[next - 1];
            if (next < tokens.size())
                buf << ", got " << tokens[next];
            return fail(buf.str());
            return false;
        }

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
        }
        else
        {
            auto name = std::make_unique<ast::TypeRef>();
            name->kind = ast::TypeRef::Kind::Name;
            EXPECT(name->name);
            name->loc = name->name.components.front().loc;
            if (name->name.components.size() > 1)
                name->loc.merge(name->name.components.back().loc);
            type = std::move(name);

            if (consume(TokenType::LeftAngle)) {
                auto gen = std::make_unique<ast::TypeRef>();
                gen->kind = ast::TypeRef::Kind::Generic;
                gen->loc = type->loc;
                EXPECT(gen->typeArgs.emplace_back());
                while (consume(TokenType::Comma))
                    EXPECT(gen->typeArgs.emplace_back());
                EXPECT(TokenType::RightAngle);
                gen->loc.merge(pos());
                gen->ref = std::move(type);
                type = std::move(gen);
            }
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
            arr->ref = std::move(type);

            if (match(TokenType::Number)) {
                long long arraySize = 0;
                EXPECT(arraySize);
                arr->arraySize = arraySize;
            }

            EXPECT(TokenType::RightBracket);
            arr->loc.merge(pos());
            type = std::move(arr);
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

    bool Grammar::hasCustomTag(std::string_view tag, ast::Declaration::Kind kind) const {
        auto const it = customTags.find(tag);
        return it != end(customTags) && it->second->tagKind == kind;
    }

    void Grammar::processCustomTag(ast::CustomTagDecl const& customDecl) {
        customTags.insert({ customDecl.name.id, &customDecl });
    }
}
