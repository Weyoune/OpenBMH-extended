// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/annual_accounts.hpp"
#include "game/world12/season_transition.hpp"
namespace bmh::world12 {
struct AnnualPlayerRenewalStage {
    CareerSnapshot source_binding;
    World world;
    std::optional<DesktopRuntime> desktop;
    std::optional<TransferRuntime> transfers;
    SeasonResults results;
    std::array<uint8_t,kLeagueSlots> league_rounds{};
    int32_t game_seed=0;uint32_t watcom_seed=0;
    uint64_t game_draws=0,watcom_draws=0;
    size_t visited=0,released=0;
    // National provenance stays outside active club finance/market ownership.
    // Its distinct Source refresh owner must finish before final publication.
    bool inactive_origin_refresh_required=false;
};
// Retained Source position history follows a real club through league swaps;
// at the amateur edge the newcomer inherits the former league slot's history.
// Validate every exchange receipt and the final shuffled membership permutation.
std::map<ClubId,uint8_t> annual_player_ranks(const World& before_players,
    const UnpublishedAnnualQualification&,const std::vector<AnnualPromotionOutcome>&);
// Complete active-roster/free-pool annual reset and ageing pass. The caller has
// already settled accounts/market, exchanges, cup fields and fixture shuffle.
// Historical competition receipts remain owned outside this quiescent staged
// snapshot. No calendar/date/season is published by this World-only result.
AnnualPlayerRenewalStage stage_annual_player_renewal(const CareerSnapshot& before_players,
    const UnpublishedAnnualQualification&,const std::vector<AnnualPromotionOutcome>&,
    const GameState& reference,const TextDb&);
// Source SeasonEndSteps' history shift/start marker, after player renewal and
// before highscore/length/first-Wednesday services. Unknown old origin is needed
// only when the fifty-season window actually shifts.
void begin_annual_manager_history(World&,uint32_t target_season);
}
