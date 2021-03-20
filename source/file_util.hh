// sapc - by Sean Middleditch
// This is free and unencumbered software released into the public domain.
// See LICENSE.md for more details.

#pragma once

#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

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

    inline std::filesystem::path resolveFile(std::filesystem::path target, std::filesystem::path const& base, std::vector<std::filesystem::path> const& search) {
        if (target.is_absolute())
            return target;

        if (!base.empty()) {
            auto tmp = base / target;
            if (std::filesystem::exists(tmp))
                return tmp;
        }

        for (auto const& path : search) {
            auto tmp = path / target;
            if (std::filesystem::exists(tmp))
                return tmp;
        }

        return {};
    }
}
