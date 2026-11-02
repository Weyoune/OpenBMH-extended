// SPDX-License-Identifier: GPL-3.0-only
#include "app/world12_session.hpp"
#include "app/desktop_request_source.hpp"
#include "game/world12/desktop_projection.hpp"
#include "ui/world12_desktop.hpp"
#include <algorithm>
namespace bmh::app::world12 {
using namespace bmh::world12;
void Session::apply_desktop(const ui::DeskRequest& request,const std::filesystem::path& data) {
    validate_file_owner(request.manager_slot,request.revision);
    if(request.kind!=ui::DeskRequestKind::change_layout&&request.kind!=ui::DeskRequestKind::change_option&&request.kind!=ui::DeskRequestKind::change_ticket_price)
        throw FormatError("world12: unsupported desktop mutation");
    const auto before=state_;const auto revision=owner_.revision;auto candidate=state_;auto p=project_manager_desktop(candidate,resources_.reference);auto scratch=revision;std::string error;
    const auto desk=ui::world12_initial_desktop(data,candidate,resources_.reference,resources_.german,options_,revision);
    if(request.kind==ui::DeskRequestKind::change_layout){
        const auto valid=[&](const GameState& game,uint64_t rev){return ui::desk_from_game(game,resources_.german,rev);};
        if(!apply_desktop_layout_source(p.state,request,scratch,error,valid))throw FormatError(error);
        const auto seat=*candidate.desktop->active_seat;auto& saved=*std::find_if(candidate.desktop->managers.begin(),candidate.desktop->managers.end(),[&](const auto& d){return d.seat==seat;});
        const auto base=layout::Desktops+uint32_t(request.manager_slot)*0x360;
        for(size_t row=0;row<54;++row)for(size_t b=0;b<16;++b)saved.layout[row][b]=p.state.memory.u8(base+uint32_t(row*16+b));
    }else {
        if(!apply_desk_widget_source(p.state,desk,request,scratch,error))throw FormatError(error);
        p.commit_match_state(candidate.world);
    }
    candidate.validate();
    if(state_!=before||owner_.revision!=revision||preparation_menu_)throw FormatError("world12: desktop owner changed during mutation");
    state_=std::move(candidate);++owner_.revision;
}
}
