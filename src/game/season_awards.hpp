// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>
namespace bmh {
struct SeasonScoringEntry {uint8_t goals=0,assists=0;};
// Input order is actual runtime club, then squad slot. Only positive scores
// enter the Source top ten; equal scores retain that traversal order.
std::vector<size_t> rank_season_scorers(std::span<const SeasonScoringEntry>,uint8_t flags);
// Preserve Source's signed high-byte discipline arithmetic and Games tie order.
size_t season_fairplay_winner(std::span<const uint16_t> discipline);
// Original prizes in monetary units, indexed by country-local tier/kind0..2.
int32_t season_award_prize(int local_league,int kind);
}
