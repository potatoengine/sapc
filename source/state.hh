// sapc - by Sean Middleditch
// This is free and unencumbered software released into the public domain.
// See LICENSE.md for more details.

#pragma once

#include "context.h"

#include <string>
#include <vector>
#include <variant>
#include <memory>
#include <iosfwd>
#include <sstream>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <unordered_map>

namespace sapc {
    struct None {
        constexpr operator reID() const noexcept { return { reTypeNone, -1 }; }
        constexpr operator reLoc() const noexcept { return { 0, 0 }; }
    };

    constexpr None reNone;

    constexpr bool operator==(reID a, reID b) noexcept { return a.value == b.value; }
    constexpr bool operator!=(reID a, reID b) noexcept { return a.value != b.value; }

    struct Location {
        std::filesystem::path filename;
        int line = 0;
        int column = 0;

        bool operator==(Location const& rhs) const { return filename == rhs.filename && line == rhs.line && column == rhs.column; }
    };

    struct Type;

    struct Attribute {
        std::string name;
        std::vector<reID> arguments;
        Location loc;
        Type* type = nullptr;
    };

    struct FieldType {
        std::string typeName;
        bool isPointer = false;
        bool isArray = false;

        friend std::ostream& operator<<(std::ostream& os, FieldType const& type);
    };

    struct Field {
        FieldType type;
        std::string name;
        reID init = reNone;
        std::vector<Attribute> attributes;
        Location loc;
    };

    struct Type {
        std::string name;
        std::string base;
        std::vector<Attribute> attributes;
        std::vector<Field> fields;
        Location loc;
        bool attribute = false;
        bool imported = false;
    };

    struct Error {
        Location loc;
        std::string message;

        friend std::ostream& operator<<(std::ostream& os, Error const& error);
    };

    struct StringTable {
        std::vector<std::string> strings;

        reID pushString(std::string&& str, reType type = reTypeString) {
            auto it = find(begin(strings), end(strings), str);
            if (it != end(strings))
                return reID{ type, static_cast<int>(it - begin(strings)) };

            auto const index = strings.size();
            strings.push_back(std::move(str));
            return reID{ type, static_cast<int>(index) };
        }
    };

    struct ParseState {
        StringTable& strings;
        std::vector<std::filesystem::path> search;
        std::vector<std::unique_ptr<Type>> types;

        std::vector<Field> fieldStack;
        std::vector<Attribute> attributeStack;
        std::vector<Field> attributeParamStack;
        std::vector<std::vector<Attribute>> attributeSetStack;
        std::vector<reID> argumentStack;
        std::vector<std::filesystem::path> pathStack;

        std::vector<std::string> imports;
        std::vector<std::filesystem::path> fileDependencies;

        std::vector<Error> errors;

        std::string moduleName;

        std::unordered_map<std::string, size_t> typeMap;

        nlohmann::json to_json();

        std::filesystem::path resolveInclude(std::filesystem::path filename);
        std::filesystem::path resolveModule(std::string const& name);

        bool compile(std::filesystem::path filename);
        bool compile(std::string contents, std::filesystem::path filename);

        bool importModule(std::string module_name, reLoc loc);
        bool includeFile(std::filesystem::path filename, reLoc loc);

        void addDependency(std::filesystem::path filename);
        void addImport(std::string module_name);

        bool build(std::string contents);
        bool parse(std::string contents);
        bool checkTypes();
        bool expandAttributes();

        Location location(reLoc loc) const {
            return Location{ pathStack.back(), loc.line, static_cast<int>(loc.position - loc.line_start) };
        }

        bool failed() const noexcept { return !errors.empty(); }

        template <typename... ArgsT>
        void error(Location loc, ArgsT const&... args) {
            std::ostringstream buffer;
            (buffer << ... << args);
            errors.push_back(Error{ std::move(loc), std::move(buffer).str() });
        }

        template <typename... ArgsT>
        void error(reLoc loc, ArgsT const&... args) {
            error(location(loc), args...);
        }
    };
}

struct reParseState : sapc::ParseState {};
