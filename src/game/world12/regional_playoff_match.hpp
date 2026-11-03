// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/regional_playoff.hpp"
#include "game/world12/cup_match.hpp"
namespace bmh::world12 {
// Actual Source descriptor14, common minute clock and post-match settlement.
// Result/date bookkeeping remains owned by the outer day transaction.
CupRoundOutcome simulate_and_settle_regional_playoff_group(CareerSnapshot&,const GameState&,
    const RegionalPlayoffGroup&,const LeImage&,const std::vector<AiLeagueConditions>&,
    HumanProjectionPolicy,GameRandom&,WatcomRandom&,const HumanRoundHooks& hooks={});
}
