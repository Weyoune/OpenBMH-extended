// SPDX-License-Identifier: GPL-3.0-only
#include "ui/world12_readonly.hpp"
#include "ui/world12_statistics.hpp"
#include "ui/world12_view_actor.hpp"
#include "game/world12/desktop_projection.hpp"
#include "game/world12/highscores.hpp"
#include "ui/manager.hpp"
#include <algorithm>
namespace bmh::ui {
using namespace bmh::world12;
World12ReadonlyState world12_readonly_state(const CareerSnapshot& state,
    const World12ReadonlyIdentity& id,const World12ReadonlySelection& selection,
    const GameState& reference,const LeImage& source,const Image& underlay,const HighscoreTable& highscore_rows){
    state.validate();
    const auto actor=std::find_if(state.world.managers.begin(),state.world.managers.end(),[&](const auto& manager){return manager.seat==id.manager;});
    if(!id.scope||id.revision==UINT64_MAX||actor==state.world.managers.end()||!world12_view_actor(state,id.manager,id.club)||id.date!=state.date||
        !state.desktop||state.desktop->active_seat!=id.manager||(id.menu!=4&&id.menu!=8&&id.menu!=9&&id.menu!=23&&id.menu!=20&&id.menu!=25&&id.menu!=26&&id.menu!=28))
        throw FormatError("world12: readonly league page has no current canonical club actor");
    if(id.menu==23){World12ReadonlyState out;out.identity=id;out.selection=selection;World12StatisticsSelection child;child.page=selection.statistics_page;child.prediction_mode=selection.prediction_mode;child.category=selection.title_category;child.nation=selection.alltime_nation;child.relative=selection.alltime_relative;child.first_season=selection.title_first_season;out.statistics=std::make_shared<World12StatisticsState>(world12_statistics_state(state,id,child,reference,source,underlay));return out;}
    if(id.menu==26){const auto actual=project_owned_highscores(state,reference,highscore_rows);World12ReadonlyState out;out.identity=id;out.selection=selection;DeskRecordsState page;page.kind=DeskRecordKind::highscore;page.highscore={actual.signature,actual.ranked,actual.rows};page.underlay=underlay;page.manager_name=utf8_to_original(actor->name);page.gradient_text=reference.memory.u8(layout::TextGradientDisabled)==0&&source.u8(layout::TextGradientMode)==2;page.original_panel=reference.memory.u8(layout::SolidPanelBackground)==0;page.decorated_background=reference.memory.u8(layout::PlainBackground)==0;if(state.world.economy&&state.world.economy->presentation_options){const auto& options=*state.world.economy->presentation_options;page.gradient_text=options[layout::TextGradientDisabled-layout::LeaguePresentationOptions]==0&&source.u8(layout::TextGradientMode)==2;page.original_panel=options[layout::SolidPanelBackground-layout::LeaguePresentationOptions]==0;page.decorated_background=options[layout::PlainBackground-layout::LeaguePresentationOptions]==0;}out.records=std::move(page);return out;}
    if(id.menu==4){World12ReadonlyState out;out.identity=id;out.selection=selection;out.achievements=world12_achievements(state,id,reference,source,underlay);return out;}
    if(id.menu==9){if(!reference.texts)throw FormatError("world12: indoor standings lack actual Source texts");World12ReadonlyState out;out.identity=id;out.selection=selection;out.indoor=world12_indoor_standings(state,id,reference,source,*reference.texts);return out;}
    if(id.menu==8){if(!reference.texts)throw FormatError("world12: readonly calendar lacks actual Source text context");World12ReadonlyState out;out.identity=id;out.selection=selection;out.calendar=world12_calendar_state(state,source,*reference.texts);out.decorated_background=reference.memory.u8(layout::PlainBackground)==0;if(state.world.economy&&state.world.economy->presentation_options)out.decorated_background=(*state.world.economy->presentation_options)[layout::PlainBackground-layout::LeaguePresentationOptions]==0;return out;}
    if(id.menu==28&&state.world.clubs.at(id.club).kind!=ClubKind::club)throw FormatError("world12: current balance requires an actual club financial owner; national accounts are unavailable");
    if(state.world.clubs.at(id.club).kind!=ClubKind::club||!state.world.clubs.at(id.club).league.valid())throw FormatError("world12: scorer/fairplay page requires an actual club league; national campaign facts have their own overview");
    const auto league=state.world.clubs.at(id.club).league;const auto& definition=state.world.catalogue.league(league);
    if(!definition.active||!definition.playable)throw FormatError("world12: readonly league requires its actual playable country owner");
    const auto& members=state.world.membership.at(league.value).clubs;
    auto p=project_manager_desktop(state,reference,members);auto& game=p.state;
    int local_manager=-1;for(size_t n=0;n<p.human_managers();++n)if(p.global_manager(uint8_t(n))==id.manager)local_manager=int(n);
    if(local_manager<0)throw FormatError("world12: readonly page lost its canonical manager mapping");
    std::vector<int> teams;for(auto club:members)teams.push_back(int(p.local_club(club)));
    World12ReadonlyState out;out.identity=id;out.selection=selection;
    for(const auto& manager:state.world.managers)out.clubs.push_back(manager.club);
    for(auto club:members)if(std::find(out.clubs.begin(),out.clubs.end(),club)==out.clubs.end())out.clubs.push_back(club);
    for(size_t n=0;n<out.clubs.size();++n)if(p.local_club(out.clubs[n])!=n)throw FormatError("world12: readonly local club identity changed");
    for(size_t n=0;n<game.players.size();++n)out.players.push_back(p.global_player(uint32_t(n)));
    if(id.menu==28){
        if(!actor->finance||!actor->career||!actor->career->season_services||!actor->match_state||!actor->match_state->attendance||!state.world.economy||!state.world.economy->managers[id.manager].monthly||!state.world.economy->managers[id.manager].depreciation)
            throw FormatError("world12: current balance requires known actual finance, monthly bookings, depreciation and attendance");
        // Beta1 exposed no editable plan. Its first planning entry starts a
        // fresh draft; already recorded income/attendance remains authoritative.
        if(!state.world.economy->managers[id.manager].balance_plan){
            const auto base=size_t(local_manager)*0xb0c;
            for(size_t n=0;n<18*4;++n)game.manager_records.at(base+0x2b0+n)=0;
            for(auto offset:{0x2f8,0x2fc,0x308,0x310})for(size_t n=0;n<4;++n)game.manager_records.at(base+size_t(offset)+n)=0;
            game.memory.add_block(layout::PlanTicketPrice+uint32_t(local_manager),1);game.memory.set8(layout::PlanTicketPrice+uint32_t(local_manager),0);
        }
        auto draft=selection.balance_draft?*selection.balance_draft:open_balance_plan(game,local_manager);
        if(draft.manager!=local_manager||uint8_t(draft.view)>2)throw FormatError("world12: balance draft changed its actual manager");
        out.selection.balance_draft=draft;
        BalanceIdentity child{local_manager,int(p.local_club(id.club)),int(game.club_ids.at(p.local_club(id.club))),state.date,id.revision,id.scope,id.sequence};
        auto page=balance_from_game(game,draft,source,child,underlay);
        page.pending_close=selection.balance_pending_close;page.club_name=utf8_to_original(state.world.clubs.at(id.club).name);
        const auto& crest=state.world.crests.at(state.world.clubs.at(id.club).crest);
        for(auto* notice:{&page.read_only_notice,&page.close_notice}){notice->badge_atlas=crest.path;notice->badge_sprite=crest.cell;notice->badge_fallback=false;}
        page.decorated=game.memory.u8(layout::PlainBackground)==0;page.original_panel=game.memory.u8(layout::SolidPanelBackground)==0;page.gradient=game.memory.u8(layout::TextGradientDisabled)==0&&source.u8(layout::TextGradientMode)==2;
        page.cached_backdrop=game.memory.has(layout::CareerSession_ApplyBalanceSourceStyle_ExecutableField)?game.memory.u32(layout::CareerSession_ApplyBalanceSourceStyle_ExecutableField)!=0:source.u32(layout::CareerSession_ApplyBalanceSourceStyle_ExecutableField)!=0;
        out.balance=std::move(page);
    }else if(id.menu==20){
        TopScorersState page;page.explicit_teams=true;page.metric=selection.metric;page.scope=selection.scope;page.underlay=underlay;
        page.projection=bmh::game::top_scorers_from_teams(game,local_manager,int(league.value),teams,top_scorers_metric_to_game(page.metric),top_scorers_scope_to_game(page.scope));
        page.projection.manager_slot=id.manager;
        page.identity={int(id.manager),int(p.local_club(id.club)),int(game.club_ids.at(p.local_club(id.club))),int(league.value),state.date,id.revision,id.scope,id.sequence,0,0};
        for(auto& row:page.projection.rows){row.player_name=utf8_to_original(state.world.players.at(p.global_player(uint32_t(row.player))).attributes.name);row.team_name=utf8_to_original(state.world.clubs.at(p.global_club(uint32_t(row.team))).name);}
        page.decorated_background=game.memory.u8(layout::PlainBackground)==0;page.gradient_text=game.memory.u8(layout::TextGradientDisabled)==0&&source.u8(layout::TextGradientMode)==2;
        out.top_scorers=std::move(page);
    }else {
        auto page=fair_play_state_from_teams(game,int(league.value),teams,underlay,local_manager);
        for(auto& row:page.fair_play_rows)row.name=utf8_to_original(state.world.clubs.at(p.global_club(uint32_t(row.team))).name);
        page.manager_name=utf8_to_original(state.world.clubs.at(id.club).name);page.gradient_text=game.memory.u8(layout::TextGradientDisabled)==0&&source.u8(layout::TextGradientMode)==2;page.decorated_background=game.memory.u8(layout::PlainBackground)==0;
        out.records=std::move(page);
    }
    return out;
}
}
