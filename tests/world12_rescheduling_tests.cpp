// SPDX-License-Identifier: GPL-3.0-only
#include "world12_fixture.hpp"
#include "fixture_tables.hpp"
#include "game/world12/rescheduling.hpp"
#include "game/world12/progression.hpp"
#include "game/world12/standings.hpp"
#include "game/schedule.hpp"
#include <iostream>
#include <algorithm>
using namespace bmh;using namespace bmh::world12;
namespace {
void check(bool b,const char* s){if(!b)throw std::runtime_error(s);}
template<class F>void rejects(F f){bool caught=false;try{f();}catch(const std::exception&){caught=true;}check(caught,"invalid fixture move accepted");}
CareerSnapshot fixture(uint32_t league){CareerSnapshot s;s.world=bmh::test::world12::complete_world();s.results=SeasonResults::empty(s.world.catalogue);s.world.settings.emplace();s.world.markets.emplace();s.world.free_players.emplace();s.world.players_initialized=true;
    const auto club=s.world.membership[league].clubs[0];s.world.managers.push_back({0,"Host",club,{league},0});s.world.managers.back().career.emplace();s.world.managers.back().career->stadium.facility_values[14]=1;
    s.calendar.emplace();s.calendar->schedule.days=366;for(const auto& l:s.world.catalogue.leagues)for(uint8_t round=1;round<=l.rounds();++round)s.calendar->schedule.leagues.push_back({uint16_t(7+round*7),l.id,round});s.validate();return s;
}
void advance(CareerSnapshot& s,uint16_t day){while(s.calendar->next_day<=day){const auto ticket=*prepare_day(s);std::vector<CompletedLeagueRound> scores;for(const auto& p:ticket.leagues){CompletedLeagueRound r{p.appointment.league,p.appointment.round,{},p.match_indices,p.ordinary};for(size_t i=0;i<p.matches.size();++i)r.results.push_back({true,1,0});scores.push_back(r);}commit_league_day(s,ticket,scores);s.validate();}}
}
int main()try{set_fixture_tables(bmh::test::authored_fixture_tables());
    for(uint32_t league:{0u,5u}){
        auto s=fixture(league);const auto before=s;const auto options=reschedulable_fixtures(s,0);check(!options.empty(),"no real home match to move");
        const auto move=options[0];const auto dates=reschedule_dates(s,0,move);check(!dates.empty(),"no eligible date");const auto friday=uint16_t(move.source_day-1);check(std::find(dates.begin(),dates.end(),friday)!=dates.end(),"floodlit previous-day option missing");
        move_fixture(s,0,move,friday);check(s.watcom_seed==before.watcom_seed&&s.game_seed==before.game_seed&&s.results==before.results&&s.world==before.world&&s.calendar->schedule==before.calendar->schedule,"selection moved other fixtures/world or consumed RNG");
        const auto saved=make_save(s);check(saved.stream[7]=='N'&&read_save(saved)==s,"individual fixture move did not save/restore");const auto accepted=s;rejects([&]{move_fixture(s,0,move,dates[0]);});check(s==accepted,"repeated move changed state");
        undo_fixture_move(s,0,s.reschedules[0]);auto expected=before;expected.calendar->revision+=2;check(s==expected,"undo failed to restore original fixture exactly");
        move_fixture(s,0,move,friday);advance(s,friday);check(s.reschedules[0].played&&s.results.result(s.world.catalogue,move.league,move.round,move.match).played&&s.league_rounds[league]==move.round-1,"Friday replay advanced the ordinary round cursor");check(visible_league_round(s,{league})==move.round,"early result absent from visible table range");
        const auto ordinary=*prepare_day(s);const auto p=std::find_if(ordinary.leagues.begin(),ordinary.leagues.end(),[&](const auto& p){return p.appointment.league==move.league;});check(p!=ordinary.leagues.end()&&p->matches.size()==size_t(s.world.catalogue.league(move.league).clubs/2-1)&&p->ordinary,"ordinary day repeated the Friday fixture");advance(s,move.source_day);check(read_save(make_save(s))==s,"played replay receipt failed roundtrip");
        auto late=fixture(league);const auto late_dates=reschedule_dates(late,0,move);const auto target=std::find_if(late_dates.begin(),late_dates.end(),[&](auto day){return day>move.source_day;});check(target!=late_dates.end(),"no later replay date");move_fixture(late,0,move,*target);advance(late,move.source_day);check(!late.results.result(late.world.catalogue,move.league,move.round,move.match).played&&late.league_rounds[league]==move.round,"deferred hole was scored or blocked its ordinary round");check(read_save(make_save(late))==late,"pending hole did not roundtrip");advance(late,*target);check(late.reschedules[0].played,"later replay was not committed");
    }
    auto s=fixture(0);const auto move=reschedulable_fixtures(s,0)[0];const auto before=s;rejects([&]{move_fixture(s,0,move,150);});rejects([&]{move_fixture(s,1,move,20);});check(s==before,"invalid date/actor changed accepted state");auto bad=s;bad.reschedules.push_back(move);rejects([&]{bad.validate();});
    auto full=fixture(0);const auto own=reschedulable_fixtures(full,0)[0];const auto games=league_fixtures(full.world,own.league,own.round);uint8_t slot=0;for(size_t i=0;i<games.size();++i)if(i!=own.match)full.reschedules.push_back({own.league,own.round,uint8_t(i),own.source_day,uint16_t(own.source_day-1),slot++,false});full.validate();check(slot==8,"day-cap fixture incomplete");const auto dates=reschedule_dates(full,0,own);check(std::find(dates.begin(),dates.end(),own.source_day-1)==dates.end(),"ninth replay admitted on a full day");
    const auto full_before=full;rejects([&]{move_fixture(full,0,own,uint16_t(own.source_day-1));});check(full==full_before,"full replay date changed state");auto duplicate=full;duplicate.reschedules[1].slot=duplicate.reschedules[0].slot;rejects([&]{duplicate.validate();});auto incompatible=make_save(full);incompatible.stream[7]='M';rejects([&]{read_save(incompatible);});
    std::cout<<"DE18/EN20 individual moves, Friday and later replay tickets, unchanged neighbours/RNG, ordinary partial rounds, undo/identity/season restrictions, early table range and N/old save roundtrips passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
