// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "world12_national_fixture.hpp"
#include "world12_national_programme_fixture.hpp"
#include "game/world12/national_setup.hpp"
#include "game/world12/national_coach.hpp"
#include "game/cup.hpp"
namespace bmh::test::world12 {
inline World inactive_master_fixture(){auto world=complete_world();for(auto& [id,club]:world.clubs)for(size_t row=0;row<11;++row){auto& player=world.players.at(club.roster[row].player).attributes;player.w364=uint32_t(row%4);player.w368=uint32_t(club.country)<<27;}
    const auto crest=world.clubs.begin()->second.crest;
    for(uint32_t n=0;n<54;++n){PlayerRecord player;player.id={0xd1000000u+n};player.attributes={"Authored Source pool "+std::to_string(n),50u|(60u<<7)|(65u<<14),(n<50?5u:0u)<<27,n%4};world.players[player.id]=player;}
    for(uint32_t c=0;c<62;++c){ClubRecord nation;nation.id={0xe1000000u+c};nation.country=Country(c);nation.kind=ClubKind::national_team;nation.name="Authored nation "+std::to_string(c);nation.crest=crest;world.clubs[nation.id]=nation;}
    for(uint32_t c=5;c<32;++c)for(uint32_t slot=0;slot<7;++slot){ClubRecord club;club.id={0xe2000000u+c*7+slot};club.country=Country(c);club.name="Authored foreign pool "+std::to_string(c)+"/"+std::to_string(slot);club.crest=crest;world.clubs[club.id]=club;}
    world.validate();return world;
}
inline GameState inactive_reference_fixture(){auto reference=national_reference();reference.stocks.resize(12*0x5c);reference.memory.add_block(layout::FundBlock,6*0x1c);reference.memory.add_block(layout::EditorList,44*0x58);for(size_t n=0;n<4;++n)reference.memory.set16(layout::EditorList+40*0x58+uint32_t(n*2),n?65535:0);return reference;}
inline World inactive_national_fixture(const World& master,const GameState& reference,Country primary,GameRandom& game,WatcomRandom& random){auto selected=select_world_scope(master,{false,false,true});InitialStrengthRanges ranges;for(auto& range:ranges)range={20,80};const auto receipt=initialize_national_only_players(selected,master,primary,ranges,reference,false,game,random);selected.settings=CareerSettings{3,2,5,1,false};initialize_national_coaches(selected,reference);for(auto& record:*selected.national_coaches)if(selected.clubs.at(record.team).country==primary)record.startup=receipt;selected.validate();return selected;}
inline LeImage inactive_rules_fixture(bool english){auto source=country_rules(english);const auto national=national_rules();for(uint8_t entry=1;entry<=14;++entry){const auto at=cup_entry_address(entry);source.patch(at,national.data(at),0x28);}return source;}
}
