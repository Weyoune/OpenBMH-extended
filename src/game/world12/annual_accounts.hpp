// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/annual_qualification.hpp"
#include "game/season.hpp"
#include "formats/textd.hpp"
namespace bmh::world12 {
struct AnnualTaxEvent {
    uint8_t manager=0;ClubId club;
    // The numeric manager slot is the canonical seat. The packed local team is
    // cleared; club above owns the real identity.
    SeasonTaxEvent source;
    bool operator==(const AnnualTaxEvent&)const=default;
};
struct AnnualTaxDecision {
    AnnualTaxEvent question;bool adjust=false;
    bool operator==(const AnnualTaxDecision&)const=default;
};
struct AnnualAccountsHooks {
    std::function<bool(const AnnualTaxEvent&)> present;
    std::function<std::optional<bool>(const AnnualTaxEvent&)> choose;
};
struct AnnualAcademyRecruit {
    uint8_t manager=0;PlayerId player;ClubId donor,recipient;
    bool operator==(const AnnualAcademyRecruit&)const=default;
};
struct UnpublishedAnnualAccounts {
    UnpublishedAnnualQualification qualification;
    CareerSnapshot snapshot;
    std::array<uint8_t,4> pending_sanctions{};
    std::vector<AnnualTaxEvent> tax_events;
    std::vector<AnnualTaxDecision> tax_decisions;
    std::vector<AnnualAcademyRecruit> academy;
    int32_t carried_fine=0;
    uint64_t game_draws=0,watcom_draws=0;
    bool operator==(const UnpublishedAnnualAccounts&)const=default;
};
// Complete Source per-manager accounts, youth settlement, bonuses, academy,
// credit, tax/refund and tickets in canonical manager order. Decisions happen
// against staged state and require real presenters. Membership, player ageing,
// sanctions, date/season and calendar publication belong to the outer pass.
UnpublishedAnnualAccounts stage_annual_accounts(const UnpublishedAnnualQualification&,
    const GameState& reference,const TextDb&,const AnnualAccountsHooks&);
// Replay the recorded choices against the exact qualification binding; neither
// the human presenters nor the caller's random streams are consumed again.
void validate_annual_accounts(const UnpublishedAnnualAccounts&,const GameState&,const TextDb&);
}
