// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/save.hpp"
#include <memory>
#include "ui/desk_state.hpp"
#include "ui/balance_plan.hpp"
#include "ui/desk_records.hpp"
#include "ui/calendar_state.hpp"
#include "ui/calendar.hpp"
#include "ui/indoor_standings.hpp"
#include "ui/achievements.hpp"
#include "ui/top_scorers_state.hpp"
namespace bmh::ui {
struct World12StatisticsState;
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
    int statistics_page=-1,prediction_mode=1,title_category=0,title_first_season=0,alltime_nation=0;
    bool alltime_relative=false;
    std::optional<BalancePlanDraft> balance_draft;
    bool balance_pending_close=false;
    std::optional<bmh::world12::ManagerBalancePlan> balance_closed_plan;
    bool operator==(const World12ReadonlySelection&)const=default;
};
enum class World12ReadonlyAction:uint8_t {close,set_metric,toggle_scope,close_child,select_statistics_page,set_prediction,set_statistics_flags,select_title_category,select_alltime_nation,toggle_alltime_relative,balance,reschedule};
struct World12ReadonlyRequest {
    World12ReadonlyIdentity identity;
    World12ReadonlyAction action=World12ReadonlyAction::close;
    TopScorersMetric metric=TopScorersMetric::goals;
    int value=-1,first_season=0;
    uint8_t expected_flags=0,target_flags=0;
    std::optional<BalanceRequest> balance_request;
    std::optional<RescheduleAction> reschedule_request;
    bool operator==(const World12ReadonlyRequest&)const=default;
};
struct World12ReadonlyState {
    World12ReadonlyIdentity identity;
    World12ReadonlySelection selection;
    bool decorated_background=true;
    std::shared_ptr<World12StatisticsState> statistics;
    std::optional<TopScorersState> top_scorers;
    std::optional<DeskRecordsState> records;
    std::optional<AchievementsState> achievements;
    std::optional<CalendarState> calendar;
    std::optional<IndoorStandingsState> indoor;
    std::optional<BalanceState> balance;
    std::vector<bmh::world12::ClubId> clubs;
    std::vector<bmh::world12::PlayerId> players;
};
AchievementsState world12_achievements(const bmh::world12::CareerSnapshot&,
    const World12ReadonlyIdentity&,const GameState& reference,const LeImage&,const Image& underlay);
IndoorStandingsState world12_indoor_standings(const bmh::world12::CareerSnapshot&,
    const World12ReadonlyIdentity&,const GameState& reference,const LeImage&,const TextDb&);
CalendarState world12_calendar_state(const bmh::world12::CareerSnapshot&,const LeImage&,const TextDb&);
CalendarState world12_reschedule_calendar_state(const bmh::world12::CareerSnapshot&,uint8_t seat,uint64_t revision,const LeImage&,const TextDb&);
World12ReadonlyState world12_readonly_state(const bmh::world12::CareerSnapshot&,
    const World12ReadonlyIdentity&,const World12ReadonlySelection&,const GameState& reference,
    const LeImage& source,const Image& underlay={},const HighscoreTable& highscore_rows={});
}
