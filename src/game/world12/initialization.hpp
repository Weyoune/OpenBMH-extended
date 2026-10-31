// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/world.hpp"
#include "game/random.hpp"
namespace bmh {class LeImage;}
namespace bmh::world12 {
using InitialStrengthRanges=std::array<std::array<uint8_t,2>,kLeagueSlots>;
InitialStrengthRanges read_initial_strength_ranges(const Catalogue&,const LeImage& german,const LeImage& english);
struct InitialPlayerGroup {
    LeagueId league;
    uint8_t rank=0;
    std::vector<PlayerId> players;
};
struct PlayerInitializationPlan {
    GameScope scope;
    InitialStrengthRanges strengths;
    std::vector<PlayerId> extra_players;
    std::vector<InitialPlayerGroup> groups;
};
// Derive before filtering the immutable master. National-only play retains the
// whole player pool, while club scopes discard excluded domestic squads.
PlayerInitializationPlan make_player_initialization_plan(const World& master,GameScope,
    const InitialStrengthRanges&);
struct PlayerInitializationOutcome {uint64_t game_draws=0,watcom_draws=0;size_t players=0;};
PlayerInitializationOutcome initialize_career_players(World&,const PlayerInitializationPlan&,
    GameRandom&,WatcomRandom&);
}
