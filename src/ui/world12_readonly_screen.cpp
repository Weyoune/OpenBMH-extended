// SPDX-License-Identifier: GPL-3.0-only
#include "ui/world12_readonly_screen.hpp"
namespace bmh::ui {
World12ReadonlyScreen::World12ReadonlyScreen(const std::filesystem::path& data,World12ReadonlyState state,const LeImage& source):state_(std::move(state)){
    if(state_.identity.menu==28&&state_.balance){if(!state_.balance->actuals_only||state_.balance->draft.view!=BalanceView::so_far)throw FormatError("world12: readonly balance cannot own a plan");balance_=std::make_unique<BalanceScreen>(data,*state_.balance,source);}
    else if(state_.identity.menu==23&&state_.statistics)statistics_=std::make_unique<World12StatisticsScreen>(data,state_,source);
    else if(state_.identity.menu==4&&state_.achievements)achievements_=std::make_unique<AchievementsScreen>(data,*state_.achievements,source);
    else if(state_.identity.menu==9&&state_.indoor)indoor_=std::make_unique<IndoorStandingsScreen>(data,*state_.indoor,source);
    else if(state_.identity.menu==8&&state_.calendar&&!state_.top_scorers&&!state_.records){if(state_.calendar->reschedule_manager>=0||state_.calendar->friendly_selection||!state_.calendar->reschedule_options.empty())throw FormatError("world12: readonly calendar cannot own rescheduling");DeskRequest owner;owner.kind=DeskRequestKind::open_menu;owner.menu_kind=8;owner.manager_slot=state_.identity.manager;owner.revision=state_.identity.revision;calendar_=std::make_unique<CalendarScreen>(data,*state_.calendar,std::string{},state_.decorated_background,-1,owner,source);}
    else if(state_.identity.menu==20&&state_.top_scorers&&!state_.records)top_=std::make_unique<TopScorersScreen>(data,*state_.top_scorers,source);
    else if((state_.identity.menu==25||state_.identity.menu==26)&&state_.records&&!state_.top_scorers)records_=std::make_unique<DeskRecordsScreen>(data,*state_.records,source);
    else throw FormatError("world12: readonly view lacks its actual Original screen");
}
Image World12ReadonlyScreen::render()const{return balance_?balance_->render():statistics_?statistics_->render():achievements_?achievements_->render():indoor_?indoor_->render():calendar_?calendar_->render():top_?top_->render():records_->render();}
bool World12ReadonlyScreen::capture(bool changed){
    if(balance_&&balance_->pending()){if(balance_->pending()->kind!=BalanceAction::close||balance_->pending()->identity!=state_.balance->identity)throw FormatError("world12: readonly balance attempted a plan action");pending_=World12ReadonlyRequest{state_.identity,World12ReadonlyAction::close,state_.selection.metric};}
    else if(statistics_&&statistics_->pending())pending_=*statistics_->pending();
    else if(achievements_&&achievements_->pending())pending_=World12ReadonlyRequest{state_.identity,World12ReadonlyAction::close,state_.selection.metric};
    else if(calendar_&&calendar_->close_requested())pending_=World12ReadonlyRequest{state_.identity,World12ReadonlyAction::close,state_.selection.metric};
    else if(top_&&top_->pending()){
        const auto& request=*top_->pending();if(request.identity!=state_.top_scorers->identity)throw FormatError("world12: readonly child changed identity");
        World12ReadonlyRequest out;out.identity=state_.identity;out.metric=request.metric;
        if(request.action==TopScorersAction::close)out.action=World12ReadonlyAction::close;
        else if(request.action==TopScorersAction::set_metric)out.action=World12ReadonlyAction::set_metric;
        else if(request.action==TopScorersAction::toggle_scope)out.action=World12ReadonlyAction::toggle_scope;
        else throw FormatError("world12: unexpected readonly child action");pending_=out;
    }else if(records_&&records_->closed())pending_=World12ReadonlyRequest{state_.identity,World12ReadonlyAction::close,state_.selection.metric};
    return changed;
}
bool World12ReadonlyScreen::move(int x,int y){return !pending_&&(statistics_?statistics_->move(x,y):achievements_?achievements_->move(x,y):calendar_?calendar_->move(x,y):top_?top_->move(x,y):false);}
bool World12ReadonlyScreen::pointer_down(int x,int y,bool right){if(pending_||calendar_)return false;if(balance_){const auto changed=capture(balance_->pointer_down(x,y,right));if(pending_)pending_on_down_=true;return changed;}if(statistics_){const auto changed=capture(statistics_->down(x,y,right));if(pending_)pending_on_down_=statistics_->pending_on_down();return changed;}if(achievements_){const auto previous=achievements_->state().selected_season;const auto action=achievements_->pointer_down(x,y,right);const bool changed=action.kind!=AchievementsRequestKind::none||previous!=achievements_->state().selected_season;if(achievements_->pending()){capture(changed);pending_on_down_=true;}return changed;}if(indoor_){pending_=World12ReadonlyRequest{state_.identity,World12ReadonlyAction::close,state_.selection.metric};pending_on_down_=true;return true;}const bool changed=capture(top_?top_->pointer_down(x,y,right):records_->pointer_down(x,y,right));if(pending_)pending_on_down_=true;return changed;}
bool World12ReadonlyScreen::pointer_up(int x,int y,bool right){if(pending_||indoor_||achievements_)return false;return capture(balance_?(!right&&balance_->release(x,y)):statistics_?statistics_->up(x,y,right):calendar_?calendar_->click(x,y,right):top_?(!right&&top_->release(x,y)):records_->pointer_up(x,y,right));}
bool World12ReadonlyScreen::key(std::string_view key){return !pending_&&capture(balance_?balance_->key(key):calendar_?calendar_->key(key):records_?records_->key_input(key):false);}
bool World12ReadonlyScreen::process_timer_expired(){return !pending_&&balance_&&capture(balance_->process_timer_expired());}
bool World12ReadonlyScreen::text(std::string_view text){return !pending_&&records_&&records_->text_input(text);}
std::string World12ReadonlyScreen::state_json()const{
    if(balance_)return balance_->state_json();
    if(statistics_)return statistics_->state_json();
    if(achievements_)return "{\"screen\":\"achievements\",\"manager\":"+std::to_string(state_.identity.manager)+",\"season_count\":"+std::to_string(achievements_->state().season_count)+"}";
    if(indoor_)return indoor_->state_json();
    if(calendar_)return calendar_->state_json();
    if(top_)return top_->state_json();
    return std::string("{\"screen\":\"")+(state_.identity.menu==26?"highscore":"fair_play")+"\",\"manager\":"+std::to_string(state_.identity.manager)+",\"revision\":"+std::to_string(state_.identity.revision)+",\"scope\":"+std::to_string(state_.identity.scope)+",\"sequence\":"+std::to_string(state_.identity.sequence)+"}";
}
}
