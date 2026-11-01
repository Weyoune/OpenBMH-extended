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
// Actual global membership order and all eligible AI clubs participate. The
// listing/physical bid slots, Source sorting quirks and RNG are retained.
MarketBidOutcome add_computer_market_bid(CareerSnapshot&,uint8_t row,const GameState& reference,
    MarketBidPolicy,bool high=false);
// The new-bid part of weekly_processing, AFTER countdown/settlement. No expiry,
// roster movement or calendar advancement is implied by this phase alone.
MarketBidOutcome generate_weekly_market_bids(CareerSnapshot&,const GameState& reference,MarketBidPolicy);
}
