// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/save.hpp"
#include "game/state.hpp"
namespace bmh::world12 {
struct MarketBidPolicy {
    bool three_points=true;
    // Annual release overflow runs after league swaps/shuffle while Source
    // still retains old per-club position history. Never substitute rankings
    // recomputed under the new fixture order.
    const std::map<ClubId,uint8_t>* completed_ranks=nullptr;
    ClubId released_from; // Source slot_team(slot), excluded before no-list scoring.
};
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
// Actual inactive origin population; no active club/minimum/market is invented.
ClubId place_unlisted_national_player(World&,PlayerId,const GameState&,WatcomRandom&,ClubId released_from={});
// Actual global membership order and all eligible AI clubs participate. The
// listing/physical bid slots, Source sorting quirks and RNG are retained.
MarketBidOutcome add_computer_market_bid(CareerSnapshot&,uint8_t row,const GameState& reference,
    MarketBidPolicy,bool high=false);
// The new-bid part of weekly_processing, AFTER countdown/settlement. No expiry,
// roster movement or calendar advancement is implied by this phase alone.
MarketBidOutcome generate_weekly_market_bids(CareerSnapshot&,const GameState& reference,MarketBidPolicy);
}
