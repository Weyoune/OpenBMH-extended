// SPDX-License-Identifier: GPL-3.0-only
#include "game/world12/national_annual.hpp"
#include "game/turn.hpp"
#include <algorithm>
namespace bmh::world12 {
bool national_annual_ready(const CareerSnapshot& state){
    GameState date;date.date=state.date;
    if(state.world.catalogue.scope.club_play()||!state.world.catalogue.scope.international||!state.world.settings||!state.world.settings->duration||!state.progress||(state.progress->phase!=ProgressPhase::season_end&&state.progress->phase!=ProgressPhase::annual_transition)||!is_season_end(date)||!state.calendar||state.prepared_event||!state.desktop||state.desktop->active_seat||state.desktop->initial_event||state.desktop->autosave_pending||state.world.managers.size()!=1||state.desktop->manager_order!=std::vector<uint8_t>{state.world.managers.front().seat}||!state.world.highscore_signature||!state.world.economy||!state.world.markets||!state.world.free_players||!state.world.national_origins||!state.world.national_coaches||!state.world.inactive_club_state||state.world.inactive_club_state->season!=state.season||!state.world.national_campaigns||!state.world.national_campaigns->seasons.contains(state.season)||!state.world.national_campaigns->seasons.at(state.season).complete||!state.world.annual_awards_completed||(*state.world.annual_awards_completed!=state.season&&uint64_t(*state.world.annual_awards_completed)!=uint64_t(state.season)+1))return false;
    const auto& actor=state.world.managers.front();const auto role=std::find_if(state.world.national_coaches->begin(),state.world.national_coaches->end(),[&](const auto& role){return role.team==actor.club;});
    if(role==state.world.national_coaches->end()||!role->startup||!role->active||role->coach!=actor.seat||std::none_of(state.world.national_squads.begin(),state.world.national_squads.end(),[&](const auto& squad){return squad.team==actor.club&&squad.coach==actor.seat;}))return false;
    return std::all_of(state.world.national_origins->begin(),state.world.national_origins->end(),[](const auto& origin){return origin.source_initialized;})&&std::all_of(state.league_rounds.begin(),state.league_rounds.end(),[](auto round){return !round;});
}
}
