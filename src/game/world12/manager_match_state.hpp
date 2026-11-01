// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/world.hpp"
#include "game/state.hpp"
namespace bmh::world12 {
// Explicit local maps belong to the workspace owner. Only known match fields
// are transferred; names, guest pointers and unrelated manager fields stay out.
ManagerMatchState manager_match_state_from_legacy(const GameState&,int manager,
    const std::vector<ClubId>& clubs,const std::vector<PlayerId>& players,
    const ManagerMatchState* previous=nullptr);
void install_manager_match_state(GameState&,int manager,const ManagerMatchState&,
    const std::vector<ClubId>& clubs,const std::vector<PlayerId>& players);
}
