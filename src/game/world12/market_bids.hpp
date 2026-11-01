// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/save.hpp"
#include "game/state.hpp"
namespace bmh::world12 {
struct MarketBidPolicy {bool three_points=true;};
struct MarketBidOutcome {
    uint64_t game_draws=0,watcom_draws=0;
    size_t added=0;
};
struct UnsolicitedOffer {
    uint8_t row=40;
    bool salary_only=false,warned=false;
    bool operator==(const UnsolicitedOffer&)const=default;
};
// Later-day offer using current standings, squad/loan counts and retained bid
// history. The event owner stores the returned notice before entering the desk.
std::optional<UnsolicitedOffer> prepare_unsolicited_offer(CareerSnapshot&,uint8_t seat,
    const GameState& reference,WatcomRandom&,MarketBidPolicy);
// Place a regenerated, unassigned player when the appropriate free pool is
// full. Uses the Source no-list two-pass recruitment scoring and AI lineup.
ClubId place_unlisted_player(CareerSnapshot&,PlayerId,const GameState&,WatcomRandom&,MarketBidPolicy);
// Actual global membership order and all eligible AI clubs participate. The
// listing/physical bid slots, Source sorting quirks and RNG are retained.
MarketBidOutcome add_computer_market_bid(CareerSnapshot&,uint8_t row,const GameState& reference,
    MarketBidPolicy,bool high=false);
// The new-bid part of weekly_processing, AFTER countdown/settlement. No expiry,
// roster movement or calendar advancement is implied by this phase alone.
MarketBidOutcome generate_weekly_market_bids(CareerSnapshot&,const GameState& reference,MarketBidPolicy);
}
