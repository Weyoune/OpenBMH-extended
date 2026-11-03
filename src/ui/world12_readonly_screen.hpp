// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "ui/world12_readonly.hpp"
#include "ui/top_scorers.hpp"
#include "ui/calendar.hpp"
namespace bmh::ui {
// Owns the existing Original screens; render/input rules stay in those screens.
class World12ReadonlyScreen {
public:
    World12ReadonlyScreen(const std::filesystem::path&,World12ReadonlyState,const LeImage&);
    Image render()const;
    bool move(int,int);
    bool pointer_down(int,int,bool right=false);
    bool pointer_up(int,int,bool right=false);
    bool key(std::string_view);
    bool text(std::string_view);
    bool points_active()const{return records_&&records_->points_active();}
    bool advance_point(){return records_&&records_->advance_point();}
    bool finish_points(){return records_&&records_->finish_points();}
    const std::optional<World12ReadonlyRequest>& pending()const{return pending_;}
    bool owns_pending_release()const{return pending_on_down_;}
    const World12ReadonlyState& state()const{return state_;}
    std::string state_json()const;
private:
    World12ReadonlyState state_;
    std::unique_ptr<AchievementsScreen> achievements_;
    std::unique_ptr<IndoorStandingsScreen> indoor_;
    std::unique_ptr<CalendarScreen> calendar_;
    std::unique_ptr<TopScorersScreen> top_;
    std::unique_ptr<DeskRecordsScreen> records_;
    std::optional<World12ReadonlyRequest> pending_;
    bool pending_on_down_=false;
    bool capture(bool);
};
}
