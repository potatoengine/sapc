// sapc - by Sean Middleditch
// This is free and unencumbered software released into the public domain.
// See LICENSE.md for more details.

#include "log.hh"
#include "schema.hh"
#include "validate.hh"

#include <cassert>
#include <filesystem>
#include <unordered_map>

namespace fs = std::filesystem;

namespace sapc {
    namespace {
        struct Validator {
            Log& log;

            inline bool validate(schema::Module const& mod);
            inline bool validate(schema::Type const& type);
            inline bool validate(schema::TypeAggregate const& type);
        };
    }
}

bool sapc::validate(schema::Module const& mod, Log& log) {
    Validator validator{ log };
    return validator.validate(mod);
}

bool sapc::Validator::validate(schema::Module const& mod) {
    bool success = true;

    // validate that the module has a name declaration
    if (mod.name.empty()) {
        log.error(mod.location, "module name is missing");
        success = false;
    }

    // module name should be the same as the filename
    const fs::path basename = mod.location.filename.stem();
    if (basename != fs::path{ mod.name })
        log.warn(mod.location, "module name `", mod.name, "' does not match filename");

    // validate all types
    for (auto const* type : mod.root->types)
        success |= validate(*type);

    return success;
}

bool sapc::Validator::validate(schema::Type const& type) {
    switch (type.kind) {
    case schema::Type::Kind::Struct:
    case schema::Type::Kind::Attribute:
    case schema::Type::Kind::Union:
        return validate(static_cast<schema::TypeAggregate const&>(type));
    default: return true;
    }
}

bool sapc::Validator::validate(schema::TypeAggregate const& type) {
    bool success = true;

    // field names should be unique
    {
        std::unordered_map<std::string_view, schema::Field const*> fields;
        for (auto const& field : type.fields) {
            assert(!field->name.empty());
            assert(field->type != nullptr);

            auto const rs = fields.insert({ field->name, field.get() });
            if (!rs.second) {
                log.error(field->location, "duplicate field `", field->name, "' in type `", type.name, "'");
                log.info(rs.first->second->location, "first declaration of field `", field->name, "'");
                success = false;
            }
        }
    }

    return success;
}
