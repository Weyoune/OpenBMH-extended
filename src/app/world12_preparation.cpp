// SPDX-License-Identifier: GPL-3.0-only
#include "app/world12_session.hpp"
#include "app/preparation_actions.hpp"
#include "app/training_camp_request_source.hpp"
#include "game/world12/economy.hpp"
#include "game/world12/desktop_projection.hpp"
#include "game/desk_actions.hpp"
#include "game/lineup.hpp"
#include "ui/world12_preparation.hpp"
#include "ui/desktop.hpp"
#include <algorithm>
#include <limits>
#include <type_traits>
namespace bmh::app::world12 {
using namespace bmh::world12;
namespace {
void actor(const GameState& local,int manager,int club,uint64_t revision,uint64_t expected) {
    if(revision!=expected||manager!=local.memory.u8(layout::ActiveManager)||manager<0||size_t(manager)>=local.manager_teams.size()||local.manager_teams[size_t(manager)]!=club)
        throw FormatError("world12: stale preparation actor/revision");
}
void row_identity(const GameState& local,int club,int row,int player,uint16_t slot) {
    if(row<0||row>=24||local.squad_slots.at(squad_range(club).first+size_t(row))!=slot||((slot&0xfff)==0xfff?-1:int(slot&0xfff))!=player)
        throw FormatError("world12: stale preparation player/row");
}
void prepare_lineup(GameState& local,int manager,int club) {
    auto_lineup(local,club,0xe4);prepare_lineup_entry(local,club);open_lineup_screen(local,manager);
    if(!initialize_pitch_set_piece_source(local,manager,club))throw FormatError("world12: set-piece initialization failed");
    const auto display=layout::CareerSession_AnyOf_ExecutableField+uint32_t(manager*2);
    if((local.memory.u16(display)&0x80)&&!normalize_pitch_set_piece_source(local,manager,club))throw FormatError("world12: set-piece reentry failed");
    std::array<bool,4096> seen{};const auto first=squad_range(club).first;
    for(size_t row=0;row<11;++row){const auto slot=local.squad_slots.at(first+row);const auto id=slot&0xfff;const auto at=size_t(manager)*0x840+row*0x58;
        if(id==0xfff||size_t(id)>=local.players.size()||seen[id]||!(slot&0x8000)||(slot&0x4000)||int8_t(local.tactics.at(at+0x41))<0||int8_t(local.tactics.at(at+0x41))>19||int8_t(local.tactics.at(at+0x42))<0||int8_t(local.tactics.at(at+0x42))>8)
            throw FormatError("world12: incomplete prepared eleven");seen[id]=true;}
}
void pitch_action(GameState& local,const ui::PitchRequest& r,bool foreign_rule) {
    if(r.indoor!=local.indoor)throw FormatError("world12: stale pitch mode");
    const auto at=size_t(r.manager_slot)*0xb0c;const auto display=layout::CareerSession_AnyOf_ExecutableField+uint32_t(r.manager_slot*2);
    const auto word=[&](size_t p){return uint16_t(local.manager_records.at(at+p)|uint16_t(local.manager_records.at(at+p+1))<<8);};
    const std::array<uint16_t,3> takers{word(0xafc),word(0xafe),word(0xb08)};const auto old=local.memory.u16(display);const auto mode=local.memory.u8(layout::GameMode);
    const bool editor=r.kind==ui::PitchRequestKind::open_set_piece_editor||r.kind==ui::PitchRequestKind::close_set_piece_editor||r.kind==ui::PitchRequestKind::set_piece_taker||r.kind==ui::PitchRequestKind::auto_set_piece_takers;
    if(editor||r.kind==ui::PitchRequestKind::select_pitch_page||r.kind==ui::PitchRequestKind::set_player_order){
        if(r.expected_display_word!=old||r.expected_set_piece_values!=takers||r.expected_set_piece_signature!=word(0xb0a)||r.expected_mode!=mode)throw FormatError("world12: stale pitch page/takers");
    }
    if(editor&&(word(0xb0a)!=0x3253||(r.kind!=ui::PitchRequestKind::open_set_piece_editor&&!(old&0x80))))throw FormatError("world12: inactive set-piece editor");
    bool accepted=false;
    switch(r.kind){
    case ui::PitchRequestKind::move_player:
        accepted=move_pitch_player_source(local,r.manager_slot,r.runtime_club,r.row,r.player_id,r.slot,r.old_depth,r.old_lane,r.new_depth,r.new_lane);break;
    case ui::PitchRequestKind::swap_players:
        row_identity(local,r.runtime_club,r.row,r.player_id,r.slot);row_identity(local,r.runtime_club,r.second_row,r.second_player,r.second_slot);
        accepted=swap_pitch_roster_rows_source(local,r.manager_slot,r.runtime_club,r.row,r.second_row,r.slot,r.second_slot,foreign_rule);break;
    case ui::PitchRequestKind::set_automatic_weight:
        accepted=set_pitch_automatic_weight_source(local,r.manager_slot,r.runtime_club,r.weight_index,r.old_weights,r.new_weights);break;
    case ui::PitchRequestKind::apply_formation:
        accepted=apply_pitch_formation_source(local,r.manager_slot,r.runtime_club,r.formation,r.old_weights);break;
    case ui::PitchRequestKind::save_formation:
        accepted=save_pitch_formation_source(local,r.manager_slot,r.runtime_club,r.formation,r.old_weights,r.formation_positions);break;
    case ui::PitchRequestKind::open_set_piece_editor: {
        const auto next=uint16_t((old&0xff00)|((old&0x8f)|0x80));accepted=r.new_display_word==next&&normalize_pitch_set_piece_source(local,r.manager_slot,r.runtime_club);if(accepted)local.memory.set16(display,next);break;}
    case ui::PitchRequestKind::close_set_piece_editor:
        accepted=r.new_display_word==uint16_t(old&~uint16_t(0x80));if(accepted)local.memory.set16(display,r.new_display_word);break;
    case ui::PitchRequestKind::set_piece_taker:
        accepted=r.set_piece_selector>=2&&r.set_piece_selector<=4&&r.expected_set_piece_value==takers[size_t(r.set_piece_selector-2)]&&set_pitch_set_piece_taker_source(local,r.manager_slot,r.runtime_club,r.set_piece_selector,r.row,r.player_id,r.slot,r.expected_set_piece_value);break;
    case ui::PitchRequestKind::auto_set_piece_takers:
        accepted=auto_pitch_set_piece_takers_source(local,r.manager_slot,r.runtime_club,r.expected_set_piece_values);break;
    case ui::PitchRequestKind::set_player_order:
        row_identity(local,r.runtime_club,r.row,r.player_id,r.slot);
        if(mode==1||(old&0x70)!=0x20||r.row>=11||r.order_aspect<0||r.order_aspect>3||r.new_order_value<2||r.new_order_value>40||r.new_order_value==r.expected_order_value||local.tactics.at(size_t(r.manager_slot)*0x840+size_t(r.row)*0x58+2+size_t(r.order_aspect))!=r.expected_order_value)break;
        accepted=set_player_order_source(local,r.manager_slot,r.runtime_club,r.row,r.player_id,r.order_aspect,r.new_order_value);break;
    case ui::PitchRequestKind::select_pitch_page: {
        uint16_t next=old;
        switch(r.page_control){case 0:next=uint16_t((old&~uint16_t(0x80))^3);break;case 1:next=uint16_t(old^12);break;case 2:next=uint16_t((old&0x8f)|0x10);break;case 3:if(mode!=1)next=uint16_t((old&0x8f)|0x20);else throw FormatError("world12: player orders unavailable in mode I");break;default:throw FormatError("world12: no authenticated opponent for marking page");}
        // Page 2/3 clears editor bit 80, preserving the Original low-word mask.
        if(r.page_control==2||r.page_control==3)next=uint16_t(next&~uint16_t(0x80));
        accepted=r.new_display_word==next;if(accepted)local.memory.set16(display,next);break;}
    default:throw FormatError("world12: unsupported pitch command");
    }
    if(!accepted)throw FormatError("world12: rejected stale or unavailable pitch command");
}
void capture_display(CareerSnapshot& candidate,const AiMatchProjection& projection) {
    for(size_t i=0;i<projection.human_managers();++i){const auto seat=projection.global_manager(uint8_t(i));auto& desk=*std::find_if(candidate.desktop->managers.begin(),candidate.desktop->managers.end(),[&](const auto& d){return d.seat==seat;});
        desk.display_word=projection.state.memory.u16(layout::CareerSession_AnyOf_ExecutableField+uint32_t(i*2));}
}
}
void Session::open_preparation(const ui::DeskRequest& request) {
    if(preparation_menu_||request.kind!=ui::DeskRequestKind::open_menu||(request.menu_kind!=3&&request.menu_kind!=18&&request.menu_kind!=16&&request.menu_kind!=14&&request.menu_kind!=15)||owner_.revision==std::numeric_limits<uint64_t>::max())throw FormatError("world12: unsupported or occupied preparation menu");
    const auto before=state_;auto candidate=state_;auto projection=project_manager_desktop(candidate,resources_.reference);auto& local=projection.state;
    actor(local,request.manager_slot,request.manager_slot>=0&&size_t(request.manager_slot)<local.manager_teams.size()?local.manager_teams[size_t(request.manager_slot)]:-1,request.revision,owner_.revision);
    const auto team=local.manager_teams.at(size_t(request.manager_slot));
    if(request.menu_kind==15) {
        const auto seat=projection.global_manager(uint8_t(request.manager_slot));
        const auto& manager=*std::find_if(candidate.world.managers.begin(),candidate.world.managers.end(),[&](const auto& m){return m.seat==seat;});
        if(!manager.career||!manager.career->camp_focus)throw FormatError("world12: old career lacks saved camp focus; start a current fresh career");
        const auto camp=ui::training_camp_from_game(local,resources_.german,request.manager_slot,owner_.revision);
        if(camp.focus_normalization_required&&!set_training_camp_focus(local,request.manager_slot,camp.focus))throw FormatError("world12: camp entry normalization failed");
    }
    if(request.menu_kind==3)prepare_lineup(local,request.manager_slot,team);
    if(request.menu_kind==16){if(options_.mode<2)throw FormatError("world12: training unavailable in mode I");const auto training=ui::training_from_game(local,resources_.german,owner_.revision);
        if(training.needs_initialization()){ui::TrainingRequest r;r.kind=ui::TrainingRequestKind::initialize;r.manager_slot=request.manager_slot;r.runtime_club=team;r.revision=owner_.revision;r.desired_types=training.types;r.desired_areas=training.areas;r.desired_automation=0x9f;
            auto revision=owner_.revision;std::string error;if(!apply_training_request_source(local,resources_.german,r,revision,error))throw FormatError(error);}}
    projection.commit_match_state(candidate.world);capture_display(candidate,projection);candidate.validate();
    if(state_!=before||request.revision!=owner_.revision)throw FormatError("world12: preparation owner changed during opening");
    state_=std::move(candidate);++owner_.revision;preparation_menu_=request.menu_kind;
}
void Session::apply_preparation(const ui::PreparationRequest& request) {
    if(!preparation_menu_||owner_.revision==std::numeric_limits<uint64_t>::max())throw FormatError("world12: no preparation command owner");
    const auto before=state_;const auto revision=owner_.revision;auto candidate=state_;auto projection=project_manager_desktop(candidate,resources_.reference);auto& local=projection.state;
    WatcomRandom random(candidate.watcom_seed);bool camp_changed=false;
    std::visit([&](const auto& r){using R=std::decay_t<decltype(r)>;
        if constexpr(std::is_same_v<R,ui::LineupRequest>||std::is_same_v<R,ui::PitchRequest>||std::is_same_v<R,ui::TacticsRequest>||std::is_same_v<R,ui::TrainingRequest>){
            actor(local,r.manager_slot,r.runtime_club,r.revision,revision);
            if constexpr(std::is_same_v<R,ui::LineupRequest>){if(*preparation_menu_!=3||r.kind!=ui::LineupRequestKind::swap_players)throw FormatError("world12: invalid lineup command");
                row_identity(local,r.runtime_club,r.first_row,r.first_player,r.first_slot);row_identity(local,r.runtime_club,r.second_row,r.second_player,r.second_slot);
                if(!swap_pitch_roster_rows_source(local,r.manager_slot,r.runtime_club,r.first_row,r.second_row,r.first_slot,r.second_slot,state_.world.settings->foreign_rule))throw FormatError("world12: roster swap requires a separate injury/foreign-rule decision");}
            else if constexpr(std::is_same_v<R,ui::PitchRequest>){if(*preparation_menu_!=3)throw FormatError("world12: pitch has no lineup owner");pitch_action(local,r,state_.world.settings->foreign_rule);}
            else {auto scratch=revision;std::string error;
                if constexpr(std::is_same_v<R,ui::TacticsRequest>){if(*preparation_menu_!=18||!apply_tactics_request_source(local,resources_.german,r,scratch,error))throw FormatError(error.empty()?"world12: tactic menu mismatch":error);}
                else {if(*preparation_menu_!=16||!apply_training_request_source(local,resources_.german,r,scratch,error))throw FormatError(error.empty()?"world12: training menu mismatch":error);}
            }
        } else if constexpr(std::is_same_v<R,ui::TrainingCampRequest>) {
            if(*preparation_menu_!=15)throw FormatError("world12: camp command has no current menu owner");
            actor(local,r.manager_slot,r.runtime_club,r.revision,revision);auto scratch=revision;std::string error;
            if(!apply_training_camp_request_source(local,random,r,scratch,error))throw FormatError(error.empty()?"world12: camp command rejected":error);
            camp_changed=true;
        } else throw FormatError("world12: unsupported preparation command");
    },request);
    projection.commit_match_state(candidate.world);capture_display(candidate,projection);
    if(camp_changed)capture_career_economy(candidate.world,projection);
    candidate.watcom_seed=random.seed();candidate.validate();
    if(state_!=before||owner_.revision!=revision)throw FormatError("world12: preparation owner changed during command");state_=std::move(candidate);++owner_.revision;
}
void Session::close_preparation(){if(preparation_menu_==5)throw FormatError("world12: competition overview requires its owned close request");if(preparation_menu_==19)throw FormatError("world12: settings require their owned close request");if(preparation_menu_==22&&stadium_.opened)throw FormatError("world12: stadium requires its owned close request");if(preparation_menu_==27)throw FormatError("world12: contracts require their owned close request");if(preparation_menu_&&*preparation_menu_==63)throw FormatError("world12: mailbox requires its owned close request");if(!preparation_menu_)throw FormatError("world12: no preparation menu to close");preparation_menu_.reset();environment_owner_.reset();environment_request_=0;}
ui::PreparationState Session::preparation_view(const std::filesystem::path& data)const {
    auto view=ui::world12_preparation(data,state_,resources_.reference,resources_.german,options_,owner_.revision);
    if(view.bank){view.bank->last_tip=bank_tip_;view.bank->property_notice=bank_property_notice_;view.bank->credit_notice=bank_credit_notice_;}
    if(preparation_menu_==22)view.stadium=stadium_view(data);
    if(preparation_menu_==13)view.toto=toto_view(data);
    return view;
}
}
