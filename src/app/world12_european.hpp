// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "app/world12_ai_day.hpp"
#include "app/world12_cups.hpp"
#include "game/world12/european_match.hpp"
#include "game/world12/indoor_match.hpp"
namespace bmh::app::world12 {
struct EuropeanDayConditions {
    bmh::world12::EuropeanGroup group;
    std::vector<bmh::world12::AiLeagueConditions> matches;
};
struct IndoorDayConditions {bmh::world12::IndoorGroup group;std::vector<bmh::world12::AiLeagueConditions> matches;};
struct CompetitionDayOutcome {
    std::vector<bmh::world12::AiLeagueRoundOutcome> leagues;
    std::vector<bmh::world12::CupRoundOutcome> cups,european,indoor;
    bmh::world12::DayPresentation presentation;
    std::vector<bmh::world12::PoolsMatchOutcome> pools;
    uint64_t game_draws=0,watcom_draws=0;
};
// Runs the exact scheduled league/domestic/European groups, then publishes every
// result, draw and both random streams with the calendar cursor in one revision.
// Callers retain explicit live presenters; this owner never changes their policy.
CompetitionDayOutcome play_competition_day(bmh::world12::CareerSnapshot&,ManagerActionOwner&,
    ManagerActionOwner expected,const bmh::world12::DayTicket&,const GameState&,
    const CupSources&,const std::vector<AiDayLeagueConditions>&,
    const std::vector<CupDayConditions>&,const std::vector<EuropeanDayConditions>&,
    bmh::world12::HumanProjectionPolicy,const bmh::world12::HumanRoundHooks&,const std::vector<IndoorDayConditions>& indoor={});
}
