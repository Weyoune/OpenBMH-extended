// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/annual_players.hpp"
#include "game/world12/cup_draw.hpp"
namespace bmh::world12 {
struct AnnualFieldsStage {
    World world;
    std::vector<KnockoutStage> cups;
    std::vector<CupDrawNotice> draws;
    int32_t game_seed=0;uint32_t watcom_seed=0;
    uint64_t game_draws=0,watcom_draws=0;
};
// After actual accounts, market/exchanges and pending sanctions. Completed
// physical-namespace owners are retained in the outer receipt before this
// quiescent workspace; new indoor/regional/calendar owners follow player reset.
// Cup selection uses actual post-exchange Source slots; recruitment history
// separately follows stable clubs via annual_player_ranks.
AnnualFieldsStage stage_annual_fields(const CareerSnapshot& post_exchanges,
    const UnpublishedAnnualQualification&,const LeImage& german,const LeImage& english);
}
