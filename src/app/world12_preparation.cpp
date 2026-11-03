// SPDX-License-Identifier: GPL-3.0-only
#include "app/world12_session.hpp"
#include "app/preparation_actions.hpp"
#include "app/world12_pitch_source.hpp"
#include "app/training_camp_request_source.hpp"
#include "game/world12/economy.hpp"
#include "game/world12/desktop_projection.hpp"
#include "game/desk_actions.hpp"
#include "game/lineup.hpp"
#include "game/national_identity.hpp"
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
    if(row<0||row>=24||local.squad_slots.at(squad_range(local,club).first+size_t(row))!=slot||((slot&0xfff)==0xfff?-1:int(slot&0xfff))!=player)
        throw FormatError("world12: stale preparation player/row");
}
void prepare_lineup(GameState& local,int manager,int club) {
    auto_lineup(local,club,0xe4);prepare_lineup_entry(local,club);open_lineup_screen(local,manager);
    if(!initialize_pitch_set_piece_source(local,manager,club))throw FormatError("world12: set-piece initialization failed");
    const auto display=layout::CareerSession_AnyOf_ExecutableField+uint32_t(manager*2);
    if((local.memory.u16(display)&0x80)&&!normalize_pitch_set_piece_source(local,manager,club))throw FormatError("world12: set-piece reentry failed");
    std::array<bool,4096> seen{};const auto first=squad_range(local,club).first;
    for(size_t row=0;row<11;++row){const auto slot=local.squad_slots.at(first+row);const auto id=slot&0xfff;const auto at=size_t(manager)*0x840+row*0x58;
        if(id==0xfff&&slot==0xfff)continue;
        if(id==0xfff||size_t(id)>=local.players.size()||seen[id]||!(slot&0x8000)||(slot&0x4000)||int8_t(local.tactics.at(at+0x41))<0||int8_t(local.tactics.at(at+0x41))>19||int8_t(local.tactics.at(at+0x42))<0||int8_t(local.tactics.at(at+0x42))>8)
            throw FormatError("world12: incomplete prepared eleven");seen[id]=true;}
}
void capture_display(CareerSnapshot& candidate,const AiMatchProjection& projection) {
    for(size_t i=0;i<projection.human_managers();++i){const auto seat=projection.global_manager(uint8_t(i));auto& desk=*std::find_if(candidate.desktop->managers.begin(),candidate.desktop->managers.end(),[&](const auto& d){return d.seat==seat;});
        desk.display_word=projection.state.memory.u16(layout::CareerSession_AnyOf_ExecutableField+uint32_t(i*2));}
}
}
void Session::open_preparation(const ui::DeskRequest& request) {
    if(preparation_menu_||request.kind!=ui::DeskRequestKind::open_menu||(request.menu_kind!=3&&request.menu_kind!=18&&request.menu_kind!=16&&request.menu_kind!=14&&request.menu_kind!=15)||owner_.revision==std::numeric_limits<uint64_t>::max())throw FormatError("world12: unsupported or occupied preparation menu");
    validate_file_owner(request.manager_slot,request.revision);
    const auto before=state_;auto candidate=state_;auto projection=project_manager_desktop(candidate,resources_.reference);auto& local=projection.state;
    const auto native_manager=int(local.memory.u8(layout::ActiveManager));
    const auto team=local.manager_teams.at(size_t(native_manager));
    actor(local,native_manager,team,request.revision,owner_.revision);
    if(request.menu_kind==15) {
        const auto seat=projection.global_manager(uint8_t(native_manager));
        const auto& manager=*std::find_if(candidate.world.managers.begin(),candidate.world.managers.end(),[&](const auto& m){return m.seat==seat;});
        if(!manager.career||!manager.career->camp_focus)throw FormatError("world12: old career lacks saved camp focus; start a current fresh career");
        const auto& camp_rules=candidate.world.clubs.at(manager.club).country==Country::england?resources_.english:resources_.german;configure_training_camp_data(local,camp_rules);const auto camp=ui::training_camp_from_game(local,camp_rules,native_manager,owner_.revision);
        if(camp.focus_normalization_required&&!set_training_camp_focus(local,native_manager,camp.focus))throw FormatError("world12: camp entry normalization failed");
    }
    if(request.menu_kind==3)prepare_lineup(local,native_manager,team);
    if(request.menu_kind==16){if(options_.mode<2)throw FormatError("world12: training unavailable in mode I");const auto training=ui::training_from_game(local,resources_.german,owner_.revision);
        if(training.needs_initialization()){ui::TrainingRequest r;r.kind=ui::TrainingRequestKind::initialize;r.manager_slot=native_manager;r.runtime_club=team;r.revision=owner_.revision;r.desired_types=training.types;r.desired_areas=training.areas;r.desired_automation=0x9f;
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
            else if constexpr(std::is_same_v<R,ui::PitchRequest>){if(*preparation_menu_!=3)throw FormatError("world12: pitch has no lineup owner");apply_world12_pitch_action(local,r,state_.world.settings->foreign_rule);}
            else {auto scratch=revision;std::string error;
                if constexpr(std::is_same_v<R,ui::TacticsRequest>){if(*preparation_menu_!=18||!apply_tactics_request_source(local,resources_.german,r,scratch,error))throw FormatError(error.empty()?"world12: tactic menu mismatch":error);}
                else {if(*preparation_menu_!=16||!apply_training_request_source(local,resources_.german,r,scratch,error))throw FormatError(error.empty()?"world12: training menu mismatch":error);}
            }
        } else if constexpr(std::is_same_v<R,ui::TrainingCampRequest>) {
            if(*preparation_menu_!=15)throw FormatError("world12: camp command has no current menu owner");
            actor(local,r.manager_slot,r.runtime_club,r.revision,revision);const auto seat=projection.global_manager(uint8_t(r.manager_slot));const auto& manager=*std::find_if(candidate.world.managers.begin(),candidate.world.managers.end(),[&](const auto& m){return m.seat==seat;});configure_training_camp_data(local,candidate.world.clubs.at(manager.club).country==Country::england?resources_.english:resources_.german);auto scratch=revision;std::string error;
            if(!apply_training_camp_request_source(local,random,r,scratch,error))throw FormatError(error.empty()?"world12: camp command rejected":error);
            camp_changed=true;
        } else throw FormatError("world12: unsupported preparation command");
    },request);
    projection.commit_match_state(candidate.world);capture_display(candidate,projection);
    if(camp_changed)capture_career_economy(candidate.world,projection);
    candidate.watcom_seed=random.seed();candidate.validate();
    if(state_!=before||owner_.revision!=revision)throw FormatError("world12: preparation owner changed during command");state_=std::move(candidate);++owner_.revision;
}
void Session::close_preparation(){if(friendly_)throw FormatError("world12: friendly quote requires its owned close request");if(readonly_owner_)throw FormatError("world12: readonly window requires its owned close request");if(preparation_menu_==0||preparation_menu_==5||preparation_menu_==17)throw FormatError("world12: competition overview requires its owned close request");if(preparation_menu_==19)throw FormatError("world12: settings require their owned close request");if(preparation_menu_==22&&stadium_.opened)throw FormatError("world12: stadium requires its owned close request");if(preparation_menu_==27)throw FormatError("world12: contracts require their owned close request");if(preparation_menu_&&*preparation_menu_==63)throw FormatError("world12: mailbox requires its owned close request");if(!preparation_menu_)throw FormatError("world12: no preparation menu to close");preparation_menu_.reset();environment_owner_.reset();environment_request_=0;}
ui::PreparationState Session::preparation_view(const std::filesystem::path& data)const {
    auto view=ui::world12_preparation(data,state_,resources_.reference,resources_.german,options_,owner_.revision,&resources_.english);
    if(view.bank){view.bank->last_tip=bank_tip_;view.bank->property_notice=bank_property_notice_;view.bank->credit_notice=bank_credit_notice_;}
    if(preparation_menu_==22)view.stadium=stadium_view(data);
    if(preparation_menu_==13)view.toto=toto_view(data);
    return view;
}
}
