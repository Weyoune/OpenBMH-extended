// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/save.hpp"
#include "game/state.hpp"
#include "formats/textd.hpp"
namespace bmh::world12 {
enum class AnnualAwardKind:uint8_t { goals,scorer,fair_play };
struct AnnualAward {
    AnnualAwardKind kind=AnnualAwardKind::goals;
    LeagueId league;
    ClubId club;
    PlayerId player; // Fair-play and empty scoring tables have no player.
    uint8_t goals=0,assists=0;
    std::optional<uint8_t> manager;
    int32_t prize=0;
    bool operator==(const AnnualAward&)const=default;
};
// Source kind-first/catalogue draw-slot order across both enabled countries.
// Winners come from actual player counters and club discipline, never forecasts.
std::vector<AnnualAward> calculate_annual_awards(const World&);
struct AnnualAwardsStage {
    World world;
    std::vector<AnnualAward> awards;
    uint32_t completed_seasons=0;
};
// Unpublished annual boundary stage. No date, random stream, roster, archive or
// qualification is changed. The outer owner must persist its exactly-once
// completion fact and own the closing desk before continuing the annual pass.
AnnualAwardsStage stage_annual_awards(const CareerSnapshot&,const GameState& reference,
    const TextDb&);
}
