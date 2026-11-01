// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "app/world12_manager.hpp"
#include "game/world12/daily_services.hpp"
#include "game/world12/progression.hpp"
namespace bmh::app::world12 {
struct ClockHooks {
    bmh::world12::DailyServiceHooks services;
    // Called only for an actual weekly boundary, on the staged current date.
    std::function<bmh::world12::WeeklyIncidentContext(const bmh::world12::CareerSnapshot&)> relations;
};
struct ClockStepOutcome {
    bmh::world12::ProgressPhase phase=bmh::world12::ProgressPhase::daily_services;
    uint64_t game_draws=0,watcom_draws=0;
    bool date_advanced=false;
};
// Finish the owned initial empty event. Finance remains explicitly pending in
// the saved progress record; no earlier date or random stream is replayed.
void finish_initial_event(bmh::world12::CareerSnapshot&,ManagerActionOwner&,ManagerActionOwner expected,const GameState&,CalendarEpoch);
ClockStepOutcome advance_clock_step(bmh::world12::CareerSnapshot&,ManagerActionOwner&,ManagerActionOwner expected,
    const GameState&,const TextDb&,const ClockHooks&);
// Atomic complete search: cannot skip a scheduled competition or annual desk.
ClockStepOutcome seek_next_event(bmh::world12::CareerSnapshot&,ManagerActionOwner&,ManagerActionOwner expected,
    const GameState&,const TextDb&,const ClockHooks&);
}
