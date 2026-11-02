// SPDX-License-Identifier: GPL-3.0-only
#include "app/world12_session.hpp"
#include "app/stadium_request_source.hpp"
#include "app/stadium_projection_source.hpp"
#include "game/world12/desktop_projection.hpp"
#include "game/world12/economy.hpp"
#include "ui/world12_desktop.hpp"
#include "formats/textd.hpp"
#include <algorithm>
#include <limits>
namespace bmh::app::world12 {
using namespace bmh::world12;
ui::StadiumState Session::stadium_view(const std::filesystem::path& data)const {
    if(preparation_menu_!=22)throw FormatError("world12: stadium menu is not open");
    auto projection=project_manager_desktop(state_,resources_.reference);const auto texts=load_textd(ui::asset_path(data,"TEXTD.BLK"));projection.state.texts=&texts;
    const auto desk=ui::world12_initial_desktop(data,state_,resources_.reference,resources_.german,options_,owner_.revision);
    // Typed clubs do not own stadium-name offsets into the reference install.
    // Use Source's club-name fallback rather than borrowing another club's stadium.
    projection.state.club_heap_bc.clear();projection.state.stadium_names.clear();
    auto out=stadium_projection_source(projection.state,resources_.german,texts,owner_.revision,desk,stadium_,[&](uint32_t at){return projection.state.memory.has(at)?projection.state.memory.u8(at):resources_.german.u8(at);});
    for(auto* notice:{&out.construction_tip_dialog,&out.periphery_quote_dialog})if(*notice){(*notice)->badge_sprite=desk.badge;(*notice)->badge_atlas=desk.badge_atlas;(*notice)->badge_fallback=false;}
    return out;
}
void Session::open_stadium(const ui::DeskRequest& request,const std::filesystem::path& data) {
    validate_file_owner(request.manager_slot,request.revision);
    if(request.kind!=ui::DeskRequestKind::open_menu||request.menu_kind!=22||owner_.revision==UINT64_MAX||!state_.world.transfer_market)
        throw FormatError("world12: stadium has no current menu owner");
    const auto before=state_;const auto revision=owner_.revision;auto candidate=state_;
    auto p=project_manager_desktop(candidate,resources_.reference);const auto texts=load_textd(ui::asset_path(data,"TEXTD.BLK"));p.state.texts=&texts;
    ui::StadiumRequest open;open.kind=ui::StadiumRequestKind::open;open.manager_slot=request.manager_slot;open.runtime_club=p.state.manager_teams.at(size_t(request.manager_slot));open.stable_club_identity=int(p.state.club_ids.at(size_t(open.runtime_club)));open.revision=revision;
    GameRandom game(candidate.game_seed);WatcomRandom random(candidate.watcom_seed);auto scratch=revision,next=next_stadium_scope_;auto runtime=stadium_;std::string error;
    if(!apply_stadium_request_source(p.state,resources_.german,game,random,runtime,next,open,scratch,error))throw FormatError(error);
    p.commit_match_state(candidate.world);capture_career_economy(candidate.world,p);candidate.game_seed=game.seed();candidate.watcom_seed=random.seed();candidate.validate();
    if(state_!=before||owner_.revision!=revision||preparation_menu_)throw FormatError("world12: stadium owner changed during opening");
    state_=std::move(candidate);++owner_.revision;stadium_=std::move(runtime);next_stadium_scope_=next;preparation_menu_=22;
}
void Session::apply_stadium(const std::filesystem::path& data,const ui::StadiumRequest& request,
    const ConstructionNoticeHook& notice,const SeasonTicketCompensationNoticeHook& compensation) {
    if(preparation_menu_!=22||request.kind==ui::StadiumRequestKind::open||request.revision!=owner_.revision||owner_.revision==UINT64_MAX||
       !request.stable_club_identity||request.scope!=stadium_.scope||request.sequence!=stadium_.sequence)
        throw FormatError("world12: stale stadium menu/request");
    const auto before=state_;const auto revision=owner_.revision,scope=stadium_.scope,sequence=stadium_.sequence,next_scope=next_stadium_scope_;auto candidate=state_;
    auto p=project_manager_desktop(candidate,resources_.reference);const auto texts=load_textd(ui::asset_path(data,"TEXTD.BLK"));p.state.texts=&texts;
    if(request.kind==ui::StadiumRequestKind::open_data){const auto seat=*candidate.desktop->active_seat;const auto& manager=*std::find_if(candidate.world.managers.begin(),candidate.world.managers.end(),[&](const auto& m){return m.seat==seat;});if(!manager.career||!manager.career->stadium_curve_flags)throw FormatError("world12: older career has unknown stadium curve preferences");}
    GameRandom game(candidate.game_seed);WatcomRandom random(candidate.watcom_seed);auto scratch=revision,next=next_stadium_scope_;auto runtime=stadium_;std::string error;
    if(!apply_stadium_request_source(p.state,resources_.german,game,random,runtime,next,request,scratch,error,notice,compensation))throw FormatError(error);
    p.commit_match_state(candidate.world);capture_career_economy(candidate.world,p);candidate.game_seed=game.seed();candidate.watcom_seed=random.seed();candidate.validate();
    if(state_!=before||owner_.revision!=revision||preparation_menu_!=22||stadium_.scope!=scope||stadium_.sequence!=sequence||next_stadium_scope_!=next_scope)
        throw FormatError("world12: stadium owner changed during calculation/presentation");
    state_=std::move(candidate);++owner_.revision;stadium_=std::move(runtime);next_stadium_scope_=next;
}
}
