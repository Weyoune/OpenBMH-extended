// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/cup_match.hpp"
#include "game/world12/friendlies.hpp"
namespace bmh::world12 {
struct FriendlyRoundOutcome: CupRoundOutcome {std::vector<std::optional<uint8_t>> kickoff_dispositions;};
FriendlyRoundOutcome simulate_and_settle_friendly_group(CareerSnapshot&,const GameState&,const FriendlyGroup&,const LeImage& actual_source,const std::vector<AiLeagueConditions>&,HumanProjectionPolicy,GameRandom&,WatcomRandom&,const HumanRoundHooks& hooks={});
}
