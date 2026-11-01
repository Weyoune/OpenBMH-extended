// SPDX-License-Identifier: GPL-3.0-only
#include "app/world12_initial_season.hpp"
#include "game/world12/european.hpp"
#include "game/world12/match_projection.hpp"
#include "game/season.hpp"
#include "game/lineup.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>
namespace bmh::app::world12 {
using namespace bmh::world12;
InitialCompetitionOutcome prepare_initial_competitions(CareerSnapshot& state,ManagerActionOwner& owner,
    ManagerActionOwner expected,const SeasonCalendar& calendar,const CupSources& sources,const GameState& reference) {
    state.validate();calendar.validate(state.world.catalogue);
    if(!owner.scope||owner.scope!=expected.scope||owner.revision!=expected.revision||owner.revision==std::numeric_limits<uint64_t>::max()||
        state.date||state.season||state.calendar||!state.cups.empty()||state.world.initial_competitions||!state.world.settings||
        state.world.managers.size()!=state.world.settings->planned_managers||
        std::any_of(state.league_rounds.begin(),state.league_rounds.end(),[](auto value){return value!=0;}))throw std::invalid_argument("world12: invalid initial competition owner/phase");
    for(const auto& manager:state.world.managers)if(!manager.career||!manager.career->season_records||!manager.match_state||!manager.finance||!manager.tactics)throw std::invalid_argument("world12: initial competitions need complete managers");
    if(reference.memory.pointer(layout::Career_CompetitionPointersEnglish_ExecutableField5)!=layout::Career_CompetitionPointersEnglish_RuntimeField14+16)
        throw std::invalid_argument("world12: unsupported Champions League qualifier layout");
    const auto before=state;auto candidate=state;GameRandom game(state.game_seed);WatcomRandom random(state.watcom_seed);
    const auto settings=*candidate.world.settings;std::vector<ClubId> clubs;
    for(auto group:{TableGroup::germany,TableGroup::england,TableGroup::europe})for(auto league:candidate.world.catalogue.table_leagues(group))
        clubs.insert(clubs.end(),candidate.world.membership[league.value].clubs.begin(),candidate.world.membership[league.value].clubs.end());
    for(auto club:clubs) {
        const bool human=std::any_of(candidate.world.managers.begin(),candidate.world.managers.end(),[&](const auto& m){return m.club==club;});
        auto projected=human?AiMatchProjection::create_for_humans(candidate,{club},reference,{settings.mode,settings.difficulty,settings.foreign_rule,true}):AiMatchProjection::create(candidate.world,{club},reference);
        bmh::club_balance(projected.state,0,random);bmh::auto_lineup(projected.state,0,human?0xe4:100);projected.commit_match_state(candidate.world);
    }
    auto fields=draw_initial_shared_fields(candidate.world,game,random);
    for(auto& manager:candidate.world.managers) {
        auto& history=*manager.career->season_records;history[1]=history[2]=0;
        const uint8_t rounds[]={1,3,2};
        for(size_t k=0;k<3;++k)if(std::find(fields.qualified[k].begin(),fields.qualified[k].end(),manager.club)!=fields.qualified[k].end())history[2]=uint8_t(rounds[k]+k*32);
    }
    candidate.world.initial_competitions=std::move(fields);candidate.game_seed=game.seed();candidate.watcom_seed=random.seed();
    CountryCupRanking germany,england;
    for(auto country:{Country::germany,Country::england})if(candidate.world.catalogue.selectable(country))for(uint8_t local=0;local<5;++local)
        (country==Country::germany?germany:england)[local]=candidate.world.membership[candidate.world.catalogue.starting_league(country,local).value].clubs;
    if(candidate.world.catalogue.scope.germany)candidate.cups=initial_domestic_cup_fields(candidate.world,Country::germany,germany,sources.german,game);
    if(candidate.world.catalogue.scope.england){auto fields=initial_domestic_cup_fields(candidate.world,Country::england,england,sources.english,game);candidate.cups.insert(candidate.cups.end(),fields.begin(),fields.end());}
    candidate.game_seed=game.seed();std::vector<CupDrawNotice> notices;
    for(const auto& cup:candidate.cups)if(auto notice=cup_draw_notice(candidate.world,cup))notices.push_back(std::move(*notice));
    for(auto& manager:candidate.world.managers)for(const auto& cup:candidate.cups)if(cup.cup==DomesticCup::german_cup||cup.cup==DomesticCup::fa_cup){
        bool present=std::find(cup.seeded.begin(),cup.seeded.end(),manager.club)!=cup.seeded.end();for(const auto& f:cup.rounds.front().fixtures)present|=f.home==manager.club||f.away==manager.club;
        if(present)(*manager.career->season_records)[1]=1;
    }
    candidate.calendar=CalendarCursor{calendar,0,0};candidate.validate();
    if(state!=before||owner.scope!=expected.scope||owner.revision!=expected.revision)throw std::invalid_argument("world12: initial competition owner changed during draw");
    InitialCompetitionOutcome outcome{notices,game.calls(),random.calls()};
    state=std::move(candidate);++owner.revision;return outcome;
}
}
