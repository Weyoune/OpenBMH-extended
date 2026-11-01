// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/monthly_services.hpp"
#include "game/world12/weekly_services.hpp"
#include "game/calendar.hpp"
namespace bmh::world12 {
struct DailyServiceContext {
    uint32_t previous_date=0xffffffffu,season=0;
    CalendarEpoch epoch{0,0};
    std::optional<DailyEconomyKind> entry;
    WeeklyIncidentContext weekly_relations;
};
struct DailyServiceHooks {
    WeeklyIncidentHooks incidents;MarketAuctionHooks auctions;
    LoanReturnHooks loans;MonthlyContractHooks contracts;
    MarketBidPolicy bids;MonthlyMarketPolicy listings;
};
struct DailyServiceOutcome {
    uint64_t game_draws=0,watcom_draws=0;
    bool trained=false,weekly=false,monthly=false,season_end=false;
    // next_event performs this additional market tick after DayStepRest and
    // before examining the new date's calendar entry. Season-end desks return
    // before that tick. The remaining event/season owner handles this boundary.
    bool post_event_stock_pending=false;
};
// Finance and DayStepRest after an authenticated date increment. Does not
// advance a calendar, process fixtures, present a desk or seek the next event.
DailyServiceOutcome process_daily_services(CareerSnapshot&,const GameState&,const TextDb&,
    const DailyServiceContext&,const DailyServiceHooks&);
}
