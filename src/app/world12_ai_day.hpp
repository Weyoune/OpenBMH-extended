// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/ai_match.hpp"
#include "game/world12/progression.hpp"
namespace bmh::app::world12 {
struct AiDayOwner {
    uint64_t scope=0,revision=0;
    bool operator==(const AiDayOwner&)const=default;
};
struct AiDayIdentity {uint64_t scope=0,revision=0;};
struct AiDayLeagueConditions {
    bmh::world12::LeagueAppointment appointment;
    std::vector<bmh::world12::AiLeagueConditions> matches;
};
struct AiDayOutcome {
    std::vector<bmh::world12::AiLeagueRoundOutcome> leagues;
    uint64_t game_draws=0,watcom_draws=0;
};
// Internal staged-day calculation shared by both calendar owners. Complete
// human leagues run together first; remaining AI leagues follow ticket order.
// The owning day transaction validates the ticket and publishes atomically.
std::vector<bmh::world12::AiLeagueRoundOutcome> simulate_scheduled_leagues(
    bmh::world12::CareerSnapshot&,const bmh::GameState&,
    const std::vector<AiDayLeagueConditions>&,const bmh::world12::HumanProjectionPolicy*,
    bmh::GameRandom&,bmh::WatcomRandom&,const bmh::world12::HumanRoundHooks*);
// League-only match/calendar transaction. Conditions are authoritative engine
// inputs, not UI proposals. Human fixtures and every non-league appointment
// require their separate owners. This does not run daily/weekly finance.
AiDayOutcome play_ai_league_day(bmh::world12::CareerSnapshot&,AiDayOwner&,
    const AiDayIdentity&,const bmh::world12::DayTicket&,const bmh::GameState& rules,
    const std::vector<AiDayLeagueConditions>&);
// Same complete league-day transaction, with explicit human policy/presenters.
// It still rejects cup/national/indoor dates and does not perform daily economy.
AiDayOutcome play_league_day(bmh::world12::CareerSnapshot&,AiDayOwner&,
    const AiDayIdentity&,const bmh::world12::DayTicket&,const bmh::GameState& rules,
    const std::vector<AiDayLeagueConditions>&,bmh::world12::HumanProjectionPolicy,
    const bmh::world12::HumanRoundHooks&);
}
