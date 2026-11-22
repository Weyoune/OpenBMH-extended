// SPDX-License-Identifier: GPL-3.0-only
// Almanac recording: what OpenBMH-Extended writes down when a season is closed, so that later seasons
// can look it up (final tables, best scorers, the club of every human manager).
// The original game keeps none of this. The data lives in World::extended (extended_features.hpp).
// OpenBMH deviates from the original: the final table and the five best scorers of every league are kept for every completed season, together with the club each human manager led.
#pragma once
#include "game/world12/save.hpp"
namespace bmh::world12 {
// Final table and the best scorers of every active league of the completed season state.season.
// The state must be the closing state: every league round played, scorer counters not yet reset.
// The table uses the same ranking (with point deductions of human managers) as the title archive,
// so the first row is always the archived champion. Nothing is changed and no random number is drawn.
AlmanacSeason stage_almanac_season(const CareerSnapshot& state);
// Appends the season to the almanac of the world and notes the club of every human club manager.
// `managers` is the manager list of the closing state (a manager may have another club afterwards).
// A recording that does not continue the existing almanac without a gap starts a new almanac.
void record_almanac_season(World& world,AlmanacSeason season,const std::vector<Manager>& managers,const World& closing);
}
