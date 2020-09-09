// sapc - by Sean Middleditch
// This is free and unencumbered software released into the public domain.
// See LICENSE.md for more details.

#include "parser.hh"
#include "model.hh"
#include "lexer.hh"
#include "grammar.hh"

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
        if (!parse(tokens, search, errors, dependencies, out_module))
            return false;
        return analyze(out_module);
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
