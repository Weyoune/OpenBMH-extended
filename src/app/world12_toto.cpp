// SPDX-License-Identifier: GPL-3.0-only
#include "app/world12_session.hpp"
#include <algorithm>
#include "app/toto_request_source.hpp"
#include "game/world12/desktop_projection.hpp"
#include "game/world12/economy.hpp"
#include "ui/world12_desktop.hpp"
namespace bmh::app::world12 {
using namespace bmh::world12;
namespace {
AiMatchProjection coupon_workspace(const CareerSnapshot& state,const GameState& reference) {
    if(!state.world.economy||!state.world.economy->pools)throw FormatError("world12: older career has unknown Toto coupons");
    std::vector<Fixture> matches;
    if(state.prepared_event){if(!state.prepared_event->pools_matches)throw FormatError("world12: scheduled Toto selection is unknown");matches=*state.prepared_event->pools_matches;}
    std::vector<ClubId> clubs;for(const auto& match:matches)for(auto id:{match.home,match.away})if(std::find(clubs.begin(),clubs.end(),id)==clubs.end())clubs.push_back(id);
    auto p=project_manager_desktop(state,reference,clubs);p.state.memory.add_block(layout::DeskActions_SubmitPoolsCoupon_RuntimeField,44);p.state.memory.add_block(layout::CareerSession_DispatchToto_RuntimeField,1);
    p.state.memory.set8(layout::CareerSession_DispatchToto_RuntimeField,uint8_t(matches.size()));
    for(size_t n=0;n<matches.size();++n){p.state.memory.set16(layout::DeskActions_SubmitPoolsCoupon_RuntimeField+uint32_t(n*4),uint16_t(p.local_club(matches[n].home)));p.state.memory.set16(layout::DeskActions_SubmitPoolsCoupon_RuntimeField+uint32_t(n*4+2),uint16_t(p.local_club(matches[n].away)));}
    return p;
}
}
void Session::open_toto(const ui::DeskRequest& request,const std::filesystem::path& data) {
    validate_file_owner(request.manager_slot,request.revision);
    if(request.kind!=ui::DeskRequestKind::open_menu||request.menu_kind!=13||toto_generation_>=UINT32_MAX||owner_.revision==UINT64_MAX)throw FormatError("world12: invalid Toto owner");
    (void)coupon_workspace(state_,resources_.reference);++toto_generation_;toto_request_=toto_generation_<<32;preparation_menu_=13;++owner_.revision;
}
ui::TotoState Session::toto_view(const std::filesystem::path& data)const {
    if(preparation_menu_!=13)throw FormatError("world12: Toto is not open");auto p=coupon_workspace(state_,resources_.reference);
    auto out=ui::toto_from_game(p.state,owner_.revision);out.source="world12";out.request_id_base=toto_request_+1;return out;
}
void Session::apply_toto(const ui::TotoRequest& request) {
    if(preparation_menu_!=13||request.revision!=owner_.revision||owner_.revision==UINT64_MAX||request.request_id<=toto_request_||(request.request_id>>32)!=toto_generation_||request.request_id==UINT64_MAX)throw FormatError("world12: stale Toto request/window");
    const auto before=state_;const auto revision=owner_.revision,generation=toto_generation_,last=toto_request_;auto candidate=state_;auto p=coupon_workspace(candidate,resources_.reference);auto scratch=revision;std::string error;
    if(!apply_toto_request_source(p.state,request,scratch,error))throw FormatError(error);
    p.commit_match_state(candidate.world);capture_career_economy(candidate.world,p);
    auto& pools=*candidate.world.economy->pools;
    if(std::any_of(pools.managers.begin(),pools.managers.end(),[](const auto& c){return c.stake!=0;})) {
        const auto appointment=std::make_pair(candidate.season,candidate.date);
        if(pools.appointment&&*pools.appointment!=appointment)throw FormatError("world12: pending Toto stake belongs to another appointment");pools.appointment=appointment;
    } else pools.appointment.reset();candidate.validate();
    if(state_!=before||owner_.revision!=revision||preparation_menu_!=13||toto_generation_!=generation||toto_request_!=last)throw FormatError("world12: Toto owner changed during submission");
    state_=std::move(candidate);++owner_.revision;toto_request_=request.request_id;
}
}
