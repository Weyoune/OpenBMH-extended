// SPDX-License-Identifier: GPL-3.0-only
#include "app/world12_session.hpp"
#include "game/world12/desktop_projection.hpp"
#include "ui/desktop.hpp"
#include "ui/other_games.hpp"
#include "app/world12_visible_menu.hpp"
#include <algorithm>
namespace bmh::app::world12 {
namespace {
bmh::world12::Manager& actor(bmh::world12::CareerSnapshot& state,uint8_t seat) {
    auto found=std::find_if(state.world.managers.begin(),state.world.managers.end(),[&](const auto& m){return m.seat==seat;});
    if(found==state.world.managers.end())throw FormatError("world12: competition manager state is unavailable");return *found;
}
std::optional<uint8_t>& other_preference(bmh::world12::CareerSnapshot& state,uint8_t seat){
    auto& person=actor(state,seat);if(person.career)return person.career->other_games_view;
    if(state.world.catalogue.scope.club_play()||!state.world.national_coaches)throw FormatError("world12: actual competition preference owner is unknown");
    const auto coach=std::find_if(state.world.national_coaches->begin(),state.world.national_coaches->end(),[&](const auto& record){return record.active&&record.coach==seat;});if(coach==state.world.national_coaches->end())throw FormatError("world12: active national competition preference owner is missing");if(!coach->view_preferences)coach->view_preferences.emplace();return coach->view_preferences->other_games_view;
}
}
ui::OtherGamesState Session::other_games_page(const bmh::world12::CareerSnapshot& state,
    const ui::World12OtherGamesIdentity& identity,ui::World12OtherSelection selection)const {
    auto out=ui::world12_other_games_state(state,identity,selection,{},resources_.texts.get());
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
    if(!visible_world12_menu(desk,request.item_id,5))throw FormatError("world12: competition overview icon is not enabled");
    auto candidate=state_;const auto seat=*candidate.desktop->active_seat;
    // CupGroupOverview begins with detail and participant zero, even for a
    // previous save whose display byte was not yet represented by its schema.
    other_preference(candidate,seat)=0x80;
    const auto local=p.state.memory.u8(layout::ActiveManager);const auto projected_team=p.global_club(uint32_t(p.state.manager_teams.at(local)));
    const ui::World12OtherGamesIdentity identity{seat,projected_team,owner_.revision+1,owner_.scope,(other_games_generation_+1)<<32};
    auto selection=ui::default_world12_other_selection(candidate.world);if(national_desktop_active())selection.page=ui::World12OtherPage::national_qualifiers;
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
bool Session::apply_other_games(const ui::OtherGamesRequest& request,const std::filesystem::path& data,const ClubInformationPresenter& presenter) {
    const auto current=other_games_view();const auto identity=*other_games_owner_;
    if(request.world12_identity!=other_games_owner_||request.identity!=ui::OtherGamesIdentity{}||
       owner_.revision==UINT64_MAX||identity.sequence==UINT64_MAX||((identity.sequence+1)>>32)!=other_games_generation_)
        throw FormatError("world12: stale competition overview action");
    if(request.action==ui::OtherGamesAction::team_info){
        const auto team=std::find_if(current.world12_teams.begin(),current.world12_teams.end(),[&](const auto& row){return row.second==request.world12_team;});
        if(request.value!=-1||!request.world12_team.valid()||team==current.world12_teams.end())throw FormatError("world12: competition team is outside the visible fixtures");
        if(!presenter)throw FormatError("world12: club information presenter is unavailable");
    }
    const bool close=request.action==ui::OtherGamesAction::close;
    if((request.action!=ui::OtherGamesAction::team_info&&request.world12_team.valid())||(close&&request.value!=-1))throw FormatError("world12: unexpected competition action payload");
    auto candidate=state_;auto selection=other_games_selection_;
    if(request.action==ui::OtherGamesAction::team_info){
        const auto before=state_;const auto old_generation=other_games_generation_;const auto old_selection=other_games_selection_;
        if(!resources_.texts)throw FormatError("world12: club information texts are unavailable");
        inspect_club_information(candidate,resources_.reference,resources_.german,*resources_.texts,request.world12_team,presenter,ui::OtherGamesScreen(data,current,resources_.german).render());
        if(state_!=before||preparation_menu_!=5||other_games_owner_!=identity||other_games_generation_!=old_generation||
           other_games_selection_!=old_selection||owner_.revision!=identity.revision)
            throw FormatError("world12: club information changed its live competition owner");
    }else if(!close)selection=ui::navigate_world12_other_games(candidate,identity,selection,request,resources_.texts.get());
    auto next=identity;++next.revision;++next.sequence;
    const auto page=other_games_page(candidate,next,selection);
    auto& preference=other_preference(candidate,identity.manager_slot);if(!preference)throw FormatError("world12: competition preference was not initialized by its owned opening");auto& raw=*preference;
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
