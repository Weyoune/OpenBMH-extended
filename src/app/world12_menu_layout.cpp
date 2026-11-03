// SPDX-License-Identifier: GPL-3.0-only
#include "app/world12_session.hpp"
#include "app/menu_layout_source.hpp"
#include "ui/save_load.hpp"
#include "game/world12/desktop_projection.hpp"
#include <algorithm>
namespace bmh::app::world12 {
using namespace bmh::world12;
Bytes Session::prepare_menu_layout(const ui::SaveLoadRequest& request,const std::optional<Bytes>& incoming) {
    validate_file_owner(request.manager_slot,request.revision);
    if(request.file_kind!=ui::SaveLoadFileKind::menu_layout||request.origin_item_id!=53||owner_.revision==UINT64_MAX||
       (request.action!=ui::SaveLoadAction::load&&request.action!=ui::SaveLoadAction::save))throw FormatError("world12: invalid menu-layout owner");
    const auto before=state_;const auto revision=owner_.revision;auto candidate=state_;
    const auto seat=*candidate.desktop->active_seat;auto& desk=*std::find_if(candidate.desktop->managers.begin(),candidate.desktop->managers.end(),[&](const auto& d){return d.seat==seat;});
    auto* owned_layout=&desk.layout;
    const auto current=project_manager_desktop(candidate,resources_.reference);
    if(!current.state.projected_national_teams.empty()){
        if(!candidate.world.national_coaches)throw FormatError("world12: national menu layout has no owner");
        const auto role=std::find_if(candidate.world.national_coaches->begin(),candidate.world.national_coaches->end(),[&](const auto& value){return value.active&&value.coach==seat;});
        if(role==candidate.world.national_coaches->end()||!role->desktop_layout)throw FormatError("world12: national menu layout history is unknown");
        owned_layout=&*role->desktop_layout;
    }
    if((*owned_layout)[53][12]!=72)throw FormatError("world12: menu-layout Source handle no longer exists");
    Bytes bytes;
    if(request.action==ui::SaveLoadAction::load){if(!incoming)throw FormatError("world12: layout load has no payload");bytes=*incoming;}
    else {if(incoming)throw FormatError("world12: layout save contains unexpected input");for(const auto& row:*owned_layout)bytes.insert(bytes.end(),row.begin(),row.end());}
    std::string error;if(!validate_menu_layout_source(bytes,error))throw FormatError(error);
    if(request.action==ui::SaveLoadAction::load)for(size_t row=0;row<54;++row)std::copy_n(bytes.begin()+ptrdiff_t(row*16),16,(*owned_layout)[row].begin());
    auto p=project_manager_desktop(candidate,resources_.reference);(void)ui::desk_from_game(p.state,resources_.german,revision+1);candidate.validate();
    if(state_!=before||owner_.revision!=revision||preparation_menu_)throw FormatError("world12: menu-layout owner changed during preparation");
    state_=std::move(candidate);++owner_.revision;return bytes;
}
}
