// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "ui/world12_table.hpp"
#include "ui/world_cup_statistics.hpp"
#include "ui/friendly_match.hpp"
#include <memory>
#include <variant>
namespace bmh::ui {
struct World12FriendlyPicker {
    uint64_t sequence=0,revision=0;uint8_t canonical_actor=0;
    bool national=false;
    bool gradient_text=true;
    std::optional<World12TableState> table;
    std::optional<WorldCupStatisticsProjection> nations;
    std::array<bmh::world12::ClubId,62> nation_ids;
};
WorldCupStatisticsProjection world12_friendly_nations(const bmh::world12::CareerSnapshot&,bmh::world12::ClubId own,const GameState& reference,const LeImage& actual_source,std::array<bmh::world12::ClubId,62>& countries);
struct World12FriendlyClose { bool operator==(const World12FriendlyClose&)const=default; };
using World12FriendlyRequest=std::variant<World12TableRequest,WorldCupStatisticsRequest,FriendlyMatchRequest,World12FriendlyClose>;
// Routes input through the existing Source picker and consent/calendar screens.
class World12FriendlyScreen {
public:
    World12FriendlyScreen(const std::filesystem::path&,World12FriendlyPicker,Image accepted_desktop,const LeImage&);
    World12FriendlyScreen(const World12FriendlyScreen&);
    Image render()const;
    bool move(int,int);
    bool pointer_down(int,int,bool right=false);
    bool pointer_up(int,int,bool right=false);
    bool key(std::string_view);
    bool text(std::string_view){return false;}
    const std::optional<World12FriendlyRequest>& pending()const{return pending_;}
    bool owns_pending_release()const{return pending_on_down_;}
    const World12FriendlyPicker& picker()const{return picker_;}
    void show_picker(World12FriendlyPicker);
    void show_selection(FriendlyMatchState);
    bool acknowledge_quote(FriendlyQuoteProjection);
    bool acknowledge_booking(const FriendlyBookingAck&);
    void reject(std::string);
    std::string state_json()const;
    std::optional<std::pair<int,int>> league_center(bmh::world12::LeagueId id)const{return table_?table_->world12_league_center(id):std::nullopt;}
private:
    std::filesystem::path data_;
    LeImage source_;
    Image underlay_;
    World12FriendlyPicker picker_;
    std::unique_ptr<LeagueTableScreen> table_;
    std::unique_ptr<WorldCupStatisticsScreen> nations_;
    std::unique_ptr<FriendlyMatchScreen> match_;
    std::optional<World12FriendlyRequest> pending_;
    bool pending_on_down_=false;
    bool capture(bool);
};
}
