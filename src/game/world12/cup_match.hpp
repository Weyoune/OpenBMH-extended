// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/ai_match.hpp"
#include "game/world12/cups.hpp"
namespace bmh::world12 {
struct CupRoundOutcome {
    std::vector<uint16_t> scores;
    std::vector<uint8_t> end_minutes;
    uint64_t game_draws=0,watcom_draws=0;
    std::vector<PoolsMatchOutcome> pools;
    bool operator==(const CupRoundOutcome&)const=default;
};
// Actual shared Core phases, including extra time/shootouts, player development,
// morale and manager records. Gate income/premiums belong to the event owner.
// The country cup owner subsequently publishes these words/draws to its stage.
// All participants currently require league rosters; European reserve/national
// placeholders need their explicit abstract-team projection, not guessed players.
CupRoundOutcome simulate_and_settle_domestic_cup_round(CareerSnapshot&,
    const GameState& reference,const KnockoutStage&,const LeImage& country_rules,
    const std::vector<AiLeagueConditions>&,HumanProjectionPolicy,
    GameRandom&,WatcomRandom&,const HumanRoundHooks& hooks={});
}
