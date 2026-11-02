// SPDX-License-Identifier: GPL-3.0-only
#include "app/world12_session.hpp"
#include "app/world12_visible_menu.hpp"
#include "game/world12/desktop_projection.hpp"
#include "ui/league_table.hpp"
#include <algorithm>
namespace bmh::app::world12 {
using namespace bmh::world12;
namespace {
Manager& table_actor(CareerSnapshot& state,uint8_t seat){auto m=std::find_if(state.world.managers.begin(),state.world.managers.end(),[&](const auto& m){return m.seat==seat;});if(m==state.world.managers.end()||!m->career)throw FormatError("world12: missing table manager career");return *m;}
uint8_t points_mode(const RuntimeResources& resources,const League& league){
    const auto& rules=league.country==Country::england?resources.english:resources.german;
    const uint32_t row=league.playable?league.local_index:league.country==Country::spain?5:league.country==Country::england?6:league.country==Country::france?7:8;
    return rules.u8(layout::CareerSession_BeginFriendlyOpponentTable_RuntimeField+row)&1;
}
void change_table_league(ui::World12TableSelection& selection,LeagueId league,const CareerSnapshot& state,const RuntimeResources& resources){
    if(selection.league==league)return;
    selection.league=league;selection.first_round=0;selection.end_round=state.league_rounds.at(league.value);
    ui::normalize_world12_table_range(selection,state);selection.normal_mode=points_mode(resources,state.world.catalogue.league(league));selection.word&=0xf7f;
}
}
ui::World12TableState Session::table_page(const CareerSnapshot& state,const ui::World12TableIdentity& identity,const ui::World12TableSelection& selection)const {
    if(!resources_.texts)throw FormatError("world12: table texts unavailable");
    auto out=ui::world12_table_state(state,identity,selection,resources_.german,*resources_.texts);
    const auto projected=project_manager_desktop(state,resources_.reference);const auto& local=projected.state;
    out.table.decorated_background=local.memory.u8(layout::PlainBackground)==0;out.table.original_table_panel=local.memory.u8(layout::SolidPanelBackground)==0;
    out.table.source_show_previous_rank=local.memory.has(layout::LeagueTable_OriginalToUtf8_RuntimeField)&&local.memory.u8(layout::LeagueTable_OriginalToUtf8_RuntimeField)!=0&&out.table.rounds_played>1;
    const auto label=[&](uint32_t at){return ui::original_to_utf8((*resources_.texts)[(at-layout::TextPointerTable)/4]);};
    if(out.table.rounds_played>=state.world.catalogue.league(selection.league).rounds())out.subtitle=label(layout::CareerSession_FormatError_TextPointer3);
    else {
        auto suffix=label(layout::CareerSession_FormatError_TextPointer4);
        while(!suffix.empty()&&suffix.front()==' ')suffix.erase(suffix.begin());
        auto prefix=label(layout::CareerSession_FormatError_TextPointer2);
        while(!prefix.empty()&&prefix.back()==' ')prefix.pop_back();
        out.subtitle=prefix+" "+std::to_string(out.table.rounds_played+1)+". "+suffix;
    }

    return out;
}
ui::World12TableState Session::open_tables(const ui::DeskRequest& request){
    validate_file_owner(request.manager_slot,request.revision);
    if(request.kind!=ui::DeskRequestKind::open_menu||request.menu_kind!=0||request.value!=-1||table_generation_>=UINT32_MAX||owner_.revision==UINT64_MAX)
        throw FormatError("world12: invalid table desktop owner");
    const auto p=project_manager_desktop(state_,resources_.reference);const auto desk=ui::desk_from_game(p.state,resources_.german,owner_.revision);
    if(!visible_world12_menu(desk,request.item_id,0))throw FormatError("world12: table icon is not visible/enabled");
    auto candidate=state_;const auto seat=*candidate.desktop->active_seat;auto& manager=table_actor(candidate,seat);
    // OpenBMH deviates from the original: pre-schema33 development saves did
    // not represent this display word. First entry initializes that UI-only
    // unknown to InitializeCareer's 0x11; no gameplay history is reconstructed.
    const uint16_t word=manager.career->table_display_word.value_or(0x11)&0xff7f;
    manager.career->table_display_word=word;
    const auto selection=ui::initial_world12_table_selection(candidate,seat,word,points_mode(resources_,candidate.world.catalogue.league(candidate.world.clubs.at(manager.club).league)));
    const ui::World12TableIdentity identity{seat,manager.club,owner_.revision+1,owner_.scope,(table_generation_+1)<<32};
    auto view=table_page(candidate,identity,selection);candidate.validate();state_=std::move(candidate);++owner_.revision;++table_generation_;table_owner_=identity;table_selection_=selection;preparation_menu_=0;return view;
}
ui::World12TableState Session::tables_view()const {
    if(preparation_menu_!=0||!table_owner_||table_owner_->revision!=owner_.revision||table_owner_->scope!=owner_.scope||
       (table_owner_->sequence>>32)!=table_generation_||!state_.desktop||state_.desktop->active_seat!=table_owner_->manager)
        throw FormatError("world12: stale table window");
    return table_page(state_,*table_owner_,table_selection_);
}
bool Session::apply_tables(const ui::World12TableRequest& request,const std::filesystem::path& data,const ClubInformationPresenter& club_presenter,const TableRangePresenter& range_presenter){
    const auto current=tables_view();const auto identity=*table_owner_;
    if(request.identity!=identity||owner_.revision==UINT64_MAX||identity.sequence==UINT64_MAX||((identity.sequence+1)>>32)!=table_generation_||
       (request.action!=ui::World12TableAction::select_league&&request.league.valid())||
       (request.action!=ui::World12TableAction::team_info&&request.team.valid())||
       (request.action!=ui::World12TableAction::control&&request.value!=-1))throw FormatError("world12: stale or malformed table action");
    const auto before=state_;const auto generation=table_generation_;const auto original_selection=table_selection_;
    auto candidate=state_;auto selection=table_selection_;
    switch(request.action){
    case ui::World12TableAction::close:selection.word=uint16_t((selection.word&0xf7f)|(selection.normal_mode?0x80:0));break;
    case ui::World12TableAction::select_league:
        if(std::find(current.leagues.begin(),current.leagues.end(),request.league)==current.leagues.end())throw FormatError("world12: selected table league is hidden");
        change_table_league(selection,request.league,candidate,resources_);break;
    case ui::World12TableAction::next_group:{
        auto group=selection.group;do {group=TableGroup((uint8_t(group)+1)%3);}while(!current.groups[size_t(group)]);
        const auto ids=candidate.world.catalogue.table_leagues(group);const auto own=candidate.world.clubs.at(identity.club).league;
        selection.group=group;change_table_league(selection,std::find(ids.begin(),ids.end(),own)!=ids.end()?own:ids.front(),candidate,resources_);break;
    }
    case ui::World12TableAction::team_info:
        if(!request.team.valid()||std::find(current.clubs.begin(),current.clubs.end(),request.team)==current.clubs.end())throw FormatError("world12: table club is not visible");
        inspect_club_information(candidate,resources_.reference,resources_.german,*resources_.texts,request.team,club_presenter,ui::LeagueTableScreen(data,current,resources_.german).render());break;
    case ui::World12TableAction::control:
        if(request.value!=9){ui::apply_world12_table_control(selection,request.value,candidate.world.catalogue);break;}
        if(!range_presenter)throw FormatError("world12: table range requires its visible presenter");
        {
            ui::TableRangeDialogProfile profile;profile.minimum=1;profile.maximum=candidate.world.catalogue.league(selection.league).rounds();profile.default_value=1;
            const auto p=project_manager_desktop(candidate,resources_.reference);
            profile.text_mode=p.state.memory.u8(layout::TextGradientDisabled)==0&&resources_.german.u8(layout::Advertising_Button_RuntimeField+1)==3?ui::TableRangeTextMode::source_gradient:ui::TableRangeTextMode::flat;
            const auto backdrop=ui::LeagueTableScreen(data,current,resources_.german).render();
            const auto first=range_presenter(profile,backdrop);if(!first)throw FormatError("world12: table start-range presenter stopped");
            const auto validate=[&](const ui::TableRangeDialogOutcome& result,const ui::TableRangeDialogProfile& limits){if(uint8_t(result.result)>1||result.x<0||result.x>207||result.y<0||result.y>104||
                (result.result==ui::TableRangeDialogResult::accept&&(result.value<limits.minimum||result.value>limits.maximum)))throw FormatError("world12: invalid table range outcome");};
            validate(*first,profile);
            if(first->result==ui::TableRangeDialogResult::accept){
                profile.title_textd_address=layout::CareerSession_CurrentLeagueTableDispatch_ExecutableField2;profile.minimum=first->value;
                profile.default_value=std::max<uint32_t>(1,std::min<uint32_t>(candidate.league_rounds.at(selection.league.value),profile.maximum));profile.x=first->x;profile.y=first->y;
                const auto last=range_presenter(profile,backdrop);if(!last)throw FormatError("world12: table end-range presenter stopped");validate(*last,profile);
                if(last->result==ui::TableRangeDialogResult::accept){selection.first_round=int(first->value)-1;selection.end_round=int(last->value);}
                selection.word=uint16_t((selection.word&0xcff)|0x200);ui::normalize_world12_table_range(selection,candidate);
            }
            selection.word&=0xf7f;
        }
        break;
    default:throw FormatError("world12: unknown table action");
    }
    if(state_!=before||owner_.revision!=identity.revision||table_owner_!=identity||table_generation_!=generation||table_selection_!=original_selection||preparation_menu_!=0)
        throw FormatError("world12: table presenter changed its live owner");
    auto next=identity;++next.revision;++next.sequence;
    table_actor(candidate,identity.manager).career->table_display_word=selection.word;
    table_page(candidate,next,selection);candidate.validate();state_=std::move(candidate);++owner_.revision;
    if(request.action==ui::World12TableAction::close){table_owner_.reset();preparation_menu_.reset();return true;}
    table_owner_=next;table_selection_=selection;return false;
}
}
