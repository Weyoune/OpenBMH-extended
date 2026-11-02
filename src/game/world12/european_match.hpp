// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/cup_match.hpp"
#include "game/world12/european_campaign.hpp"
namespace bmh::world12 {
struct EuropeanMatchBatchOutcome {std::vector<CupRoundOutcome> groups;uint64_t game_draws=0,watcom_draws=0;};
EuropeanMatchBatchOutcome simulate_and_settle_european_groups(CareerSnapshot&,const GameState&,
    const std::vector<EuropeanGroup>&,const LeImage&,const std::vector<std::vector<AiLeagueConditions>>&,
    HumanProjectionPolicy,GameRandom&,WatcomRandom&,const HumanRoundHooks& hooks={});
// Actual Original minute phases and post-match publication, including rating-only
// foreign teams. Calendar, draw, awards and gate owners remain explicit callers.
CupRoundOutcome simulate_and_settle_european_group(CareerSnapshot&,const GameState&,
    const EuropeanGroup&,const LeImage&,const std::vector<AiLeagueConditions>&,
    HumanProjectionPolicy,GameRandom&,WatcomRandom&,const HumanRoundHooks& hooks={});
}
