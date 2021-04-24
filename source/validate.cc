// sapc - by Sean Middleditch
// This is free and unencumbered software released into the public domain.
// See LICENSE.md for more details.

#include "log.hh"
#include "schema.hh"
#include "validate.hh"

#include <filesystem>

namespace fs = std::filesystem;

namespace sapc {
    namespace {
        struct Validator {
            schema::Module const& mod;
            Log& log;

            inline bool validate();
        };
    }
}

bool sapc::validate(schema::Module const& mod, Log& log) {
    Validator validator{ mod, log };
    return validator.validate();
}

bool sapc::Validator::validate() {
    // module name should be the same as the filename
    if (mod.name.empty()) {
        log.error(mod.location, "module name is missing");
        return false;
    }

    const fs::path basename = mod.location.filename.stem();
    if (basename != fs::path{ mod.name })
        log.warn(mod.location, "module name `", mod.name, "' does not match filename");

    return true;
}
