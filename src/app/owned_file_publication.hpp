// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "formats/binary.hpp"
#include <optional>
#include <vector>
namespace bmh::app::world12 {
// Absence is a verified nonexistent target, not an unknown prior file.
struct OwnedFilePublication {
    std::filesystem::path path;
    std::optional<Bytes> expected;
    Bytes bytes;
};
// Stage and verify each replacement; compensate earlier replacements if a later
// write fails. Never restore over a separately changed external file.
void publish_owned_files(const std::vector<OwnedFilePublication>&);
}
