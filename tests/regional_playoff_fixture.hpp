// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <algorithm>
#include "world12_fixture.hpp"
#include "game/world12/regional_playoff.hpp"
#include "game/world12/save.hpp"
#include "game/world12/manager_finance.hpp"
namespace bmh::test::world12 {
inline LeImage regional_rules(){
    auto source=country_rules(false);const auto entry=cup_entry_address(35);
    const auto byte=[&](uint32_t at,uint8_t value){source.patch(at,&value,1);};
    byte(entry+0x14,195);byte(entry+0x15,255);byte(entry+0x18,13);byte(entry+0x19,0);byte(entry+0x1a,4);byte(entry+0x1b,6);byte(entry+0x1e,1);byte(entry+0x1f,2);
    return source;
}
inline bmh::world12::CareerSnapshot before_regional_final(const LeImage& source){
    using namespace bmh::world12;CareerSnapshot state;state.world=complete_world();state.world.players_initialized=true;
    state.results=SeasonResults::empty(state.world.catalogue);state.calendar.emplace();state.calendar->next_day=272;state.date=272;
    auto& calendar=state.calendar->schedule;for(const auto& league:state.world.catalogue.leagues)for(int round=1;round<=league.rounds();++round)calendar.leagues.push_back({uint16_t(round*8),league.id,uint8_t(round)});
    std::sort(calendar.leagues.begin(),calendar.leagues.end(),[](const auto& a,const auto& b){return a.day!=b.day?a.day<b.day:a.league<b.league;});
    for(uint16_t day:{281,285,289,293,297,301})calendar.others.push_back({day,CalendarDomain::germany,0x4004});
    for(const auto& appointment:calendar.leagues)if(appointment.day<272){state.league_rounds[appointment.league.value]=appointment.round;const int count=state.world.catalogue.league(appointment.league).clubs/2;for(int m=0;m<count;++m)state.results.result(state.world.catalogue,appointment.league,appointment.round,m)={true,uint8_t((m+appointment.round)%4),uint8_t((m*3+appointment.round)%3)};}
    state.world.regional_playoff=make_regional_playoff_season(state.world,source,calendar,0);state.game_seed=17;state.watcom_seed=91;state.validate();return state;
}
inline void authored_final_day_results(bmh::world12::CareerSnapshot& state){
    for(const auto& appointment:state.calendar->schedule.leagues_on(272)){state.league_rounds[appointment.league.value]=appointment.round;for(int m=0;m<state.world.catalogue.league(appointment.league).clubs/2;++m)state.results.result(state.world.catalogue,appointment.league,appointment.round,m)={true,uint8_t((m+3)%4),uint8_t(m%3)};}
}
}
