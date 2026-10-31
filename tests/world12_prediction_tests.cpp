// SPDX-License-Identifier: GPL-3.0-only
#include "world12_fixture.hpp"
#include "fixture_tables.hpp"
#include "game/world12/prediction.hpp"
#include "game/messages.hpp"
#include "game/schedule.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
namespace {
using namespace bmh::world12;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
}
int main()try {
    bmh::set_fixture_tables(bmh::test::authored_fixture_tables());const auto master=bmh::test::world12::complete_world();size_t comparisons=0,ten_team=0;
    for(const auto& league:master.catalogue.leagues)for(int rounds:{0,1,2,league.rounds()-1,league.rounds()})for(int mode:{1,2,3,4,8})for(int deduction:{0,31}) {
        CareerSnapshot state;state.world=master;state.results=SeasonResults::empty(master.catalogue);state.league_rounds[league.id.value]=uint8_t(rounds);
        for(int round=1;round<=rounds;++round)for(int match=0;match<league.clubs/2;++match)state.results.result(master.catalogue,league.id,round,match)={true,0,0};
        const auto club=master.membership[league.id.value].clubs[0];const auto before=state;
        const auto prediction=predict_league(state,club,mode,{{club,uint8_t(deduction)}});
        check(state==before,"read-only forecast changed snapshot");
        if(league.clubs==10) {
            if(rounds<2||rounds==league.rounds())check(prediction.result.best_rank==0&&!prediction.source_size_word,"ten-team forecast did not retain Source early return");
            else if(!deduction&&mode==3)check(prediction.result.best_rank==10,"Source tied-form forecast must retain its pessimistic tie quirk");
            else if(!deduction&&mode==2)check(prediction.result.best_rank==1,"ten-team optimistic forecast wrong");
            ++ten_team;continue;
        }
        uint8_t sizes[bmh::kLeagueCount];for(auto& size:sizes)size=league.clubs;bmh::runtime_dimensions.league_teams=league.clubs*9;bmh::runtime_dimensions.expanded=true;bmh::set_league_sizes(sizes);
        const int native_league=1,team=league.clubs;
        bmh::kLeagueRelegation[native_league]=league.relegation;
        int europe=0;
        if(league.playable) {
            if(league.local_index==0)europe=4;
            else if(league.country==Country::england)europe=4;
            else {const int places[]={4,3,2,2,4};europe=places[league.local_index];}
        }
        bmh::kLeagueEurope[native_league]=uint8_t(europe);
        bmh::GameState native;native.results.assign(9*0x17c,0xcc);native.league_rounds.assign(9,0);native.league_rounds[native_league]=uint8_t(rounds);
        native.manager_teams={team};native.manager_records.resize(0xb0c);native.manager_records[0x220]=uint8_t(0x60|deduction);
        native.club_ids.resize(9*league.clubs);native.club_names={'C','l','u','b',0};native.mailbox.resize(bmh::kMailboxEntries*bmh::kMailboxEntry);
        for(int r=0;r<rounds;++r)for(int m=0;m<league.clubs/2;++m)native.results[size_t(native_league)*0x17c+size_t(r)*10+size_t(m)]=0;
        std::array<uint8_t,24> ranks;int size_word=-71;
        const int result=bmh::table_prediction(native,team,mode,ranks,&size_word);
        check(result==prediction.result.best_rank&&ranks==prediction.result.ranks,"typed forecast differs from Native table/rank calculation");
        check(size_word==(prediction.source_size_word?int(*prediction.source_size_word):-71),"typed forecast lost untouched stale-size word");
        const auto notices=uint8_t(prediction.result.lost_goals|(prediction.result.secured_goals<<3));
        check(native.manager_records[0x28b]==notices,"typed forecast goal transitions differ from Source flags");++comparisons;
    }
    std::cout<<comparisons<<" typed/native forecasts and "<<ten_team<<" ten-team boundary/tie cases passed; wide IDs, deductions, early returns and notice masks retained\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
