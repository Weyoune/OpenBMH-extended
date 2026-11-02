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

#include "game/world12/save.hpp"
#include "game/season_exchange.hpp"
#include "formats/textd.hpp"
namespace bmh::world12 {
struct AnnualAmateurPool {
    Country country=Country::germany;
    std::vector<ClubId> slots;
    bool operator==(const AnnualAmateurPool&)const=default;
};
// Only the immutable, undeveloped master can establish the initial physical
// order. Later annual passes must retain the preceding stage's returned order.
AnnualAmateurPool initial_annual_amateur_pool(const World&,Country);
struct AnnualDecisionContext {
    AnnualAmateurPool amateurs;
    RegionalPromotion regional=RegionalPromotion::not_applicable;
    std::array<ClubId,2> regional_winners;
};
struct AnnualLicenceNotice {
    ClubId club;
    std::optional<uint8_t> manager; // Absent means the Source general AI message.
    LeagueId league;
    bool operator==(const AnnualLicenceNotice&)const=default;
};
struct AnnualPromotionOutcome {
    World source_world;
    AnnualPromotionPlan plan;
    AnnualAmateurPool pool_before,pool_after;
    std::vector<AnnualLicenceNotice> licence_notices;
    uint32_t date=0,season=0,watcom_seed_before=0,watcom_seed_after=0;
    int32_t game_seed_before=0,game_seed_after=0;
    uint64_t game_draws=0,watcom_draws=0;
};
// Real post-account licence evaluation and newcomer draws, in Source group
// order. Seeds remain staged in the returned outcome. No human choice exists
// in this phase; annual accounts/transfer decisions must precede this service.
AnnualPromotionOutcome evaluate_annual_promotion_decisions(const CareerSnapshot&,
    const AnnualDecisionContext&);
SeasonLicenceAccount annual_licence_account(const CareerSnapshot&,uint8_t seat);
struct AnnualMembershipStage {
    World world;
    AnnualAmateurPool amateurs;
    int32_t game_seed=0;
    uint32_t watcom_seed=0;
};
// Build an unpublished canonical World: exact roster inheritance, Source loan
// party lineage, stable club/statistics identities, promotion flags and mail.
// This is not a Session commit or a saved next season. Final awards/accounts,
// qualification, player renewal, resets and calendar publication remain owned
// prerequisites of the outer annual transaction.
AnnualMembershipStage stage_annual_membership(const CareerSnapshot&,
    const AnnualPromotionOutcome&,const TextDb&);
}
