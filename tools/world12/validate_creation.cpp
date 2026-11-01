// SPDX-License-Identifier: GPL-3.0-only
#include "game/world12/initialization.hpp"
#include "game/world12/codec.hpp"
#include "game/world12/manager_tactics.hpp"
#include "game/world12/manager_finance.hpp"
#include "game/world12/manager_match_state.hpp"
#include "game/world12/ai_match.hpp"
#include "game/world12/save.hpp"
#include "game/national.hpp"
#include "game/lineup.hpp"
#include "game/career.hpp"
#include "game/schedule.hpp"
#include "formats/data_profile.hpp"
#include "formats/le.hpp"
#include "launch/fltd.hpp"
#include <iostream>
int main(int argc,char** argv)try {
    using namespace bmh;using namespace bmh::world12;
    if(argc!=4)throw std::runtime_error("usage: world12_creation_validate <WORLD12.JSON> <German reference rules directory> <supplied FLTD.EXE>");
    const auto master=decode_world(read_file(argv[1]));const std::filesystem::path data=argv[2];
    const auto profile=load_data_profile(data);install_runtime_profile(profile);
    const auto german=load_le(data/"BMH.exe"),english=bmh::launch::translate_fltd(argv[3]);install_career_rules(german);install_fixture_tables(german);uint8_t sizes[kLeagueCount];for(size_t i=0;i<kLeagueCount;++i)sizes[i]=profile.leagues[i];set_league_sizes(sizes);
    const auto db=load_database(data/"M3D.MAN",profile.counts);
    auto expected=startup_state(german,db,read_file(data/"M3.MEN"),read_file(data/"M3.DAT"));
    const GameScope scope{true,false,true};auto world=select_world_scope(master,scope);
    const auto ranges=read_initial_strength_ranges(world.catalogue,german,english);
    const auto plan=make_player_initialization_plan(master,scope,ranges);
    for(size_t i=0;i<54;++i)if(plan.extra_players[i].value!=expected.squad_slots.at(size_t(runtime_dimensions.extra_slots_first)+i))
        throw std::runtime_error("source extra-player order differs from typed master");
    GameRandom a(17),b(17);WatcomRandom ar(91),br(91);
    const auto initial_rules=expected;
    startup_players(expected,a,ar);initialize_career_players(world,plan,b,br);
    if(a.seed()!=b.seed()||a.calls()!=b.calls()||ar.seed()!=br.seed()||ar.calls()!=br.calls())throw std::runtime_error("typed startup RNG differs from direct Original startup_players");
    if(world.players.size()!=expected.players.size())throw std::runtime_error("source startup player coverage differs");
    for(const auto& [id,p]:world.players){const auto& original=expected.players.at(id.value);
        if(p.attributes.w35c!=original.w35c||p.attributes.w368!=original.w368||p.attributes.w364!=original.w364)
            throw std::runtime_error("typed startup attributes differ from Original source player");}
    if(decode_world(encode_world(world))!=world)throw std::runtime_error("initialized source data failed roundtrip");
    world.managers={{0,"Manager",world.membership[0].clubs[0],{0},0}};
    expected.manager_teams={0};initialize_career_manager_team_settings(expected,0);expected.manager_records[0x288]=0x16;expected.manager_records[0x218]=expected.manager_records[0x219]=15;
    expected.level=3;expected.projected_foreign_rule=german.u8(layout::ForeignPlayerLimitEnabled)!=0;
    for(size_t row=0;row<24;++row)for(size_t at:{0x44u,0x46u}){expected.tactics[row*0x58+at]=0xff;expected.tactics[row*0x58+at+1]=0x0f;}
    const auto squad=squad_range(0);
    for(size_t row=0;row<20;++row){auto& p=expected.players.at(expected.squad_slots[squad.first+row]&0xfff);p.w364=(p.w364&~(0x1fu<<21))|(uint32_t(ar.range(4,9))<<21);new_board_entry(expected,0,int(row),int(squad.first+row),ar);expected.tactics[row*0x58+0xd]=100;}
    initialize_career_manager_funds(expected,0,3,0);auto_lineup(expected,0,100);
    const TacticalPolicy policy{3,false,false,german.u8(layout::ForeignPlayerLimitEnabled)!=0,3};
    initialize_manager_contracts(world,0,initial_rules,policy,br);
    std::vector<PlayerId> mapping;for(size_t i=0;i<expected.players.size();++i)mapping.push_back({uint32_t(i)});
    if(world.managers[0].tactics!=manager_full_tactics_from_legacy(expected,0,mapping)||world.managers[0].finance!=manager_initial_finance_from_legacy(expected,0)||ar.seed()!=br.seed()||ar.calls()!=br.calls())
        throw std::runtime_error("source manager board/account phase or RNG differs");
    for(const auto& [id,p]:world.players)if(p.attributes.w364!=expected.players.at(id.value).w364)throw std::runtime_error("source manager ages differ");
    for(size_t row=0;row<24;++row){const auto raw=expected.squad_slots[squad.first+row];const auto& got=world.clubs.at(world.managers[0].club).roster[row];
        if((raw&0xfff)==0xfff){if(!got.empty())throw std::runtime_error("source initial empty row differs");continue;}
        const auto flags=uint16_t(((raw&0x8000)?1:0)|((raw&0x4000)?2:0)|((raw&0x2000)?4:0)|((raw&0x1000)?8:0));
        if(got.player.value!=(raw&0xfff)||got.flags!=flags||world.players.at(got.player).cards!=expected.player_cards.at(raw&0xfff))throw std::runtime_error("source initial roster/card mapping differs");}
    if(world.managers[0].match_state!=manager_match_state_from_legacy(expected,0,{},{}))throw std::runtime_error("source initial human match state differs");
    if(decode_world(encode_world(world))!=world)throw std::runtime_error("source initialized manager failed roundtrip");
    CareerSnapshot human;human.world=world;human.results=SeasonResults::empty(world.catalogue);human.date=40;
    AiLeagueConditions conditions;conditions.month=8;conditions.temperature=19;conditions.date=human.date;conditions.spectators=15000;conditions.referee_properties=db.raw_referee_properties;conditions.presentation_buttons.assign(0xb4,1);
    const auto fixtures=league_fixtures(world,{0},1);std::vector<AiLeagueConditions> inputs(fixtures.size(),conditions);GameRandom hg(17);WatcomRandom hw(91);
    HumanRoundHooks hooks;int presented=0;hooks.live_event=[&](auto&,auto&,const auto&){++presented;};
    const auto round=simulate_and_settle_human_league_round(human,initial_rules,{0},1,inputs,{3,3,policy.foreign_rule_enabled,true},hg,hw,hooks);
    if(read_save(make_save(human))!=human)throw std::runtime_error("source human round snapshot mismatch");
    int goals=0;for(const auto& match:round.matches){if(!match.settled||match.end_minute<90)throw std::runtime_error("source human match did not complete");goals+=match.score.home+match.score.away;}
    std::cout<<round.matches.size()<<" source-data human/AI league matches, goals="<<goals<<", human events="<<presented<<"; actual scoring/settlement/snapshot passed (isolated probe)\n";
    std::cout<<"Source manager board/contracts/account phase and ages match direct Core calls; this excludes earlier club/stadium selection\n";
    std::cout<<world.players.size()<<" real-data player records and both RNG streams match direct Original startup_players; initialized world roundtrip passed\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
