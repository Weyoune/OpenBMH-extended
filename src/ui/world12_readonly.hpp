// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/save.hpp"
#include "ui/desk_state.hpp"
#include "ui/desk_records.hpp"
#include "ui/calendar_state.hpp"
#include "ui/indoor_standings.hpp"
#include "ui/achievements.hpp"
#include "ui/top_scorers_state.hpp"
namespace bmh::ui {
struct World12ReadonlyIdentity {
    uint8_t manager=0,menu=0;
    bmh::world12::ClubId club;
    uint32_t date=0;
    uint64_t revision=0,scope=0,sequence=0;
    std::vector<World12CareerIdentity> career_roster;
    bool operator==(const World12ReadonlyIdentity&)const=default;
};
struct World12ReadonlySelection {
    TopScorersMetric metric=TopScorersMetric::goals;
    TopScorersScope scope=TopScorersScope::league;
    bool operator==(const World12ReadonlySelection&)const=default;
};
enum class World12ReadonlyAction:uint8_t {close,set_metric,toggle_scope};
struct World12ReadonlyRequest {
    World12ReadonlyIdentity identity;
    World12ReadonlyAction action=World12ReadonlyAction::close;
    TopScorersMetric metric=TopScorersMetric::goals;
    bool operator==(const World12ReadonlyRequest&)const=default;
};
struct World12ReadonlyState {
    World12ReadonlyIdentity identity;
    World12ReadonlySelection selection;
    bool decorated_background=true;
    std::optional<TopScorersState> top_scorers;
    std::optional<DeskRecordsState> records;
    std::optional<AchievementsState> achievements;
    std::optional<CalendarState> calendar;
    std::optional<IndoorStandingsState> indoor;
    std::vector<bmh::world12::ClubId> clubs;
    std::vector<bmh::world12::PlayerId> players;
};
AchievementsState world12_achievements(const bmh::world12::CareerSnapshot&,
    const World12ReadonlyIdentity&,const GameState& reference,const LeImage&,const Image& underlay);
IndoorStandingsState world12_indoor_standings(const bmh::world12::CareerSnapshot&,
    const World12ReadonlyIdentity&,const GameState& reference,const LeImage&,const TextDb&);
CalendarState world12_calendar_state(const bmh::world12::CareerSnapshot&,const LeImage&,const TextDb&);
World12ReadonlyState world12_readonly_state(const bmh::world12::CareerSnapshot&,
    const World12ReadonlyIdentity&,const World12ReadonlySelection&,const GameState& reference,
    const LeImage& source,const Image& underlay={},const HighscoreTable& highscore_rows={});
}
