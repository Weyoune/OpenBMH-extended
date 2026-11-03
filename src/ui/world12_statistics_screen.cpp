// SPDX-License-Identifier: GPL-3.0-only
#include "ui/world12_statistics_screen.hpp"
namespace bmh::ui {
World12StatisticsScreen::World12StatisticsScreen(const std::filesystem::path& data,const World12ReadonlyState& state,const LeImage& reference):state_(state),source_(reference){
    if(!state.statistics)throw FormatError("world12: missing owned statistics projection");
    const auto& page=*state.statistics;menu_=std::make_unique<StatisticsMenuScreen>(data,page.menu,reference);
    const auto frame=page.selection.page<0?menu_->render():menu_->render_page(page.selection.page);
    if(page.opportunities){auto child=*page.opportunities;child.underlay=frame;opportunities_=std::make_unique<OpportunitiesScreen>(data,std::move(child),reference);}
    else if(page.transfers){if(!page.formatting)throw FormatError("world12: transfer history lost its actual formatting context");transfers_=std::make_unique<TransferHistoryScreen>(data,*page.formatting,*page.transfers,reference);}
    else if(page.analytics){auto child=*page.analytics;child.underlay=frame;analytics_=std::make_unique<AnalyticsScreen>(data,std::move(child),reference);}
    else if(page.alltime){auto child=*page.alltime;child.underlay=frame;alltime_=std::make_unique<AllTimeTableScreen>(data,std::move(child),reference);}
    else if(page.world_cup)world_cup_=std::make_unique<WorldCupStatisticsScreen>(data,*page.world_cup,frame,false,reference);
    else if(page.finance)finance_=std::make_unique<FinanceTotalsScreen>(data,*page.finance,frame);
    else if(page.series)series_=std::make_unique<SeriesRecordsScreen>(data,*page.series,frame,reference);
    else if(page.rankings){auto child=*page.rankings;child.underlay=frame;rankings_=std::make_unique<StatisticsRankingsScreen>(data,std::move(child),reference);}
    else if(page.minutes)minutes_=std::make_unique<MinuteStatisticsScreen>(data,*page.minutes,reference);
    else if(page.selection.page>=0)throw FormatError("world12: statistics child has no actual Source projection");
}
Image World12StatisticsScreen::render()const{
    if(opportunities_)return opportunities_->render();if(analytics_)return analytics_->render();if(alltime_)return alltime_->render();if(world_cup_)return world_cup_->render();if(finance_)return finance_->render();if(series_)return series_->render();if(rankings_)return rankings_->render();if(minutes_)return minutes_->render(menu_->render_page(8));
    if(transfers_){const auto count=state_.statistics->menu.transfer_history_count;return transfers_->render(menu_->render_page(1),(28-std::max(1,count))*3+13);}
    return menu_->render();
}
bool World12StatisticsScreen::capture(bool changed,bool down){
    World12ReadonlyRequest out;out.identity=state_.identity;out.metric=state_.selection.metric;bool found=true;
    if(opportunities_&&opportunities_->pending()){const auto& action=*opportunities_->pending();if(action.identity!=state_.statistics->opportunities->identity)throw FormatError("world12: opportunities changed its canonical owner");out.action=action.action==OpportunitiesAction::close?World12ReadonlyAction::close:World12ReadonlyAction::set_prediction;out.value=action.mode;}
    else if(analytics_&&analytics_->pending()){const auto& action=*analytics_->pending();if(action.owner!=state_.statistics->menu.owner||action.manager_slot!=state_.identity.manager)throw FormatError("world12: analytics changed its canonical owner");out.action=action.kind==AnalyticsRequestKind::close?World12ReadonlyAction::close:World12ReadonlyAction::set_statistics_flags;out.value=action.kind==AnalyticsRequestKind::toggle_absolute_scale?6:action.metric;out.expected_flags=action.expected_flags;out.target_flags=action.target_flags;}
    else if(alltime_&&alltime_->pending()){const auto& action=*alltime_->pending();if(action.owner!=state_.statistics->menu.owner)throw FormatError("world12: all-time table changed its canonical owner");out.action=action.kind==AllTimeRequestKind::close?World12ReadonlyAction::close:action.kind==AllTimeRequestKind::select_nation?World12ReadonlyAction::select_alltime_nation:World12ReadonlyAction::toggle_alltime_relative;out.value=action.nation;}
    else if(rankings_&&rankings_->pending()){const auto& action=*rankings_->pending();out.action=action.kind==StatisticsRankingsRequestKind::close?World12ReadonlyAction::close:World12ReadonlyAction::select_title_category;out.value=action.category;}
    else if(menu_->pending()){const auto& action=*menu_->pending();if(action.owner!=state_.statistics->menu.owner)throw FormatError("world12: statistics selector changed its canonical owner");out.action=action.close?World12ReadonlyAction::close:World12ReadonlyAction::select_statistics_page;out.value=action.selected_index;}
    else found=false;
    if(found){if(out.action==World12ReadonlyAction::close){out.value=-1;out.expected_flags=out.target_flags=0;}pending_=out;pending_on_down_=down;}return changed;
}
bool World12StatisticsScreen::move(int x,int y){if(pending_)return false;return opportunities_?opportunities_->move(x,y):rankings_?rankings_->move(x,y):world_cup_?world_cup_->move(x,y):false;}
bool World12StatisticsScreen::down(int x,int y,bool right){
    if(pending_)return false;bool changed=false;
    if(opportunities_)changed=opportunities_->pointer_down(x,y,right);
    else if(analytics_)changed=analytics_->pointer_down(x,y,right);
    else if(alltime_)changed=alltime_->pointer_down(x,y,right);
    else if(rankings_)changed=rankings_->pointer_down(x,y,right);
    else if(world_cup_){const auto action=world_cup_->pointer_down(x,y,right);if(action.kind==WorldCupStatisticsRequestKind::select_opponent)throw FormatError("world12: readonly World-Cup view cannot select a fixture");changed=action.kind!=WorldCupStatisticsRequestKind::none;if(action.kind==WorldCupStatisticsRequestKind::close)menu_->pointer_down(x,y,true);}
    if(!changed)changed=menu_->pointer_down(x,y,right);
    // Original passive child panes leave the persistent selector underneath.
    return capture(changed,true);
}
bool World12StatisticsScreen::up(int x,int y,bool right){
    if(pending_||right)return false;bool changed=false;
    if(opportunities_)changed=opportunities_->release(x,y);
    else if(alltime_)changed=alltime_->release(x,y);
    else if(rankings_)changed=rankings_->release(x,y);
    if(!changed)changed=menu_->release(x,y);return capture(changed,false);
}
std::string World12StatisticsScreen::state_json()const{return "{\"screen\":\"statistics\",\"manager\":"+std::to_string(state_.identity.manager)+",\"page\":"+std::to_string(state_.selection.statistics_page)+",\"revision\":"+std::to_string(state_.identity.revision)+"}";}
}
