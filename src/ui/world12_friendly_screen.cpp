// SPDX-License-Identifier: GPL-3.0-only
#include "ui/world12_friendlies.hpp"
#include "ui/opportunities.hpp"
namespace bmh::ui {
World12FriendlyScreen::World12FriendlyScreen(const std::filesystem::path& data,World12FriendlyPicker picker,Image desktop,const LeImage& source):data_(data),source_(source),underlay_(std::move(desktop)){if(underlay_.width!=320||underlay_.height!=240)throw FormatError("world12: friendly screen requires its actual accepted desktop");show_picker(std::move(picker));}
World12FriendlyScreen::World12FriendlyScreen(const World12FriendlyScreen& other):data_(other.data_),source_(other.source_),underlay_(other.underlay_){show_picker(other.picker_);if(other.match_){match_=std::make_unique<FriendlyMatchScreen>(*other.match_);table_.reset();nations_.reset();}else if(other.nations_)nations_=std::make_unique<WorldCupStatisticsScreen>(*other.nations_);pending_=other.pending_;pending_on_down_=other.pending_on_down_;}
void World12FriendlyScreen::show_picker(World12FriendlyPicker picker){
    if(!picker.sequence||(picker.national?(!picker.nations||picker.table.has_value()):(!picker.table||picker.nations.has_value())))throw FormatError("world12: friendly screen lacks its actual opponent picker");
    if(picker_.sequence&&(picker.sequence!=picker_.sequence||picker.canonical_actor!=picker_.canonical_actor||picker.national!=picker_.national))throw FormatError("world12: friendly picker changed canonical owner");
    picker_=std::move(picker);match_.reset();table_.reset();nations_.reset();pending_.reset();pending_on_down_=false;
    if(picker_.national){DeskRequest owner;owner.kind=DeskRequestKind::open_menu;owner.menu_kind=23;owner.manager_slot=picker_.canonical_actor;owner.revision=picker_.revision;StatisticsMenuScreen parent(data_,StatisticsMenuState{owner,underlay_,0,picker_.gradient_text},source_);nations_=std::make_unique<WorldCupStatisticsScreen>(data_,*picker_.nations,parent.render_page(4),false,source_);}else table_=std::make_unique<LeagueTableScreen>(data_,*picker_.table,source_);
}
void World12FriendlyScreen::show_selection(FriendlyMatchState state){if(!pending_||!state.owned_world12||state.national_team!=picker_.national||state.revision!=picker_.revision)throw FormatError("world12: friendly selection has no accepted picker return");match_=std::make_unique<FriendlyMatchScreen>(data_,std::move(state),source_,6);table_.reset();nations_.reset();pending_.reset();pending_on_down_=false;}
Image World12FriendlyScreen::render()const{return match_?match_->render():table_?table_->render():nations_->render();}
bool World12FriendlyScreen::capture(bool changed){if(match_&&match_->close_requested())pending_=World12FriendlyClose{};else if(table_&&table_->pending_world12_action())pending_=*table_->pending_world12_action();return changed;}
bool World12FriendlyScreen::move(int x,int y){return !pending_&&(match_?match_->move(x,y):table_?table_->move(x,y):nations_->move(x,y));}
bool World12FriendlyScreen::pointer_down(int x,int y,bool right){
    if(pending_)return false;if(match_)return capture(match_->pointer_down(x,y,right));if(table_){const bool changed=capture(table_->pointer_down(x,y,right));if(pending_)pending_on_down_=true;return changed;}
    const auto request=nations_->pointer_down(x,y,right);if(request.kind==WorldCupStatisticsRequestKind::none)return false;pending_=request;pending_on_down_=true;return true;
}
bool World12FriendlyScreen::pointer_up(int x,int y,bool right){if(pending_)return false;if(match_){const auto before=match_->stage();const auto request=match_->click(x,y,right);if(request)pending_=*request;return capture(request.has_value()||before!=match_->stage()||match_->close_requested());}return table_?capture(table_->click(x,y,right)):false;}
bool World12FriendlyScreen::key(std::string_view key){if(pending_)return false;if(match_){const auto before=match_->stage();const auto request=match_->key(key);if(request)pending_=*request;return capture(request.has_value()||before!=match_->stage()||match_->close_requested());}if(table_)return capture(table_->key(key));return false;}
bool World12FriendlyScreen::acknowledge_quote(FriendlyQuoteProjection quote){if(!match_||!pending_||!std::holds_alternative<FriendlyMatchRequest>(*pending_)||std::get<FriendlyMatchRequest>(*pending_).kind!=FriendlyRequestKind::quote)return false;const bool accepted=match_->acknowledge_quote(std::move(quote));if(accepted){pending_.reset();pending_on_down_=false;}return accepted;}
bool World12FriendlyScreen::acknowledge_booking(const FriendlyBookingAck& ack){if(!match_||!pending_||!std::holds_alternative<FriendlyMatchRequest>(*pending_)||std::get<FriendlyMatchRequest>(*pending_).kind!=FriendlyRequestKind::book)return false;const bool accepted=match_->acknowledge_booking(ack);if(accepted){picker_.revision=ack.revision;pending_.reset();pending_on_down_=false;capture(true);}return accepted;}
void World12FriendlyScreen::reject(std::string reason){if(match_)match_->reject(std::move(reason));else if(table_&&table_->pending_world12_action())table_->reject_world12_action(std::move(reason));pending_.reset();pending_on_down_=false;}
std::string World12FriendlyScreen::state_json()const{return match_?match_->state_json():table_?table_->state_json():"{\"screen\":\"world12_friendly_nations\",\"manager\":"+std::to_string(picker_.canonical_actor)+",\"revision\":"+std::to_string(picker_.revision)+",\"sequence\":"+std::to_string(picker_.sequence)+"}";}

}
