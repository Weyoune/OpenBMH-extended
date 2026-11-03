// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/annual_archive_types.hpp"
#include "cli/json.hpp"
namespace bmh::world12 {
cli::Json encode_annual_title_archive(const std::optional<AnnualTitleArchive>&);
std::optional<AnnualTitleArchive> decode_annual_title_archive(const cli::Json&);
}
