// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/cup_match.hpp"
#include "game/world12/national_campaign.hpp"
namespace bmh::world12 {
struct NationalMatchBatchOutcome {
    std::vector<CupRoundOutcome> groups;
    uint64_t game_draws=0,watcom_draws=0;
};
// The complete saved parallel programme shares the actual Source match clock.
// Calendar admission, attendance, campaign advance and coach awards stay owned
// by the enclosing event transaction.
NationalMatchBatchOutcome simulate_and_settle_national_groups(CareerSnapshot&,const GameState&,
    const std::vector<NationalGroup>&,const LeImage&,const std::vector<std::vector<AiLeagueConditions>>&,
    HumanProjectionPolicy,GameRandom&,WatcomRandom&,const HumanRoundHooks& hooks={});
}
