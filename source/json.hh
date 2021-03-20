// sapc - by Sean Middleditch
// This is free and unencumbered software released into the public domain.
// See LICENSE.md for more details.

#pragma once

#include "location.hh"
#include "schema.hh"

#include <nlohmann/json.hpp>

namespace sapc {
    nlohmann::ordered_json serializeToJson(schema::Module const& mod);
}
