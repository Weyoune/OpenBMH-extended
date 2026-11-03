// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <filesystem>
#include <string>

namespace bmh::launch {
struct ExtendedDataCheck {
    bool recognized=false;
    std::filesystem::path directory;
    // The manifest's published fact, independent of a development override.
    bool playable=false;
    std::string error;
};
// Probe a directory or the parent of a named OBD/EXE. ZIP/7z extraction remains
// owned by resolve_game; probe its resolved directory before Classic startup.
// A recognized package with an error must never fall back to Classic handling.
ExtendedDataCheck inspect_extended_data(const std::filesystem::path& input,
                                       bool allow_development=false);
}
