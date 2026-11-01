// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/match_projection.hpp"
#include "game/world12/save.hpp"
namespace bmh::world12 {
// Bounded local IDs are drawing/action coordinates only. The projection retains
// the explicit mapping back to global clubs, players and manager seats.
AiMatchProjection project_manager_desktop(const CareerSnapshot&,const GameState& reference);
}
