// SPDX-License-Identifier: GPL-3.0-only
#include "app/world12_live_settings.hpp"
#include "app/settings_request_source.hpp"
namespace bmh::app::world12 {
namespace {
std::array<uint8_t,180> cells(const GameState& state){
    std::array<uint8_t,180> raw{};
    for(uint32_t i=0;i<raw.size();++i)raw[i]=state.memory.u8(layout::LeaguePresentationOptions+i);
    return raw;
}
}
LiveSettingsScope::LiveSettingsScope(bmh::world12::AiMatchProjection& projection,MatchEngine& engine,
    const LeImage& rules,int minute,uint64_t& nonce,std::optional<ui::SettingsSoundCapabilities> sound)
    :projection_(projection),engine_(engine),rules_(rules),sound_(sound),nonce_(nonce),before_(projection.state),
     groups_(engine.groups),buttons_before_(engine.buttons),desk_before_(engine.live_desk_disabled),
     game_seed_(engine.game.seed()),random_seed_(engine.rand.seed()),game_calls_(engine.game.calls()),
     random_calls_(engine.rand.calls()),storage_(engine.groups.data()) {
    if(&engine.state!=&projection.state||nonce>=UINT64_MAX-2||minute<0||minute>125||groups_.empty())
        throw FormatError("world12: live settings have no retained minute owner");
    manager_=projection.state.memory.u8(layout::ActiveManager);
    if(manager_<0||size_t(manager_)>=projection.human_managers())throw FormatError("world12: live settings have no actual human viewer");
    auto candidate=projection.state;refresh(candidate);revision_=++nonce_;publish(std::move(candidate));
}
LiveSettingsScope::~LiveSettingsScope(){if(!closed_)try{abort();}catch(...) {}}
void LiveSettingsScope::validate()const {
    if(closed_||nonce_!=revision_||nonce_>=UINT64_MAX-1||&engine_.state!=&projection_.state||
       engine_.groups.data()!=storage_||engine_.groups.size()!=groups_.size()||engine_.buttons!=buttons_||
       engine_.game.seed()!=game_seed_||engine_.rand.seed()!=random_seed_||engine_.game.calls()!=game_calls_||engine_.rand.calls()!=random_calls_||
       projection_.state.memory.u8(layout::ActiveManager)!=manager_)
        throw FormatError("world12: stale or externally changed live settings owner");
    for(size_t i=0;i<groups_.size();++i)if(engine_.groups[i].desc!=groups_[i].desc||engine_.groups[i].fixtures!=groups_[i].fixtures)
        throw FormatError("world12: live settings changed a retained match");
}
void LiveSettingsScope::refresh(GameState& state)const {
    const auto raw=cells(state);const auto shown=ui::project_settings_availability(raw,
        {state.memory.u8(layout::GameMode),state.memory.u32(layout::CompetitionSelection),0,0},rules_);
    for(uint32_t i=0;i<raw.size();++i)if(i!=5&&i!=6)state.memory.set8(layout::LeaguePresentationOptions+i,shown[i]);
}
void LiveSettingsScope::publish(GameState candidate){
    const auto raw=cells(candidate);std::vector<uint8_t> buttons(raw.begin(),raw.end()),expected=buttons;
    projection_.state=std::move(candidate);buttons_=std::move(expected);engine_.buttons=std::move(buttons);
    engine_.live_desk_disabled=projection_.state.memory.has(layout::LiveDeskDisabled)?
        projection_.state.memory.u8(layout::LiveDeskDisabled)!=0:rules_.u8(layout::LiveDeskDisabled)!=0;
}
ui::SettingsState LiveSettingsScope::view()const {
    validate();auto out=ui::settings_from_game(projection_.state,manager_,revision_,rules_,sound_);
    if(!out.display_cells)out.display_cells=out.cells;(*out.display_cells)[17*9+4]|=0x80;return out;
}
void LiveSettingsScope::apply(const ui::SettingsWriteRequest& request){
    validate();if(request.row==17&&request.column==4)throw FormatError("world12: secondary-league admission needs its saved calendar owner");
    auto candidate=projection_.state;auto revision=revision_;std::string error;
    if(!apply_settings_request_source(candidate,rules_,sound_,true,request,revision,error))throw FormatError(error);
    refresh(candidate);publish(std::move(candidate));revision_=++nonce_;
}
Bytes LiveSettingsScope::file(const ui::SaveLoadRequest& request,const std::optional<Bytes>& incoming){
    validate();if(request.manager_slot!=manager_||request.revision!=revision_||request.origin_item_id!=-1||
       request.file_kind!=ui::SaveLoadFileKind::settings||
       (request.action!=ui::SaveLoadAction::save&&request.action!=ui::SaveLoadAction::load))throw FormatError("world12: settings file has another live owner");
    const auto raw=cells(projection_.state);Bytes bytes(raw.begin(),raw.end());auto candidate=projection_.state;
    if(request.action==ui::SaveLoadAction::load){
        if(!incoming||incoming->size()!=180)throw FormatError("world12: EIN must contain exactly 180 bytes");
        if((*incoming)[17*9+4]!=raw[17*9+4])throw FormatError("world12: EIN changes an unowned secondary-league switch");
        if(((*incoming)[16*9+4]&0x7f)!=(raw[16*9+4]&0x7f)){
            if(((*incoming)[16*9+4]&0x7f)>1)throw FormatError("world12: invalid EIN indoor switch");
            ui::SettingsWriteRequest check{manager_,revision_,raw,16,4,ui::SettingsEdit::toggle_binary};auto revision=revision_;std::string error;
            if(!apply_settings_request_source(candidate,rules_,sound_,true,check,revision,error))throw FormatError(error);
        }
        bytes=*incoming;for(uint32_t i=0;i<bytes.size();++i)candidate.memory.set8(layout::LeaguePresentationOptions+i,bytes[i]);refresh(candidate);
    }else {if(incoming)throw FormatError("world12: settings export received an unexpected payload");return bytes;}
    publish(std::move(candidate));revision_=++nonce_;return bytes;
}
void LiveSettingsScope::finish(){validate();projection_.retain_live_settings(cells(projection_.state));++nonce_;closed_=true;}
void LiveSettingsScope::abort(){if(closed_)return;projection_.state=std::move(before_);engine_.buttons=std::move(buttons_before_);engine_.live_desk_disabled=desk_before_;if(nonce_!=UINT64_MAX)++nonce_;closed_=true;}
}
