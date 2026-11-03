// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "ui/world12_table.hpp"
#include "ui/world_cup_statistics.hpp"
#include "ui/friendly_match.hpp"
namespace bmh::ui {
struct World12FriendlyPicker {
    uint64_t sequence=0,revision=0;uint8_t canonical_actor=0;
    bool national=false;
    std::optional<World12TableState> table;
    std::optional<WorldCupStatisticsProjection> nations;
    std::array<bmh::world12::ClubId,62> nation_ids;
};
WorldCupStatisticsProjection world12_friendly_nations(const bmh::world12::CareerSnapshot&,bmh::world12::ClubId own,const GameState& reference,const LeImage& actual_source,std::array<bmh::world12::ClubId,62>& countries);
}
