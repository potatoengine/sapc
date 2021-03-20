// sapc - by Sean Middleditch
// This is free and unencumbered software released into the public domain.
// See LICENSE.md for more details.

#pragma once

namespace sapc {
    struct Log;
    namespace schema {
        struct Module;
    }

    bool validate(schema::Module const& mod, Log& log);
}
