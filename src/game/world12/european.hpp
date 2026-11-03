// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/world.hpp"
#include "game/random.hpp"
namespace bmh::world12 {
// Season-zero Source selection/draw. Club identities remain global and the
// discarded Native scratch tail is consumed without inventing world entities.
InitialCompetitionFields draw_initial_shared_fields(const World&,GameRandom&,WatcomRandom&,std::vector<uint16_t>* inactive_national_tokens=nullptr);
}
