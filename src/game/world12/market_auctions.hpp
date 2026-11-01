// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/match_projection.hpp"
#include "game/world12/save.hpp"
#include "game/weekly.hpp"
#include "formats/textd.hpp"
namespace bmh::world12 {
// Shared numeric loan/history workspace, qualified by canonical identities.
void assign_transfer_loan_slots(CareerSnapshot&);
void install_transfer_loans(AiMatchProjection&,const CareerSnapshot&);
void capture_transfer_history(CareerSnapshot&,const AiMatchProjection&);
void capture_created_loan(CareerSnapshot&,const AiMatchProjection&,PlayerId,ClubId owner,ClubId borrower);
struct MarketAuctionHooks {
    std::function<std::optional<int>(const GameState&,const TransferOfferListView&,const AiMatchProjection&)> select;
    std::function<void(const GameState&,const TransferDecisionEvent&,const AiMatchProjection&)> notice;
    std::optional<uint8_t> active_seat;
    TransferAllocatorContext allocator=TransferAllocatorContext::unsupported;
};
struct MarketAuctionOutcome {uint64_t game_draws=0,watcom_draws=0;size_t processed=0,resolved=0,moved=0,loans=0;};
// Countdown and settlement only. Missing human presenters reject the staged
// phase. No new bids, monthly renewal or date advancement happens here.
MarketAuctionOutcome process_market_auctions(CareerSnapshot&,const GameState& reference,
    const TextDb&,const MarketAuctionHooks&,bool season_end=false);
}
