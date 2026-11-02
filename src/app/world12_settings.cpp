// SPDX-License-Identifier: GPL-3.0-only
#include "app/world12_session.hpp"
#include "app/settings_request_source.hpp"
#include "game/world12/desktop_projection.hpp"
#include "game/world12/economy.hpp"
#include "ui/desktop.hpp"
#include <algorithm>
namespace bmh::app::world12 {
namespace {
bool deferred(size_t index) {
    // H8 is the wall-clock deadline; R5 changes secondary-league admission.
    // Their typed runtime owners are separate from presentation preferences.
    return index==7*9+7||index==17*9+4;
}
void refresh(GameState& g,const LeImage& rules) {
    std::array<uint8_t,180> raw{};
    for(uint32_t i=0;i<180;++i)raw[i]=g.memory.u8(layout::LeaguePresentationOptions+i);
    const auto shown=ui::project_settings_availability(raw,
        {g.memory.u8(layout::GameMode),g.memory.u32(layout::CompetitionSelection),0,0},rules);
    for(uint32_t i=0;i<180;++i)if(i!=5&&i!=6)g.memory.set8(layout::LeaguePresentationOptions+i,shown[i]);
}
}
void Session::validate_settings_owner(uint64_t sequence,uint64_t revision)const {
    if(preparation_menu_!=19||!settings_owner_||sequence!=settings_generation_||!sequence||revision!=owner_.revision||revision==UINT64_MAX||
       !state_.world.economy||!state_.world.economy->presentation_options)throw FormatError("world12: stale settings window or unknown saved options");
    const auto p=bmh::world12::project_manager_desktop(state_,resources_.reference);
    if(p.state.memory.u8(layout::ActiveManager)!=settings_owner_->manager_slot)throw FormatError("world12: settings manager changed");
}
ui::SettingsState Session::settings_view()const {
    validate_settings_owner(settings_generation_,owner_.revision);
    const auto p=bmh::world12::project_manager_desktop(state_,resources_.reference);
    auto view=ui::settings_from_game(p.state,settings_owner_->manager_slot,owner_.revision,resources_.german,settings_sound_);
    if(!view.display_cells)view.display_cells=view.cells;
    (*view.display_cells)[7*9+7]=200;
    (*view.display_cells)[17*9+4]|=0x80;
    return view;
}
ui::SettingsState Session::open_settings(const ui::DeskRequest& request,std::optional<ui::SettingsSoundCapabilities> sound) {
    validate_file_owner(request.manager_slot,request.revision);
    if(request.kind!=ui::DeskRequestKind::open_menu||request.menu_kind!=19||request.value!=-1||request.item_id<0||request.item_id>=54||settings_generation_==UINT64_MAX||
       !state_.world.economy||!state_.world.economy->presentation_options)throw FormatError("world12: settings need their current menu and saved options");
    auto p=bmh::world12::project_manager_desktop(state_,resources_.reference);
    const auto desk=ui::desk_from_game(p.state,resources_.german,owner_.revision);
    const auto item=std::find_if(desk.items.begin(),desk.items.end(),[&](const auto& i){return i.id==request.item_id;});
    if(item==desk.items.end()||item->kind!=19||item->disabled)throw FormatError("world12: settings source item unavailable");
    auto candidate=*this;refresh(p.state,resources_.german);
    bmh::world12::capture_career_economy(candidate.state_.world,p);
    candidate.preparation_menu_=19;candidate.settings_owner_=request;candidate.settings_sound_=sound;
    ++candidate.owner_.revision;++candidate.settings_generation_;
    auto view=candidate.settings_view();candidate.state_.validate();*this=std::move(candidate);return view;
}
std::optional<Session::SettingsRefusal> Session::settings_refusal(const ui::SettingsWriteRequest& request,uint64_t sequence)const {
    validate_settings_owner(sequence,request.revision);
    if(request.manager_slot!=settings_owner_->manager_slot||request.row!=16||request.column!=4)return {};
    auto p=bmh::world12::project_manager_desktop(state_,resources_.reference);
    if(const auto notice=ui::settings_source_refusal(p.state,request,owner_.revision))return SettingsRefusal{std::move(p.state),*notice};
    return {};
}
void Session::apply_settings(const ui::SettingsWriteRequest& request,uint64_t sequence) {
    validate_settings_owner(sequence,request.revision);
    if(request.manager_slot!=settings_owner_->manager_slot||request.row>=20||request.column>=9||deferred(size_t(request.row)*9+request.column))throw FormatError("world12: settings field needs its separate runtime owner");
    const auto before=state_;auto candidate=state_;auto p=bmh::world12::project_manager_desktop(candidate,resources_.reference);auto revision=owner_.revision;std::string error;
    if(!apply_settings_request_source(p.state,resources_.german,settings_sound_,true,request,revision,error))throw FormatError(error);
    refresh(p.state,resources_.german);bmh::world12::capture_career_economy(candidate.world,p);candidate.validate();
    if(state_!=before)throw FormatError("world12: settings owner changed during edit");state_=std::move(candidate);owner_.revision=revision;
}
Bytes Session::prepare_settings_file(const ui::SaveLoadRequest& request,uint64_t sequence,const std::optional<Bytes>& incoming) {
    validate_settings_owner(sequence,request.revision);
    if(request.manager_slot!=settings_owner_->manager_slot||request.file_kind!=ui::SaveLoadFileKind::settings||request.origin_item_id!=settings_owner_->item_id||
       (request.action!=ui::SaveLoadAction::load&&request.action!=ui::SaveLoadAction::save))throw FormatError("world12: invalid settings file owner");
    const auto& raw=*state_.world.economy->presentation_options;Bytes bytes(raw.begin(),raw.end());auto candidate=state_;
    if(request.action==ui::SaveLoadAction::load){
        if(!incoming||incoming->size()!=180)throw FormatError("world12: EIN must contain exactly 180 bytes");
        for(size_t i=0;i<180;++i)if(deferred(i)&&(*incoming)[i]!=raw[i])throw FormatError("world12: EIN changes a setting with a separate runtime owner");
        bytes=*incoming;auto p=bmh::world12::project_manager_desktop(candidate,resources_.reference);
        // OpenBMH deviates from the original: typed EIN import also honors the
        // indoor-date refusal, so a file cannot invalidate owned event receipts.
        if((bytes[16*9+4]&0x7f)!=(raw[16*9+4]&0x7f)) {
            if((bytes[16*9+4]&0x7f)>1)throw FormatError("world12: EIN indoor setting is not a Source switch");
            ui::SettingsWriteRequest write{settings_owner_->manager_slot,owner_.revision,raw,16,4,ui::SettingsEdit::toggle_binary};
            auto revision=owner_.revision;std::string error;
            if(!apply_settings_request_source(p.state,resources_.german,settings_sound_,true,write,revision,error))throw FormatError(error);
        }
        for(uint32_t i=0;i<180;++i)p.state.memory.set8(layout::LeaguePresentationOptions+i,bytes[i]);
        refresh(p.state,resources_.german);bmh::world12::capture_career_economy(candidate.world,p);
    }else if(incoming)throw FormatError("world12: settings save has unexpected payload");
    candidate.validate();state_=std::move(candidate);++owner_.revision;return bytes;
}
void Session::close_settings(uint64_t sequence,uint64_t revision) {
    validate_settings_owner(sequence,revision);
    preparation_menu_.reset();settings_owner_.reset();settings_sound_.reset();++owner_.revision;
}
}
