// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/standings.hpp"

namespace bmh::world12 {
// These are inputs from the annual licence and competition owners, not a
// replacement for their financial decisions, messages or random consumption.
struct AnnualLicenceDecision {
    LeagueId league;
    std::vector<ClubId> withdrawn; // Source decision order; current group occupants.
    bool operator==(const AnnualLicenceDecision&)const=default;
};
enum class RegionalPromotion:uint8_t { not_applicable,champions_only,playoff_complete };
struct AnnualPromotionDecisions {
    Country country=Country::germany;
    std::vector<AnnualLicenceDecision> licences; // Exactly one per country league.
    RegionalPromotion regional=RegionalPromotion::not_applicable;
    // Frozen draw-slot identities in Source field order. The first winner
    // precedes champion A even when that winner belongs to Regionalliga B.
    std::array<ClubId,2> regional_winners;
    // Already drawn, distinct, named non-league clubs, in original draw order.
    // Even a protected lowest-league manager consumes its paired newcomer draw.
    std::vector<ClubId> newcomers;
    bool operator==(const AnnualPromotionDecisions&)const=default;
};
struct AnnualExchange {
    ClubId relegated,promoted;
    LeagueId upper,lower; // Invalid lower means the non-league/amateur edge.
    bool protected_manager=false;
    bool operator==(const AnnualExchange&)const=default;
};
struct AnnualPromotionGroup {
    std::vector<ClubId> relegated,promoted;
    size_t count=0;
    std::vector<AnnualExchange> exchanges;
    bool operator==(const AnnualPromotionGroup&)const=default;
};
struct AnnualPromotionPlan {
    AnnualPromotionDecisions decisions;
    SeasonResults source_results;
    std::array<uint8_t,kLeagueSlots> source_rounds{};
    std::array<LeagueMembers,kLeagueSlots> before,after;
    std::array<std::vector<StandingRow>,kLeagueSlots> standings;
    std::vector<AnnualPromotionGroup> groups;
    // Applying this edge also requires Original's league-slot squad lineage and
    // annual player renewal. A membership plan alone cannot publish a World.
    bool requires_amateur_roster_renewal=false;
};
// No state, date or random stream is changed. All country results and round
// cursors must be complete, including the Extended English fifth division.
AnnualPromotionPlan plan_annual_promotions(const World&,const SeasonResults&,
    const std::array<uint8_t,kLeagueSlots>& rounds,const AnnualPromotionDecisions&);
// Rebuilds against the current input; changed results, decisions, membership or
// manager protection cannot reuse a stale/tampered plan.
void validate_annual_promotion_plan(const World&,const SeasonResults&,
    const std::array<uint8_t,kLeagueSlots>& rounds,const AnnualPromotionPlan&);
}
