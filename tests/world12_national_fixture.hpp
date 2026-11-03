// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "world12_fixture.hpp"
#include "game/state.hpp"
#include <algorithm>
namespace bmh::test::world12 {
using namespace bmh;using namespace bmh::world12;
inline GameState national_reference() {
    GameState g;g.manager_records.resize(5*0xb0c);g.manager_options.resize(20);
    g.tactics.resize(5*0x840);g.formations.resize(0x580);g.club_ids.resize(445);
    g.team_flags.resize(445);g.team_morale.resize(445,25);g.club_heap_78.resize(890);
    g.manager_ratings.resize(240);g.manager_results.resize(size_t(runtime_dimensions.record_teams)*40,255);
    g.memory.add_block(layout::CompetitionSelection,4);g.memory.set32(layout::CompetitionSelection,3);
    const uint8_t depth[]={0,5,5,5,5,10,10,10,10,17,17},lane[]={4,1,3,5,7,1,3,5,7,3,5};
    for(size_t m=0;m<4;++m)for(size_t mode=0;mode<2;++mode)for(size_t f=0;f<4;++f)for(size_t r=0;r<11;++r) {
        const auto at=m*0x160+mode*0xb0+f*22+r;g.formations[at]=depth[r];g.formations[at+11]=lane[r];
    }
    return g;
}
inline World national_world() {
    World w=bmh::test::world12::complete_world();uint32_t next=1000000;
    for(auto& [id,club]:w.clubs) {
        for(size_t r=11;r<20;++r) {
            PlayerRecord p;p.id={next++};p.attributes={"Reserve "+std::to_string(p.id.value),0,0,0};
            w.players[p.id]=p;club.roster[r]={p.id,uint16_t(r<17?2:0)};
        }
        for(size_t r=0;r<20;++r) {
            auto& p=w.players.at(club.roster[r].player).attributes;
            p.w35c=60u|(uint32_t(60+r%8)<<7)|(uint32_t(75+r%7)<<14);
            p.w368=uint32_t(club.country)<<27;
            p.w364=uint32_t(r<3?0:r<8?1:r<15?2:3)|(10u<<21);
        }
    }
    for(auto country:{Country::germany,Country::england}) {
        ClubRecord nation;nation.id={3000000+uint32_t(country)};nation.name="National "+std::to_string(int(country));
        nation.country=country;nation.kind=ClubKind::national_team;nation.crest={nation.id.value};
        w.crests[nation.crest]={"W/TEST.VGA",0};w.clubs[nation.id]=nation;
    }
    w.players_initialized=true;w.markets.emplace();w.free_players.emplace();w.settings=CareerSettings{3,6,5,2,false};
    const auto base=national_reference();
    for(auto country:{Country::germany,Country::england}) {
        Manager m;m.seat=country==Country::germany?0:3;m.portrait=m.seat;m.name="Manager "+std::to_string(m.seat);
        m.start_league=w.catalogue.starting_league(country,0);m.club=w.membership[m.start_league.value].clubs[0];
        ManagerTactics t;t.backing.emplace();std::copy_n(base.formations.begin(),0x160,t.backing->formations.begin());
        std::copy_n(base.formations.begin()+0x160,13,t.backing->formation_tail.begin());
        t.backing->enabled_formations={15,15};t.automatic_formation=1;m.tactics=t;w.managers.push_back(m);
    }
    w.validate();return w;
}
}
