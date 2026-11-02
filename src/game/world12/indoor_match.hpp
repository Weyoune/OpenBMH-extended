// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/indoor.hpp"
#include "game/world12/cup_match.hpp"
namespace bmh::world12 {
struct IndoorMatchOutcome {std::vector<CupRoundOutcome> groups;uint64_t game_draws=0,watcom_draws=0;};
IndoorMatchOutcome simulate_and_settle_indoor_pass(CareerSnapshot&,const GameState&,
    const std::vector<IndoorGroup>&,const std::vector<std::vector<AiLeagueConditions>>&,
    HumanProjectionPolicy,GameRandom&,WatcomRandom&,const HumanRoundHooks& hooks={});
}
