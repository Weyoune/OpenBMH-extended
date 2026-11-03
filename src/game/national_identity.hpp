// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/state.hpp"
#include <algorithm>

namespace bmh {
// Runtime identity only: an empty projection retains Original NoTeam semantics.
inline bool is_national_team(const GameState& state,int team) {
    if(state.projected_national_teams.empty())return team==kNoTeam;
    return std::any_of(state.projected_national_teams.begin(),state.projected_national_teams.end(),[&](const auto& entry){return entry.team==team;});
}
inline bool has_national_squad(const GameState& state,int team) {
    if(state.projected_national_teams.empty())return team==kNoTeam;
    const auto entry=std::find_if(state.projected_national_teams.begin(),state.projected_national_teams.end(),[&](const auto& value){return value.team==team;});
    return entry!=state.projected_national_teams.end()&&entry->squad_first.has_value();
}
inline SquadRange squad_range(const GameState& state,int team) {
    if(!state.projected_national_teams.empty()){
        const auto entry=std::find_if(state.projected_national_teams.begin(),state.projected_national_teams.end(),[&](const auto& value){return value.team==team;});
        if(entry!=state.projected_national_teams.end())return entry->squad_first?SquadRange{*entry->squad_first,24}:SquadRange{};
    }
    return squad_range(team);
}
}
