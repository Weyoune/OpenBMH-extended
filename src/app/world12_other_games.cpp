// SPDX-License-Identifier: GPL-3.0-only
#include "app/world12_session.hpp"
#include "game/world12/desktop_projection.hpp"
#include "ui/desktop.hpp"
#include <algorithm>
namespace bmh::app::world12 {
namespace {
bmh::world12::Manager& actor(bmh::world12::CareerSnapshot& state,uint8_t seat) {
    auto found=std::find_if(state.world.managers.begin(),state.world.managers.end(),[&](const auto& m){return m.seat==seat;});
    if(found==state.world.managers.end()||!found->career)throw FormatError("world12: competition manager state is unavailable");return *found;
}
bool visible_menu(const ui::DeskState& desk,int id,int kind) {
    for(const auto& item:desk.items)if(item.id==id&&item.kind==kind&&!item.disabled)return true;
    if(id<1||id>30)return false;
    const auto& row=desk.layout_records[size_t(id)];const size_t folder=row[13]&0x7f;
    if(row[12]!=kind||folder>=3||row[5]!=0xff||row[6]!=0xff||(row[1]&0x10))return false;
    if(std::find(desk.folder_icon_slots[folder].begin(),desk.folder_icon_slots[folder].end(),uint8_t(id))==desk.folder_icon_slots[folder].end())return false;
    return std::any_of(desk.items.begin(),desk.items.end(),[&](const auto& item){return item.kind==50+int(folder)&&!item.disabled;});
}

}
ui::OtherGamesState Session::other_games_page(const bmh::world12::CareerSnapshot& state,
    const ui::World12OtherGamesIdentity& identity,ui::World12OtherSelection selection)const {
    auto out=ui::world12_other_games_state(state,identity,selection);
    const auto p=bmh::world12::project_manager_desktop(state,resources_.reference);
    out.decorated_background=p.state.memory.u8(layout::PlainBackground)==0;
    out.original_panel=p.state.memory.u8(layout::SolidPanelBackground)==0;
    out.gradient_text=p.state.memory.u8(layout::TextGradientDisabled)==0&&resources_.german.u8(layout::TextGradientMode)==2;
    return out;
}
ui::OtherGamesState Session::open_other_games(const ui::DeskRequest& request) {
    validate_file_owner(request.manager_slot,request.revision);
    if(request.kind!=ui::DeskRequestKind::open_menu||request.menu_kind!=5||request.value!=-1||
       other_games_generation_>=UINT32_MAX||owner_.revision==UINT64_MAX)
        throw FormatError("world12: invalid competition overview owner");
    const auto p=bmh::world12::project_manager_desktop(state_,resources_.reference);
    const auto desk=ui::desk_from_game(p.state,resources_.german,owner_.revision);
    if(!visible_menu(desk,request.item_id,5))throw FormatError("world12: competition overview icon is not enabled");
    auto candidate=state_;const auto seat=*candidate.desktop->active_seat;
    // CupGroupOverview begins with detail and participant zero, even for a
    // previous save whose display byte was not yet represented by its schema.
    actor(candidate,seat).career->other_games_view=0x80;
    const ui::World12OtherGamesIdentity identity{seat,actor(candidate,seat).club,owner_.revision+1,owner_.scope,(other_games_generation_+1)<<32};
    const auto selection=ui::default_world12_other_selection(candidate.world);
    auto view=other_games_page(candidate,identity,selection);candidate.validate();
    state_=std::move(candidate);++owner_.revision;++other_games_generation_;other_games_owner_=identity;other_games_selection_=selection;preparation_menu_=5;return view;
}
ui::OtherGamesState Session::other_games_view()const {
    if(preparation_menu_!=5||!other_games_owner_||other_games_owner_->revision!=owner_.revision||
       other_games_owner_->scope!=owner_.scope||(other_games_owner_->sequence>>32)!=other_games_generation_||
       !state_.desktop||state_.desktop->active_seat!=other_games_owner_->manager_slot)
        throw FormatError("world12: stale competition overview window");
    return other_games_page(state_,*other_games_owner_,other_games_selection_);
}
bool Session::apply_other_games(const ui::OtherGamesRequest& request) {
    const auto current=other_games_view();const auto identity=*other_games_owner_;
    if(request.world12_identity!=other_games_owner_||request.identity!=ui::OtherGamesIdentity{}||
       owner_.revision==UINT64_MAX||identity.sequence==UINT64_MAX||((identity.sequence+1)>>32)!=other_games_generation_)
        throw FormatError("world12: stale competition overview action");
    if(request.action==ui::OtherGamesAction::team_info){
        const auto team=std::find_if(current.world12_teams.begin(),current.world12_teams.end(),[&](const auto& row){return row.second==request.world12_team;});
        if(request.value!=-1||!request.world12_team.valid()||team==current.world12_teams.end())throw FormatError("world12: competition team is outside the visible fixtures");
        throw FormatError("Die Vereinsinfo dieser Wettbewerbsansicht ist noch nicht angebunden.");
    }
    const bool close=request.action==ui::OtherGamesAction::close;
    if(request.world12_team.valid()||(close&&request.value!=-1))throw FormatError("world12: unexpected competition action payload");
    auto candidate=state_;auto selection=other_games_selection_;
    if(!close)selection=ui::navigate_world12_other_games(candidate,identity,selection,request);
    auto next=identity;++next.revision;++next.sequence;
    const auto page=other_games_page(candidate,next,selection);
    auto& raw=*actor(candidate,identity.manager_slot).career->other_games_view;
    // Round arrows reset only the local participant cursor. Selector cycling
    // and display toggles are the Source writes to the per-seat saved byte.
    if(!close){
        if(request.action==ui::OtherGamesAction::select_competition)raw=uint8_t((raw&0x80)|(selection.participant_offset&0x7f));
        if(request.action==ui::OtherGamesAction::toggle_display_mode)raw^=0x80;
        if(page.projection.display_mode==ui::OtherGamesDisplayMode::detail)raw|=0x80;
    }
    candidate.validate();state_=std::move(candidate);++owner_.revision;
    if(close){preparation_menu_.reset();other_games_owner_.reset();}
    else {other_games_owner_=next;other_games_selection_=selection;other_games_selection_.mode=page.projection.display_mode;}
    return close;
}
}
