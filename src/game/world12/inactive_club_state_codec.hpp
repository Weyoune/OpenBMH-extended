// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/inactive_club_state_types.hpp"
#include "cli/json.hpp"
#include <optional>
namespace bmh::world12 {
cli::Json encode_inactive_club_state(const std::optional<InactiveClubState>&);
std::optional<InactiveClubState> decode_inactive_club_state(const cli::Json&);
}
