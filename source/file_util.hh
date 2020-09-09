// sapc - by Sean Middleditch
// This is free and unencumbered software released into the public domain.
// See LICENSE.md for more details.

#pragma once

#include <sstream>
#include <fstream>

namespace sapc {
    inline bool loadText(std::filesystem::path const& filename, std::string& out_text) {
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
}
