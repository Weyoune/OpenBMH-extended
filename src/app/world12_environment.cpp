// SPDX-License-Identifier: GPL-3.0-only
#include "app/world12_session.hpp"
#include "game/world12/desktop_projection.hpp"
#include "formats/textd.hpp"
#include <algorithm>
#include <limits>
namespace bmh::app::world12 {
namespace {
ui::EnvironmentView view(const bmh::world12::CareerSnapshot& state,const GameState& reference,
    uint64_t revision,const ui::DeskRequest& owner,const std::filesystem::path& data) {
    auto p=bmh::world12::project_manager_desktop(state,reference);
    const auto texts=load_textd(ui::asset_path(data,"TEXTD.BLK"));p.state.texts=&texts;
    auto out=ui::environment_from_game(p.state,revision);out.owner=owner;
    const auto seat=*state.desktop->active_seat;
    const auto& manager=*std::find_if(state.world.managers.begin(),state.world.managers.end(),[&](const auto& m){return m.seat==seat;});
    const auto& crest=state.world.crests.at(state.world.clubs.at(manager.club).crest);out.badge_sprite=crest.cell;out.badge_atlas=crest.path;
    return out;
}
}
ui::EnvironmentView Session::open_environment(const ui::DeskRequest& request,const std::filesystem::path& data) {
    validate_file_owner(request.manager_slot,request.revision);
    if(request.kind!=ui::DeskRequestKind::open_menu||request.menu_kind!=24||!environment_rules_supported()||environment_generation_>=UINT32_MAX)
        throw FormatError("world12: environment has no supported menu/rule owner");
    auto result=view(state_,resources_.reference,owner_.revision,request,data);
    result.request_id_base=(environment_generation_+1)<<32;
    environment_owner_=request;++environment_generation_;environment_request_=result.request_id_base;preparation_menu_=24;return result;
}
ui::EnvironmentView Session::environment_view(const std::filesystem::path& data)const {
    if(preparation_menu_!=24||!environment_owner_)throw FormatError("world12: no open environment owner");
    auto result=view(state_,resources_.reference,owner_.revision,*environment_owner_,data);result.request_id_base=environment_generation_<<32;return result;
}
bool Session::apply_environment(const ui::EnvironmentRequest& request) {
    if(preparation_menu_!=24||!environment_owner_||request.owner!=*environment_owner_||
       request.kind!=ui::EnvironmentRequestKind::set_item||request.revision!=owner_.revision||
       !request.request_id||request.request_id<=environment_request_||(request.request_id>>32)!=environment_generation_||owner_.revision==std::numeric_limits<uint64_t>::max())
        throw FormatError("world12: stale environment request/menu owner");
    const auto before=state_;const auto revision=owner_.revision;auto candidate=state_;
    auto p=bmh::world12::project_manager_desktop(candidate,resources_.reference);
    if(request.manager_slot!=p.state.memory.u8(layout::ActiveManager)||request.manager_slot<0||
       size_t(request.manager_slot)>=p.human_managers()||request.runtime_club!=p.state.manager_teams[size_t(request.manager_slot)])
        throw FormatError("world12: environment request belongs to another manager");
    std::string message;const auto result=set_environment(p.state,request.manager_slot,request.item,request.level,&message);
    if(result==EnvironmentResult::invalid)throw FormatError("world12: invalid environment budget");
    if(!message.empty())throw FormatError("world12: environment change requires its Source notice presenter");
    const bool changed=result==EnvironmentResult::changed;
    if(changed){p.commit_match_state(candidate.world);candidate.validate();}
    if(state_!=before||owner_.revision!=revision||preparation_menu_!=24||request.owner!=*environment_owner_)
        throw FormatError("world12: environment owner changed during calculation");
    if(changed){state_=std::move(candidate);++owner_.revision;}
    environment_owner_->revision=owner_.revision;environment_request_=request.request_id;return changed;
}
}
