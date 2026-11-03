// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/progression.hpp"
namespace bmh::world12 {
// Current manager history's Source one-based domestic ladder encoding. German
// parallel Regionalligen share one18-club band; English tiers keep20-club bands.
uint8_t league_history_position(const League&,uint8_t zero_based_rank);
// Called on the day owner's staged real results, before advancing its date.
// Unknown older history stays unknown; every known eligible record is written
// once through the shared Core helpers using a bounded real24-player workspace.
void record_completed_league_history(CareerSnapshot& staged,const CareerSnapshot& before,
    const std::vector<CompletedLeagueRound>&);
}
