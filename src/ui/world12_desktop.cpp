// SPDX-License-Identifier: GPL-3.0-only
#include "ui/world12_desktop.hpp"
#include "game/world12/desktop_projection.hpp"
#include "game/match_engine.hpp"
#include "formats/textd.hpp"
#include <algorithm>
namespace bmh::ui {
DeskState world12_initial_desktop(const std::filesystem::path& data,const bmh::world12::CareerSnapshot& state,
    const GameState& reference,const LeImage& rules,SetupOptions options,uint64_t revision) {
    using namespace bmh::world12;state.validate();
    if(!state.desktop||!state.desktop->initial_event||!state.desktop->active_seat||!state.world.settings||!options.country_selection)
        throw FormatError("world12: initial desktop event is unavailable");
    const auto seat=*state.desktop->active_seat;
    const auto& manager=*std::find_if(state.world.managers.begin(),state.world.managers.end(),[&](const auto& m){return m.seat==seat;});
    ClubId opponent;const PreparedGroup* upcoming=nullptr;bool away=false;
    if(state.prepared_event)for(const auto& group:state.prepared_event->groups)for(const auto& match:group.matches)
        if(match.fixture.home==manager.club||match.fixture.away==manager.club) {
            away=match.fixture.away==manager.club;opponent=away?match.fixture.home:match.fixture.away;upcoming=&group;
        }
    auto projection=project_manager_desktop(state,reference,opponent.valid()?std::vector<ClubId>{opponent}:std::vector<ClubId>{});auto& local=projection.state;
    if(state.prepared_event)local.manager_records.at(size_t(local.memory.u8(layout::ActiveManager))*0xb0c+0x28a)=away?1:0;
    const auto texts=load_textd(asset_path(data,"TEXTD.BLK"));local.texts=&texts;
    std::vector<MatchGroup> groups;DeskMatchCallargs call;
    call.competition=state.prepared_event?34:33;
    if(upcoming) {
        call.opponent=int(projection.local_club(opponent));
        if(upcoming->league)call.competition=rules.u8(layout::LeagueCalendarBits+state.world.catalogue.league(*upcoming->league).local_index);
        else if(upcoming->cup)call.competition=*upcoming->cup==DomesticCup::league_cup?16:
            *upcoming->cup==DomesticCup::german_supercup||*upcoming->cup==DomesticCup::english_supercup?13:5;
        else if(upcoming->european)call.competition=5+int(*upcoming->european);
        else if(upcoming->indoor)call.competition=17;
        else if(upcoming->source_entry==35)call.competition=18;
        else throw FormatError("world12: upcoming desk competition has no Source identity");
    }
    auto desk=desk_from_game(local,rules,revision,state.prepared_event?&groups:nullptr,call.competition,
        state.prepared_event?&call:nullptr,&options);
    if(upcoming) {
        const auto& crest=state.world.crests.at(state.world.clubs.at(opponent).crest);
        desk.next_opponent_badge=crest.cell;desk.next_opponent_badge_atlas=crest.path;desk.next_opponent_badge_fallback=false;
        desk.next_match_away=away;
        if(upcoming->league)desk.next_competition_label=state.world.catalogue.league(*upcoming->league).name+"\n"+std::to_string(upcoming->round)+". Spieltag";
        else if(upcoming->cup==DomesticCup::fa_cup)desk.next_competition_label="FA Cup";
        else if(upcoming->cup==DomesticCup::league_cup)desk.next_competition_label="League Cup";
        else if(upcoming->cup==DomesticCup::english_supercup)desk.next_competition_label="Charity Shield";
        else if(upcoming->european)desk.next_competition_label=texts[(layout::CupDraw_FormatError_ExecutableField6-layout::TextPointerTable)/4+size_t(*upcoming->european)];
        else if(upcoming->indoor)desk.next_competition_label="Hallenturnier";
        else if(upcoming->source_entry==35)desk.next_competition_label="Aufstiegsspiele";
    }
    for(size_t i=0;i<desk.managers.size();++i){const auto seat=projection.global_manager(uint8_t(i));const auto& manager=*std::find_if(state.world.managers.begin(),state.world.managers.end(),[&](const auto& m){return m.seat==seat;});const auto& club=state.world.clubs.at(manager.club);
        desk.managers[i].world_club=manager.club;desk.managers[i].badge=state.world.crests.at(club.crest).cell;
    }
    desk.canonical_manager_slot=int(seat);
    for(size_t row=0;row<desk.managers.size();++row)desk.managers[row].canonical_slot=int(projection.global_manager(uint8_t(row)));
    for(const auto& member:state.world.managers)desk.world12_career_roster.push_back({member.seat,member.portrait,member.club,member.name});
    std::sort(desk.world12_career_roster.begin(),desk.world12_career_roster.end(),[](const auto& a,const auto& b){return a.seat<b.seat;});
    if(state.prepared_event){desk.world12_event_known=true;for(const auto& group:state.prepared_event->groups){
        const uint64_t kind=group.league?0x100u+group.league->value:group.cup?0x200u+uint8_t(*group.cup):group.european?0x300u+uint8_t(*group.european):group.indoor?0x400u:0x500u;
        desk.world12_event_groups.push_back({kind,group.source_entry,group.revision,uint64_t(group.round)*256+group.leg,group.flags});
    }}
    const auto id=desk.managers.at(size_t(desk.manager_slot)).world_club;
    const auto& crest=state.world.crests.at(state.world.clubs.at(id).crest);desk.badge=crest.cell;desk.badge_atlas=crest.path;
    desk.rank_curves[0].league=desk.league;desk.source="world12";validate_desk_state(desk);return desk;
}
}
