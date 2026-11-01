// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/match_projection.hpp"
#include "game/world12/save.hpp"
#include "game/weekly.hpp"
#include "formats/textd.hpp"
namespace bmh::world12 {
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
