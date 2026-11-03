// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "ui/world12_statistics.hpp"
namespace bmh::ui {
// Composition of the existing Original nine-page statistics menu and screens.
class World12StatisticsScreen {
public:
    World12StatisticsScreen(const std::filesystem::path&,const World12ReadonlyState&,const LeImage&);
    Image render()const;
    bool move(int,int);
    bool down(int,int,bool);
    bool up(int,int,bool);
    const std::optional<World12ReadonlyRequest>& pending()const{return pending_;}
    bool pending_on_down()const{return pending_on_down_;}
    void reject(std::string reason){menu_->reject(std::move(reason));}
    std::string state_json()const;
private:
    World12ReadonlyState state_;
    LeImage source_;
    std::unique_ptr<StatisticsMenuScreen> menu_;
    std::unique_ptr<OpportunitiesScreen> opportunities_;
    std::unique_ptr<TransferHistoryScreen> transfers_;
    std::unique_ptr<AnalyticsScreen> analytics_;
    std::unique_ptr<AllTimeTableScreen> alltime_;
    std::unique_ptr<WorldCupStatisticsScreen> world_cup_;
    std::unique_ptr<FinanceTotalsScreen> finance_;
    std::unique_ptr<SeriesRecordsScreen> series_;
    std::unique_ptr<StatisticsRankingsScreen> rankings_;
    std::unique_ptr<MinuteStatisticsScreen> minutes_;
    std::optional<World12ReadonlyRequest> pending_;
    bool pending_on_down_=false;
    bool capture(bool,bool down);
};
}
