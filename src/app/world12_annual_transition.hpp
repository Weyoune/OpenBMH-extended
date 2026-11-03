// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "app/world12_manager.hpp"
#include "game/world12/annual_accounts.hpp"
#include "game/world12/season_transition.hpp"
#include "game/world12/highscores.hpp"
#include "game/world12/market_auctions.hpp"
#include "game/world12/national_campaign.hpp"
#include "game/world12/cups.hpp"
#include "formats/le.hpp"
namespace bmh::app::world12 {
struct AnnualPromotionNotice {bmh::world12::Country country;size_t group=0;const bmh::world12::AnnualPromotionGroup* result=nullptr;};
struct AnnualTransitionHooks {
    bmh::world12::AnnualAccountsHooks tax;
    bmh::world12::MarketAuctionHooks market;
    std::function<bool(const bmh::world12::UnpublishedAnnualQualification&)> qualification;
    std::function<bool(const AnnualPromotionNotice&)> promotion;
    std::function<bool(const bmh::world12::CupDrawNotice&)> cup_draw;
    std::function<bool(const bmh::world12::NationalCampaign&)> national_draw;
    std::function<bool(const bmh::world12::OwnedHighscoreProjection&,bool final_display)> highscore;
};
struct AnnualTransitionOutcome {
    bool career_complete=false;
    uint64_t game_draws=0,watcom_draws=0;
    size_t retired=0,academy=0;
    bmh::world12::OwnedHighscoreProjection highscore;
};
// The actual closing desks must have committed annual_transition. All numerical
// phases stage against one immutable source; missing real presenters/unknown
// carry reject. Ranked score output is atomically replaced only at the final
// publication boundary. Session/Main own the following opening-desk entry.
AnnualTransitionOutcome publish_annual_transition(bmh::world12::CareerSnapshot&,
    ManagerActionOwner&,ManagerActionOwner expected,const GameState& reference,
    const LeImage& german,const LeImage& english,const TextDb&,
    const std::filesystem::path& configured_highscore_file,const AnnualTransitionHooks&);
}
