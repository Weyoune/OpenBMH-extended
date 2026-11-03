// SPDX-License-Identifier: GPL-3.0-only
#include "app/world12_session.hpp"
#include "app/world12_visible_menu.hpp"
#include "ui/world12_desktop.hpp"
#include "ui/world12_statistics.hpp"
#include <algorithm>
namespace bmh::app::world12 {
using namespace bmh::world12;
namespace {
std::vector<ui::World12CareerIdentity> roster(const CareerSnapshot& state){
    std::vector<ui::World12CareerIdentity> result;for(const auto& manager:state.world.managers)result.push_back({manager.seat,manager.portrait,manager.club,manager.name});std::sort(result.begin(),result.end(),[](const auto& a,const auto& b){return a.seat<b.seat;});return result;
}
}
ui::World12ReadonlyState Session::open_readonly(const ui::DeskRequest& request,const std::filesystem::path& data,const Image& underlay,const std::filesystem::path& save_directory){
    state_.validate();
    if(preparation_menu_||readonly_owner_||request.kind!=ui::DeskRequestKind::open_menu||(request.menu_kind!=4&&request.menu_kind!=8&&request.menu_kind!=9&&request.menu_kind!=23&&request.menu_kind!=20&&request.menu_kind!=25&&request.menu_kind!=26&&request.menu_kind!=28)||request.value!=-1||request.revision!=owner_.revision||owner_.revision==UINT64_MAX||readonly_generation_>=UINT32_MAX||underlay.width!=320||underlay.height!=240)
        throw FormatError("world12: stale or unavailable readonly desktop owner");
    const auto desk=ui::world12_initial_desktop(data,state_,resources_.reference,resources_.german,options_,owner_.revision);
    if(ui::desk_actor(desk)!=request.manager_slot||desk.world12_career_roster!=roster(state_)||!visible_world12_menu(desk,request.item_id,request.menu_kind))
        throw FormatError("world12: readonly request belongs to another canonical actor or hidden menu");
    const auto row=desk.manager_slot;if(row<0||size_t(row)>=desk.managers.size())throw FormatError("world12: readonly rendered actor row is missing");
    const auto& actor=desk.managers[size_t(row)];
    ui::World12ReadonlyIdentity identity{uint8_t(ui::desk_actor(desk)),uint8_t(request.menu_kind),actor.world_club,state_.date,owner_.revision+1,owner_.scope,(readonly_generation_+1)<<32,desk.world12_career_roster};
    HighscoreTable highscore_rows{};
    if(request.menu_kind==26){if(!state_.world.highscore_signature)throw FormatError("world12: saved highscore signature is unknown");if(*state_.world.highscore_signature){if(save_directory.empty()||!resources_.texts)throw FormatError("world12: ranked highscore requires its actual configured ledger directory/texts");const auto file=highscore_file(save_directory,*state_.world.highscore_signature,resources_.texts->strings.at(889));if(std::filesystem::exists(file)){const auto bytes=read_file(file);if(bytes.size()!=360)throw FormatError("world12: actual highscore ledger has invalid Source size");HighscoreBytes raw;std::copy(bytes.begin(),bytes.end(),raw.begin());highscore_rows=decode_highscore_table(raw);}}}
    ui::World12ReadonlySelection selection;const auto country=state_.world.clubs.at(actor.world_club).country;selection.alltime_nation=int(country);
    auto page=ui::world12_readonly_state(state_,identity,selection,resources_.reference,resources_.german,underlay,highscore_rows);
    auto source=std::make_shared<const CareerSnapshot>(state_);
    readonly_highscore_rows_=highscore_rows;readonly_owner_=identity;readonly_selection_=selection;readonly_underlay_=underlay;readonly_source_=std::move(source);++readonly_generation_;++owner_.revision;preparation_menu_=request.menu_kind;return page;
}
ui::World12ReadonlyState Session::readonly_view()const{
    if(!readonly_owner_||preparation_menu_!=readonly_owner_->menu||readonly_owner_->scope!=owner_.scope||readonly_owner_->revision!=owner_.revision||(readonly_owner_->sequence>>32)!=readonly_generation_||!readonly_source_||state_!=*readonly_source_||roster(state_)!=readonly_owner_->career_roster||!state_.desktop||state_.desktop->active_seat!=readonly_owner_->manager)
        throw FormatError("world12: readonly window source/actor/generation changed");
    return ui::world12_readonly_state(state_,*readonly_owner_,readonly_selection_,resources_.reference,resources_.german,readonly_underlay_,readonly_highscore_rows_);
}
bool Session::apply_readonly(const ui::World12ReadonlyRequest& request){
    const auto current=readonly_view();if(request.identity!=current.identity||owner_.revision==UINT64_MAX||uint32_t(current.identity.sequence)==UINT32_MAX)throw FormatError("world12: stale readonly action");
    if(current.identity.menu==28&&(request.action!=ui::World12ReadonlyAction::close||request.value!=-1||request.first_season||request.expected_flags||request.target_flags))throw FormatError("world12: current balance owns only its exact close action");
    auto selection=readonly_selection_;bool closed=false;std::optional<CareerSnapshot> changed;
    switch(request.action){
    case ui::World12ReadonlyAction::close:if(request.metric!=current.selection.metric)throw FormatError("world12: readonly close metric changed");closed=true;break;
    case ui::World12ReadonlyAction::set_metric:
        if(current.identity.menu!=20||uint8_t(request.metric)>uint8_t(ui::TopScorersMetric::scorer))throw FormatError("world12: unsupported readonly metric");selection.metric=request.metric;break;
    case ui::World12ReadonlyAction::toggle_scope:
        if(current.identity.menu!=20||request.metric!=current.selection.metric)throw FormatError("world12: unsupported readonly scope action");selection.scope=selection.scope==ui::TopScorersScope::league?ui::TopScorersScope::managers:ui::TopScorersScope::league;break;
    case ui::World12ReadonlyAction::close_child:
        if(current.identity.menu!=23||selection.statistics_page<0)throw FormatError("world12: statistics child is not open");selection.statistics_page=-1;break;
    case ui::World12ReadonlyAction::select_statistics_page:
        if(current.identity.menu!=23||request.value<0||request.value>8)throw FormatError("world12: unavailable statistics selector");selection.statistics_page=request.value;if(request.value==0)selection.prediction_mode=1;if(request.value==7){selection.title_category=0;selection.title_first_season=0;}break;
    case ui::World12ReadonlyAction::set_prediction:
        if(current.identity.menu!=23||selection.statistics_page!=0||(request.value!=1&&request.value!=2&&request.value!=4&&request.value!=8))throw FormatError("world12: unavailable prediction selector");selection.prediction_mode=request.value;break;
    case ui::World12ReadonlyAction::select_title_category:
        if(current.identity.menu!=23||selection.statistics_page!=7||request.value<0||request.value>=9||request.first_season<0||request.first_season%4)throw FormatError("world12: unavailable historical title selector");selection.title_category=request.value;selection.title_first_season=request.first_season;break;
    case ui::World12ReadonlyAction::select_alltime_nation:
        if(current.identity.menu!=23||selection.statistics_page!=3||request.value<0||request.value>=5)throw FormatError("world12: unavailable all-time country selector");selection.alltime_nation=request.value;break;
    case ui::World12ReadonlyAction::toggle_alltime_relative:
        if(current.identity.menu!=23||selection.statistics_page!=3)throw FormatError("world12: unavailable all-time scale selector");selection.alltime_relative=!selection.alltime_relative;break;
    case ui::World12ReadonlyAction::set_statistics_flags:{
        if(current.identity.menu!=23||selection.statistics_page!=2||!current.statistics||!current.statistics->analytics||request.expected_flags!=current.statistics->analytics->source_flags||request.value<0||request.value>6||request.target_flags!=uint8_t(request.expected_flags^(request.value==6?0xc0:1u<<request.value)))throw FormatError("world12: stale or unsupported statistics preference");
        changed=state_;auto actor=std::find_if(changed->world.managers.begin(),changed->world.managers.end(),[&](const auto& manager){return manager.seat==current.identity.manager;});if(actor==changed->world.managers.end())throw FormatError("world12: statistics flags have no actual manager owner");if(actor->career)actor->career->statistics_flags=request.target_flags;else {if(changed->world.catalogue.scope.club_play()||!changed->world.national_coaches)throw FormatError("world12: statistics flags have no actual national preference owner");const auto coach=std::find_if(changed->world.national_coaches->begin(),changed->world.national_coaches->end(),[&](const auto& record){return record.active&&record.coach==current.identity.manager&&record.team==current.identity.club;});if(coach==changed->world.national_coaches->end())throw FormatError("world12: national statistics actor disappeared");if(!coach->view_preferences)coach->view_preferences.emplace();coach->view_preferences->statistics_flags=request.target_flags;}changed->validate();break;
    }
    default:throw FormatError("world12: unsupported readonly action");
    }
    auto identity=current.identity;++identity.revision;++identity.sequence;
    if(!closed)(void)ui::world12_readonly_state(changed?*changed:state_,identity,selection,resources_.reference,resources_.german,readonly_underlay_,readonly_highscore_rows_);
    auto source=changed?std::make_shared<const CareerSnapshot>(*changed):readonly_source_;
    if(changed)state_=std::move(*changed);readonly_source_=std::move(source);++owner_.revision;
    if(closed){readonly_owner_.reset();readonly_source_.reset();readonly_underlay_={};preparation_menu_.reset();}else {readonly_owner_=identity;readonly_selection_=selection;}
    return closed;
}
}
