// sapc - by Sean Middleditch
// This is free and unencumbered software released into the public domain.
// See LICENSE.md for more details.

#include "analyze.hh"
#include "model.hh"
#include "location.hh"

#include <sstream>

namespace sapc {
    bool analyze(Module& module, std::vector<std::string>& errors) {
        bool valid = true;

        auto error = [&](Location const& loc, std::string error, auto const&... args) {
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

            if (type.kind == Type::Kind::Enum)
                continue;

            for (auto& field : type.fields) {
                if (field.type.isTypeName) {
                    if (type.kind != Type::Kind::Attribute)
                        error(field.location, "illegal field `", field.type, "', only attributes may have `typename' fields");

                    continue;
                }

                auto typeIt = module.typeMap.find(field.type.type);
                if (typeIt == module.typeMap.end()) {
                    error(field.location, "unknown type `", field.type, '\'');
                    continue;
                }
                auto const& fieldType = module.types[typeIt->second];

                if (fieldType.kind == Type::Kind::Enum && field.init.type != Value::Type::None) {
                    if (field.init.type != Value::Type::Enum) {
                        error(field.location, "enumeration type `", field.type, "' may only be initialized by enumerants");
                        continue;
                    }

                    auto findEnumerant = [&](std::string const& name) -> TypeField const* {
                        for (auto const& value : fieldType.fields) {
                            if (value.name == name)
                                return &value;
                        }
                        return nullptr;
                    };

                    auto const* enumValue = findEnumerant(field.init.dataName);
                    if (enumValue == nullptr) {
                        error(field.init.location, "enumerant `", field.init.dataName, "' not found in enumeration '", fieldType.name, '\'');
                        error(fieldType.location, "enumeration `", fieldType.name, "' defined here");
                        continue;
                    }

                    field.init.type = Value::Type::Enum;
                    field.init.dataName = enumValue->name;
                    field.init.dataString = fieldType.name;
                    field.init.dataNumber = enumValue->init.dataNumber;
                }
                else if (field.init.type == Value::Type::Enum)
                    error(field.init.location, "only enumeration types can be initialized by enumerants");
            }
        }

        // expand attribute arguments
        auto expand = [&](Annotation& annotation) {
            auto const it = module.typeMap.find(annotation.name);
            if (it == module.typeMap.end()) {
                error(annotation.location, "unknown attribute `", annotation.name, '\'');
                return;
            }

            auto const argc = annotation.arguments.size();
            auto const& attrType = module.types[it->second];
            auto const& params = attrType.fields;

            if (attrType.kind != Type::Kind::Attribute) {
                error(annotation.location, "attribute type `", annotation.name, "' is declared as a regular type (not attribute) at ", attrType.location);
                return;
            }

            if (argc > params.size()) {
                error(annotation.location, "too many arguments to attribute `", annotation.name, '\'');
                return;
            }

            for (size_t index = 0; index != argc; ++index) {
                auto const& param = params[index];
                auto const& arg = annotation.arguments[index];

                if (param.type.isTypeName) {
                    if (arg.type != Value::Type::TypeName) {
                        error(arg.location, "typename field `", param.name, "' requires initialization to a type");
                        continue;
                    }

                    auto typeIt = module.typeMap.find(arg.dataString);
                    if (typeIt == module.typeMap.end()) {
                        error(arg.location, "unknown type `", arg.dataString, '\'');
                        continue;
                    }
                }
            }

            for (size_t index = argc; index != params.size(); ++index) {
                auto const& param = params[index];

                if (param.init.type == Value::Type::None)
                    error(param.init.location, "missing required argument `", param.name, "' to attribute `", annotation.name, '\'');
                else
                    annotation.arguments.push_back(param.init);
            }
        };

        for (auto& type : module.types) {
            for (auto& annotation : type.annotations)
                expand(annotation);

            for (auto& field : type.fields) {
                for (auto& annotation : field.annotations)
                    expand(annotation);
            }
        }

        return valid;
    }
}
