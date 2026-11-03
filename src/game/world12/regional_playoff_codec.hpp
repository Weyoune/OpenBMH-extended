// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/regional_playoff.hpp"
#include "cli/json.hpp"
namespace bmh::world12 {
cli::Json encode_regional_playoff(const std::optional<RegionalPlayoffSeason>&);
std::optional<RegionalPlayoffSeason> decode_regional_playoff(const cli::Json&);
}
