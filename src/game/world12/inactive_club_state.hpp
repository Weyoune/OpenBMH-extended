// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/world.hpp"
#include "game/random.hpp"
#include "formats/le.hpp"
#include <utility>
namespace bmh {struct GameState;}
namespace bmh::world12 {
struct InactiveClubStartupOutcome {
    std::vector<uint16_t> initial_national_field;
    uint64_t game_draws=0,watcom_draws=0;
};
// Own Source setup draws against the actual full retained population. It creates
// no club game, finance, human-club assignment or resolved trophy fact. The
// national owner installs this already-drawn four field without another draw.
InactiveClubStartupOutcome initialize_inactive_club_state(World&,const World& actual_master,const LeImage& german,
    const LeImage& english,const GameState& reference,GameRandom&,WatcomRandom&);
// The actual national owner has already drawn target_season. Only the inactive
// club fields advance here. No daily/weekly club business runs in this scope.
std::pair<uint64_t,uint64_t> advance_inactive_club_state(World&,uint32_t target_season,
    const LeImage& german,const LeImage& english,GameRandom&,WatcomRandom&);
void validate_inactive_club_state(const World&);
}
