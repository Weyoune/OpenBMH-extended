// SPDX-License-Identifier: GPL-3.0-only
#include "world12_fixture.hpp"
#include "fixture_tables.hpp"
#include "game/world12/league_history.hpp"
#include "game/world12/standings.hpp"
#include "game/matchday.hpp"
#include "game/schedule.hpp"
#include <iostream>
using namespace bmh;using namespace bmh::world12;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class F>void rejects(F action,const char* message){bool rejected=false;try{action();}catch(const std::exception&){rejected=true;}check(rejected,message);}
void put16(Bytes& bytes,size_t at,uint16_t value){bytes.at(at)=uint8_t(value);bytes.at(at+1)=uint8_t(value>>8);}
CareerSnapshot fixture(const std::vector<LeagueId>& leagues,uint8_t count=0,uint32_t season=0){
    CareerSnapshot s;s.world=bmh::test::world12::complete_world();s.world.players_initialized=true;s.world.free_players.emplace();s.world.markets.emplace();s.world.settings=CareerSettings{3,2,5,uint8_t(leagues.size()),false};s.results=SeasonResults::empty(s.world.catalogue);s.date=40;s.game_seed=17;s.watcom_seed=91;s.season=season;
    for(const auto& league:s.world.catalogue.leagues){s.league_rounds[league.id.value]=2;for(int r=1;r<=2;++r)for(int m=0;m<league.clubs/2;++m)s.results.result(s.world.catalogue,league.id,r,m)={true,uint8_t((m+r)%4),uint8_t((2*m+r)%3)};}
    uint8_t seat=0;for(auto league:leagues){const auto club=s.world.membership[league.value].clubs[3];Manager manager{seat,"History",club,league,seat};auto& career=manager.career.emplace();career.season_records.emplace();for(size_t i=0;i<150;++i)(*career.season_records)[i]=uint8_t(i%120);auto& services=career.season_services.emplace();services.strength_history_count=count;for(size_t i=0;i<38;++i){services.strength_history[i]=uint8_t(i+1);services.position_history[i]=uint8_t(i%10);}s.world.managers.push_back(std::move(manager));seat=uint8_t(leagues.size()==2?3:seat+1);}
    s.validate();return s;
}
std::vector<CompletedLeagueRound> score_round(CareerSnapshot& state,const std::vector<LeagueId>& leagues){
    std::vector<CompletedLeagueRound> result;for(auto league:leagues){CompletedLeagueRound round{league,3,{}};state.league_rounds[league.value]=3;for(int m=0;m<state.world.catalogue.league(league).clubs/2;++m){LeagueResult score{true,uint8_t((m+1)%5),uint8_t((m*3)%4)};round.results.push_back(score);state.results.result(state.world.catalogue,league,3,m)=score;}result.push_back(std::move(round));}return result;
}
GameState native_history(const CareerSnapshot& before,const CareerSnapshot& after,Country country){
    GameState g;g.season=int(std::min(before.season,49u));g.league_rounds.assign(9,0);g.results.assign(9*380,0xcc);g.position_history.assign(138*26,0);g.team_morale.resize(90);g.squad_slots.assign(90*24,kEmptySlot);
    std::vector<ClubId> clubs;for(uint8_t local=0;local<5;++local){const auto league=before.world.catalogue.starting_league(country,local);for(auto club:before.world.membership[league.value].clubs)clubs.push_back(club);g.league_rounds[local]=before.league_rounds[league.value];for(int r=1;r<=3;++r)for(int m=0;m<before.world.catalogue.league(league).clubs/2;++m){const auto& score=after.results.result(after.world.catalogue,league,r,m);if(score.played)g.results[local*380+(r-1)*10+m]=uint8_t((score.home<<4)|score.away);}}
    for(size_t team=0;team<clubs.size();++team){const auto& club=after.world.clubs.at(clubs[team]);g.team_morale[team]=club.morale;for(size_t row=0;row<24;++row){const auto& slot=club.roster[row];if(slot.empty())continue;const auto player=uint16_t(g.players.size());g.players.push_back(after.world.players.at(slot.player).attributes);g.squad_slots[team*24+row]=uint16_t(player|((slot.flags&1)?0x8000:0)|((slot.flags&2)?0x4000:0)|((slot.flags&4)?0x2000:0)|((slot.flags&8)?0x1000:0));}}
    for(const auto& manager:before.world.managers)if(before.world.clubs.at(manager.club).country==country){const auto team=std::find(clubs.begin(),clubs.end(),manager.club)-clubs.begin();const auto local=g.manager_teams.size();g.manager_teams.push_back(int(team));g.manager_records.resize((local+1)*0xb0c);put16(g.manager_records,local*0xb0c+0x290,uint16_t(team));g.manager_records[local*0xb0c+0x210]=before.world.catalogue.league(before.world.clubs.at(manager.club).league).local_index;
        std::copy(manager.career->season_records->begin(),manager.career->season_records->end(),g.manager_records.begin()+local*0xb0c+0xda);const auto& history=*manager.career->season_services;g.manager_records[local*0xb0c+0x284]=history.strength_history_count;std::copy(history.strength_history.begin(),history.strength_history.end(),g.manager_records.begin()+local*0xb0c+0x222);std::copy(history.position_history.begin(),history.position_history.end(),g.manager_records.begin()+local*0xb0c+0x248);}
    return g;
}
void original_round_oracle(uint8_t count,uint32_t season){
    const std::vector<LeagueId> leagues{{1},{6},{4},{8}};const auto before=fixture(leagues,count,season);auto state=before;const auto completed=score_round(state,leagues);auto expected=state;
    for(auto country:{Country::germany,Country::england}){
        const uint8_t de[9]={18,18,18,18,18,12,12,12,12},en[9]={20,20,20,20,10,12,12,12,12};set_league_sizes(country==Country::germany?de:en);
        const uint8_t de_first[8]={0,1,2,4,5,6,7,8},en_first[8]={0,1,2,3,5,6,7,8},de_count[8]={1,1,2,1,1,1,1,1},en_count[8]={1,1,1,1,1,1,1,1};set_league_groups(country==Country::germany?de_first:en_first,country==Country::germany?de_count:en_count);
        auto native=native_history(before,state,country);WatcomRandom random(91);MinuteReport report;advance_league_rounds(native,(1u<<1)|(1u<<3),random,report);check(!report.unsupported&&random.calls()==0,"Original post-round oracle changed RNG or failed");
        size_t local=0;for(auto& manager:expected.world.managers)if(expected.world.clubs.at(manager.club).country==country){auto& career=*manager.career;std::copy_n(native.manager_records.begin()+local*0xb0c+0xda,150,career.season_records->begin());const auto league=expected.world.catalogue.league(expected.world.clubs.at(manager.club).league);if(country==Country::england&&league.local_index==3)(*career.season_records)[std::min(season,49u)*3]=uint8_t((*career.season_records)[std::min(season,49u)*3]+18);auto& history=*career.season_services;history.strength_history_count=native.manager_records[local*0xb0c+0x284];std::copy_n(native.manager_records.begin()+local*0xb0c+0x222,38,history.strength_history.begin());std::copy_n(native.manager_records.begin()+local*0xb0c+0x248,38,history.position_history.begin());++local;}
    }
    record_completed_league_history(state,before,completed);check(state==expected,"typed post-round history differs from Original fields or documented English correction");check(state.game_seed==17&&state.watcom_seed==91&&read_save(make_save(state))==state,"post-round history changed RNG or failed save/reload");
    const auto after=state;rejects([&]{record_completed_league_history(state,before,completed);},"post-round replay accepted");check(state==after,"post-round replay leaked history changes");
    std::cout<<"Mixed DE/EN history Native oracle: count="<<int(count)<<" season="<<season<<" full records/rank/strength,38rollover and exactzeroRNG passed\n";
}
void bounded_and_unknown(){
    const auto before=fixture({{2},{3}});auto state=before;const auto completed=score_round(state,{{2},{3}});record_completed_league_history(state,before,completed);for(const auto& manager:state.world.managers){const auto& history=*manager.career->season_services;check((*manager.career->season_records)[0]>=37&&(*manager.career->season_records)[0]<=54&&history.strength_history_count==1,"German parallel regional history did not share its actual18-slot band");}
    auto unknown=fixture({{0},{9}});unknown.world.managers[0].career->season_records.reset();unknown.world.managers[0].career->season_services.reset();const auto unknown_before=unknown;const auto rounds=score_round(unknown,{{0},{9}});record_completed_league_history(unknown,unknown_before,rounds);check(!unknown.world.managers[0].career->season_records&&!unknown.world.managers[0].career->season_services,"new round invented older unknown history");check((*unknown.world.managers[1].career->season_records)[0]>=81&&(*unknown.world.managers[1].career->season_records)[0]<=90,"actual internal10-club owner was mapped to an invented visible league");
    auto unchanged=before;record_completed_league_history(unchanged,before,{});check(unchanged==before,"empty round list changed history");
    auto failed=before;auto scored=score_round(failed,{{2},{3}});auto tampered=scored;tampered.back().round=4;const auto intact=failed;rejects([&]{record_completed_league_history(failed,before,tampered);},"wrong final round accepted");check(failed==intact,"invalid final round published partial history");
}
void atomic_day(){
    auto before=fixture({{1},{6},{4},{8}});before.calendar.emplace();before.calendar->next_day=40;
    for(const auto& league:before.world.catalogue.leagues)for(int round=1;round<=league.rounds();++round){const auto day=uint16_t(round<3?round*8:round==3?40:100+round*5);before.calendar->schedule.leagues.push_back({day,league.id,uint8_t(round)});}
    std::sort(before.calendar->schedule.leagues.begin(),before.calendar->schedule.leagues.end(),[](const auto& a,const auto& b){return a.day!=b.day?a.day<b.day:a.league<b.league;});before.validate();
    const auto ticket=*prepare_day(before);auto expected=before;std::vector<LeagueId> leagues;for(const auto& plan:ticket.leagues)leagues.push_back(plan.appointment.league);const auto completed=score_round(expected,leagues);
    record_completed_league_history(expected,before,completed);++expected.calendar->next_day;++expected.calendar->revision;++expected.date;
    auto actual=before;commit_league_day(actual,ticket,completed);check(actual==expected,"completed-day owner did not publish exact history with scores/date");const auto after=actual;rejects([&]{commit_league_day(actual,ticket,completed);},"day history receipt replay accepted");check(actual==after,"replayed day changed history or date");
    auto invalid=before;invalid.calendar->revision=UINT64_MAX;const auto exhausted=invalid;rejects([&]{commit_league_day(invalid,*prepare_day(invalid),completed);},"exhausted day published history");check(invalid==exhausted,"exhausted day leaked partial history");
}

}
int main()try{set_fixture_tables(bmh::test::authored_fixture_tables());original_round_oracle(0,0);original_round_oracle(38,50);bounded_and_unknown();atomic_day();return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
