// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "ui/world12_readonly.hpp"
#include "ui/opportunities.hpp"
#include "ui/analytics.hpp"
#include "ui/alltime_table.hpp"
#include "ui/finance_totals.hpp"
#include "ui/series_records.hpp"
#include "ui/minute_statistics.hpp"
#include "ui/statistics_rankings.hpp"
#include "ui/transfer_history.hpp"
#include "ui/world_cup_statistics.hpp"
namespace bmh::ui {
struct World12StatisticsSelection {
    int page=-1, prediction_mode=1, category=0, nation=0,first_season=0;
    bool relative=false;
    bool operator==(const World12StatisticsSelection&)const=default;
};
struct World12StatisticsState {
    StatisticsMenuState menu;
    World12StatisticsSelection selection;
    std::optional<OpportunitiesState> opportunities;
    std::optional<TransferHistoryProjection> transfers;
    std::optional<AnalyticsState> analytics;
    std::optional<AllTimeTableState> alltime;
    std::optional<WorldCupStatisticsProjection> world_cup;
    std::optional<FinanceTotals> finance;
    std::optional<SeriesRecordsProjection> series;
    std::optional<StatisticsRankingState> rankings;
    std::optional<MinuteStatisticsProjection> minutes;
    std::optional<GameState> formatting;
    std::vector<bmh::world12::ClubId> clubs;
};
World12StatisticsState world12_statistics_state(const bmh::world12::CareerSnapshot&,
    const World12ReadonlyIdentity&,World12StatisticsSelection,const GameState&,const LeImage&,const Image&);
}
