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
    const auto texts=load_textd(asset_path(data,"TEXTD.BLK"));local.texts=&texts;
    std::vector<MatchGroup> groups;DeskMatchCallargs call;
    call.competition=state.prepared_event?34:33;
    if(upcoming) {
        call.opponent=int(projection.local_club(opponent));
        if(upcoming->league)call.competition=rules.u8(layout::LeagueCalendarBits+state.world.catalogue.league(*upcoming->league).local_index);
        else call.competition=*upcoming->cup==DomesticCup::league_cup?16:
            *upcoming->cup==DomesticCup::german_supercup||*upcoming->cup==DomesticCup::english_supercup?13:5;
    }
    auto desk=desk_from_game(local,rules,revision,state.prepared_event?&groups:nullptr,call.competition,
        state.prepared_event?&call:nullptr,&options);
    if(upcoming) {
        const auto& crest=state.world.crests.at(state.world.clubs.at(opponent).crest);
        desk.next_opponent_badge=crest.cell;desk.next_opponent_badge_atlas=crest.path;desk.next_opponent_badge_fallback=false;
        desk.next_match_away=away;
        if(upcoming->league)desk.next_competition_label=state.world.catalogue.league(*upcoming->league).name+"\n"+std::to_string(upcoming->round)+". Spieltag";
        else if(*upcoming->cup==DomesticCup::fa_cup)desk.next_competition_label="FA Cup";
        else if(*upcoming->cup==DomesticCup::league_cup)desk.next_competition_label="League Cup";
        else if(*upcoming->cup==DomesticCup::english_supercup)desk.next_competition_label="Charity Shield";
    }
    for(size_t i=0;i<desk.managers.size();++i){const auto seat=projection.global_manager(uint8_t(i));const auto& manager=*std::find_if(state.world.managers.begin(),state.world.managers.end(),[&](const auto& m){return m.seat==seat;});const auto& club=state.world.clubs.at(manager.club);
        desk.managers[i].world_club=manager.club;desk.managers[i].badge=state.world.crests.at(club.crest).cell;
    }
    const auto id=desk.managers.at(size_t(desk.manager_slot)).world_club;
    const auto& crest=state.world.crests.at(state.world.clubs.at(id).crest);desk.badge=crest.cell;desk.badge_atlas=crest.path;
    desk.rank_curves[0].league=desk.league;desk.source="world12";validate_desk_state(desk);return desk;
}
}
