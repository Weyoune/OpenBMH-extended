// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "annual_qualification_fixture.hpp"
#include "game/world12/manager_tactics.hpp"
#include "game/messages.hpp"
namespace bmh::test::world12 {
using namespace bmh;using namespace bmh::world12;
// Authored account/academy operands. The closing score programme is explicitly
// synthetic; this fixture is never used as a production game fallback.
inline GameState annual_reference() {
    GameState g;g.club_ids.resize(445);g.team_flags.resize(445);g.team_morale.resize(445);g.formations.resize(0x580);
    g.manager_records.resize(4*0xb0c);g.manager_ratings.resize(4*60);g.tactics.resize(4*0x840);g.manager_options.resize(20);
    g.memory.add_block(layout::Loans,4*0x78);g.mailbox.assign(kMailboxEntries*kMailboxEntry,0);
    const uint8_t depth[]={0,5,5,5,5,10,10,10,10,17,17};
    for(size_t m=0;m<4;++m){g.manager_options[m*5]=1;for(size_t mode=0;mode<2;++mode)for(size_t formation=0;formation<8;++formation)for(size_t r=0;r<11;++r){const auto at=m*0x160+mode*0xb0+formation*22+r;g.formations[at]=depth[r];g.formations[at+11]=uint8_t(r%9);}}
    return g;
}
inline TextDb annual_texts(){TextDb t;for(size_t i=0;i<TEXT_COUNT;++i){t.strings.push_back("Annual "+std::to_string(i));t.offsets.push_back(t.blob.size());const auto& s=t.strings.back();t.blob.insert(t.blob.end(),s.begin(),s.end());t.blob.push_back(0);}return t;}
inline CareerSnapshot completed_account_closing(const GameState& rules,GameScope scope,bool academy) {
    auto s=bmh::test::world12::completed_qualification_closing(scope);auto& w=s.world;uint32_t next=0xd1000000;
    for(auto& [id,club]:w.clubs)if(club.league.valid())for(size_t row=0;row<20;++row){
        if(club.roster[row].empty()){PlayerRecord player;player.id={next++};player.attributes.name="Reserve";w.players[player.id]=player;club.roster[row]={player.id,uint16_t(row<17?2:0)};}
        auto& player=w.players.at(club.roster[row].player).attributes;player.w364=uint32_t(row==0||row==11?0:row<5?1:row<9?2:3);player.w35c=60u|(75u<<7)|(85u<<14);
    }
    const auto initial_fields=w.initial_competitions;const auto completed_campaigns=w.european_campaigns;w.initial_competitions.reset();w.european_campaigns.reset();
    auto& manager=w.managers[0];manager.finance.reset();manager.tactics.reset();WatcomRandom random(51);
    initialize_manager_contracts(w,manager.seat,rules,{3,false,false,false,2},random);
    w.initial_competitions=initial_fields;w.european_campaigns=completed_campaigns;
    auto& ready=w.managers[0];ready.finance->balance+=5000000;
    ready.career->season_services->promotion_tail_flags=0;
    ready.career->environment.youth[2]={90,5,0};if(academy)ready.career->environment.monthly[1]=8000;
    w.economy->managers[ready.seat].monthly.emplace();w.economy->managers[ready.seat].depreciation=0;
    w.amateur_slots.emplace();
    for(auto country:{Country::germany,Country::england})if(w.catalogue.selectable(country))for(uint32_t row=0;row<8;++row){
        ClubRecord club;club.id={next++};club.country=country;club.name="Authored amateur "+std::to_string(row);club.crest={club.id.value};
        w.crests[club.crest]={"W/TEST.VGA",0};w.clubs[club.id]=club;(*w.amateur_slots)[country==Country::germany?0:1].push_back(club.id);
    }
    w.transfer_market.emplace();s.transfers.emplace();s.validate();return s;
}
}
