// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/national_coach.hpp"
#include "game/world12/save.hpp"
#include <algorithm>
namespace bmh::ui {
// The current projected national role supersedes a parked club, while the full
// saved person roster remains the authentication identity across both roles.
inline bool world12_view_actor(const bmh::world12::CareerSnapshot& state,uint8_t seat,bmh::world12::ClubId team){
    const auto actors=bmh::world12::national_desktop_actors(state);
    if(!actors.empty())return std::any_of(actors.begin(),actors.end(),[&](const auto& actor){return actor.seat==seat&&actor.team==team;});
    return std::any_of(state.world.managers.begin(),state.world.managers.end(),[&](const auto& actor){return actor.seat==seat&&actor.club==team;});
}
}
