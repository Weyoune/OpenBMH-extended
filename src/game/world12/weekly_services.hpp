// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/weekly_incidents.hpp"
#include "game/world12/market_auctions.hpp"
#include "game/world12/market_bids.hpp"
#include "game/world12/weekly_players.hpp"
namespace bmh::world12 {
struct WeeklyServiceOutcome {
    uint64_t game_draws=0,watcom_draws=0;
    WeeklyIncidentOutcome incidents;
    MarketAuctionOutcome auctions;
    MarketBidOutcome bids;
    WeeklyPlayerOutcome players;
    EconomyPhaseOutcome settlement;
    TrainingOutcome training;
};
// Source DayStepRest3, followed by familiarity and human training. The caller
// owns the weekday and any subsequent month change. Complete phase publication
// is atomic, including failures after earlier human decisions or transfers.
WeeklyServiceOutcome process_weekly_services(CareerSnapshot&,const GameState& reference,
    const TextDb&,const WeeklyIncidentContext&,const WeeklyIncidentHooks&,
    const MarketAuctionHooks&,MarketBidPolicy);
}
