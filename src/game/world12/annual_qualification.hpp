// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/save.hpp"
#include "game/weekly.hpp"

namespace bmh::world12 {
struct AnnualCupFinal {
    ClubId winner,runner_up;
    bool operator==(const AnnualCupFinal&)const=default;
};
struct AnnualDomesticTitle {
    Country country=Country::germany;
    ClubId champion;
    AnnualCupFinal cup;
    bool operator==(const AnnualDomesticTitle&)const=default;
};
// European ownership only. National fields and their random phase have a
// separate owner; no season-zero national selection is reused here.
struct AnnualEuropeanFields {
    std::array<std::vector<ClubId>,3> qualified,fields;
    std::array<ClubId,3> holders;
    bool operator==(const AnnualEuropeanFields&)const=default;
};
struct AnnualLeagueQualification {
    LeagueId league;
    AnnualCupFinal domestic;
    uint8_t uefa_places=0;
    bool source_random_domestic=false;
    bool operator==(const AnnualLeagueQualification&)const=default;
};
struct AnnualQualificationCorrection {
    LeagueId league;
    ClubId club;
    bool reserve_pool=false;
    bool operator==(const AnnualQualificationCorrection&)const=default;
};
struct UnpublishedAnnualQualification {
    // Exact paid-awards source, before accounts, membership or player resets.
    CareerSnapshot source_binding;
    uint32_t source_season=0,target_season=0;
    std::array<std::vector<ClubId>,kLeagueSlots> final_rankings;
    std::vector<AnnualDomesticTitle> domestic_titles;
    std::array<AnnualCupFinal,3> european_titles;
    // Source primary/Spain/England/France/Italy order; disabled primary rows
    // follow the existing shared-career quota redistribution.
    std::vector<AnnualLeagueQualification> league_qualification;
    AnnualEuropeanFields next_european;
    // Only an otherwise unrepresentable Source CW deficit is repaired, after
    // every ordinary qualification. Never changes a valid Source selection.
    std::vector<AnnualQualificationCorrection> corrections;
    int32_t game_seed_before=0,game_seed_after=0;
    uint32_t watcom_seed_before=0,watcom_seed_after=0;
    uint64_t game_draws=0,watcom_draws=0;
    bool operator==(const UnpublishedAnnualQualification&)const=default;
};
// Require all actual final standings, completed domestic and European owners,
// and the paid annual-awards boundary. Both random streams remain staged.
UnpublishedAnnualQualification stage_annual_qualification(const CareerSnapshot&);
// Rebuilds the full packet against the unchanged source, including draw results.
void validate_annual_qualification(const CareerSnapshot&,const UnpublishedAnnualQualification&);
// Country-scoped scalar relations for Original ticket/popularity/account code.
// Qualification is NEXT season; championship/cup titles are the completed year.
ClubAttractionTitles annual_club_relations(const UnpublishedAnnualQualification&,ClubId);
// Source zero-based table position, not the one-based displayed rank.
uint8_t annual_final_rank(const UnpublishedAnnualQualification&,ClubId);
}
