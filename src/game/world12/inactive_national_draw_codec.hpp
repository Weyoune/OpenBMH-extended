// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/inactive_national_draw.hpp"
#include "cli/json.hpp"
namespace bmh::world12 {
cli::Json encode_inactive_national_draw(const std::optional<InactiveNationalDraw>&);
std::optional<InactiveNationalDraw> decode_inactive_national_draw(const cli::Json&);
}
