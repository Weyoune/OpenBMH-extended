// SPDX-License-Identifier: GPL-3.0-only
#include "game/world12/initialization.hpp"
#include "game/world12/codec.hpp"
#include "game/world12/manager_tactics.hpp"
#include "game/world12/manager_finance.hpp"
#include "game/world12/manager_match_state.hpp"
#include "game/world12/manager_creation.hpp"
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
#include <algorithm>
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
    new_career_markets_and_moods(expected,a,ar);
    initialize_career_markets_and_moods(world,initial_rules,b,br);
    if(a.seed()!=b.seed()||a.calls()!=b.calls()||ar.seed()!=br.seed()||ar.calls()!=br.calls())throw std::runtime_error("typed market/mood RNG differs from direct Original initialization");
    if(!std::equal(expected.stocks.begin(),expected.stocks.end(),world.markets->stocks.begin()))throw std::runtime_error("source stock history/trends differ");
    for(size_t i=0;i<world.markets->funds.size();++i)if(world.markets->funds[i]!=expected.memory.u8(layout::FundBlock+uint32_t(i)))throw std::runtime_error("source initial fund state differs");
    for(const auto& [id,club]:world.clubs)if(club.morale!=expected.team_morale.at(id.value)||club.match_flags!=expected.team_flags.at(id.value))throw std::runtime_error("source initial club mood/balance differs");
    if(decode_world(encode_world(world))!=world)throw std::runtime_error("source market/mood state failed roundtrip");
    const bool foreign_rule=german.u8(layout::ForeignPlayerLimitEnabled)!=0;expected.projected_foreign_rule=foreign_rule;
    MinuteReport initialization_report;
    if(!new_career_players(expected,a,ar,initialization_report))throw std::runtime_error("Source age/free-player phase failed");
    initialize_career_player_pools(world,plan,initial_rules,foreign_rule,b,br);
    if(a.seed()!=b.seed()||a.calls()!=b.calls()||ar.seed()!=br.seed()||ar.calls()!=br.calls())throw std::runtime_error("typed age/free-player RNG differs from Source");
    for(const auto& [id,p]:world.players){const auto& native=expected.players.at(id.value);
        if(p.attributes.w35c!=native.w35c||p.attributes.w368!=native.w368||p.attributes.w364!=native.w364||p.bonus!=expected.player_bonus.at(id.value)||p.cards!=expected.player_cards.at(id.value))throw std::runtime_error("Source age/release/player-card state differs");}
    for(const auto& members:world.membership)for(auto id:members.clubs){const auto& club=world.clubs.at(id);const auto range=squad_range(int(id.value));
        if(club.match_flags!=expected.team_flags.at(id.value))throw std::runtime_error("Source overflow lineup flags differ");
        for(size_t row=0;row<24;++row){const auto raw=expected.squad_slots.at(range.first+row);const auto& slot=club.roster[row];
            if((raw&0xfff)==0xfff){if(!slot.empty())throw std::runtime_error("Source initial roster empty differs");}
            else if(slot.player.value!=(raw&0xfff)||slot.flags!=uint16_t(((raw&0x8000)?1:0)|((raw&0x4000)?2:0)|((raw&0x2000)?4:0)|((raw&0x1000)?8:0)))throw std::runtime_error("Source initial overflow roster differs: club="+std::to_string(id.value)+" row="+std::to_string(row)+" expected="+std::to_string(raw)+" got_player="+std::to_string(slot.player.value)+" flags="+std::to_string(slot.flags));}}
    const auto compare_pool=[&](const auto& pool,int first){for(size_t i=0;i<pool.size();++i){const auto raw=expected.squad_slots.at(size_t(first)+i);if((raw==0xfff)!=(!pool[i].valid())||(raw!=0xfff&&raw!=pool[i].value))throw std::runtime_error("Source initial free pool differs");}};
    compare_pool(world.free_players->domestic,runtime_dimensions.free_first);compare_pool(world.free_players->foreign,runtime_dimensions.foreign_free_first);
    if(decode_world(encode_world(world))!=world)throw std::runtime_error("source free-player phase failed roundtrip");
    size_t full_cases=0;
    for(uint8_t tier:{uint8_t(0),uint8_t(2),uint8_t(4)})for(uint8_t duration:{uint8_t(1),uint8_t(5)})for(uint32_t selected:{0u,72u,164u}) {
        auto actual=world;auto native=expected;auto ag=b;auto aw=br;auto ng=a;auto nw=ar;
        native.memory.set8(layout::ManagerCount,1);native.memory.set8(layout::GameMode,3);native.memory.set8(layout::Difficulty,3);native.memory.set8(layout::CareerDuration,duration);native.level=3;
        career_managers_begin(native,false);
        // Empty Original BSS marking words are not references to player zero.
        for(size_t row=0;row<24;++row)for(size_t at:{0x44u,0x46u}){native.tactics[row*0x58+at]=0xff;native.tactics[row*0x58+at+1]=0x0f;}
        MinuteReport report;
        if(!career_add_manager(native,german,0,{"Full manager",int(selected),int(tier),1,0},false,ng,nw,report))throw std::runtime_error("Source full manager confirmation failed");
        add_initial_manager(actual,{0,{selected},{tier},"Full manager",0,1},{3,3,duration,1,foreign_rule},ranges,initial_rules,german,ag,aw);
        if(ag.seed()!=ng.seed()||ag.calls()!=ng.calls()||aw.seed()!=nw.seed()||aw.calls()!=nw.calls())throw std::runtime_error("full manager creation RNG mismatch: tier="+std::to_string(tier)+" duration="+std::to_string(duration)+" club="+std::to_string(selected));
        std::vector<PlayerId> map;for(size_t i=0;i<native.players.size();++i)map.push_back({uint32_t(i)});
        const auto& manager=actual.managers[0];
        if(manager.career!=manager_career_state_from_legacy(native,0,map))throw std::runtime_error("full manager facilities/environment/sponsor state differs");
        if(manager.tactics!=manager_full_tactics_from_legacy(native,0,map)||manager.finance!=manager_initial_finance_from_legacy(native,0)||manager.match_state!=manager_match_state_from_legacy(native,0,{},{}))throw std::runtime_error("full manager account/tactics/match state differs");
        for(const auto& [id,p]:actual.players){const auto& n=native.players.at(id.value);if(p.attributes.w35c!=n.w35c||p.attributes.w368!=n.w368||p.attributes.w364!=n.w364||p.bonus!=native.player_bonus.at(id.value)||p.cards!=native.player_cards.at(id.value))throw std::runtime_error("full manager strength/age/player state differs");}
        size_t ordinal=0;
        for(auto table:{TableGroup::germany,TableGroup::europe})for(auto league:actual.catalogue.table_leagues(table))for(auto id:actual.membership[league.value].clubs){const auto& c=actual.clubs.at(id);
            if(c.name!=native.club_name(int(ordinal))||c.match_flags!=native.team_flags[ordinal]||c.morale!=(native.team_morale[ordinal]&63))throw std::runtime_error("full manager league membership/club state differs");
            const auto squad=squad_range(int(ordinal));for(size_t row=0;row<24;++row){const auto raw=native.squad_slots.at(squad.first+row);const auto& slot=c.roster[row];
                if((raw&0xfff)==0xfff){if(!slot.empty())throw std::runtime_error("full manager roster emptiness differs");}
                else if(slot.player.value!=(raw&0xfff)||slot.flags!=uint16_t(((raw&0x8000)?1:0)|((raw&0x4000)?2:0)|((raw&0x2000)?4:0)|((raw&0x1000)?8:0)))throw std::runtime_error("full manager roster differs");}
            ++ordinal;
        }
        CareerSnapshot snapshot;snapshot.world=actual;snapshot.results=SeasonResults::empty(actual.catalogue);snapshot.game_seed=ag.seed();snapshot.watcom_seed=aw.seed();
        if(read_save(make_save(snapshot))!=snapshot)throw std::runtime_error("full manager creation snapshot differs");++full_cases;
    }
    std::cout<<full_cases<<" source-data full manager confirmations: placement/strength/stadium/environment/contracts/snapshot and both RNGs match Native Core\n";
    auto combined=select_world_scope(master,{});const auto combined_ranges=read_initial_strength_ranges(combined.catalogue,german,english);
    const auto combined_plan=make_player_initialization_plan(master,{},combined_ranges);GameRandom cg(17);WatcomRandom cw(91);
    initialize_career_players(combined,combined_plan,cg,cw);initialize_career_markets_and_moods(combined,initial_rules,cg,cw);initialize_career_player_pools(combined,combined_plan,initial_rules,foreign_rule,cg,cw);
    const uint32_t clubs_to_manage[]={0,108,164,526},leagues_to_manage[]={0,5,4,9};
    for(uint8_t seat=0;seat<4;++seat){const bool en=seat%2;
        add_initial_manager(combined,{seat,{clubs_to_manage[seat]},{leagues_to_manage[seat]},"Mixed "+std::to_string(seat),seat,1},{3,3,5,4,foreign_rule},combined_ranges,initial_rules,en?english:german,cg,cw);}
    CareerSnapshot mixed;mixed.world=combined;mixed.results=SeasonResults::empty(combined.catalogue);mixed.date=40;
    size_t mixed_matches=0;GameRandom mixed_game(17);WatcomRandom mixed_random(91);HumanRoundHooks mixed_hooks;mixed_hooks.live_event=[](auto&,auto&,const auto&){};
    for(LeagueId league:{LeagueId{0},LeagueId{5},LeagueId{4},LeagueId{9}}){const auto fixtures=league_fixtures(mixed.world,league,1);AiLeagueConditions conditions;conditions.month=8;conditions.temperature=19;conditions.date=40;conditions.spectators=15000;conditions.referee_properties=db.raw_referee_properties;conditions.presentation_buttons.assign(0xb4,1);
        const auto played=simulate_and_settle_human_league_round(mixed,initial_rules,league,1,std::vector<AiLeagueConditions>(fixtures.size(),conditions),{3,3,foreign_rule,true},mixed_game,mixed_random,mixed_hooks);mixed_matches+=played.matches.size();
        for(size_t i=0;i<played.matches.size();++i)mixed.results.result(mixed.world.catalogue,league,1,int(i))=played.matches[i].score;
        mixed.league_rounds[league.value]=1;
    }
    mixed.game_seed=mixed_game.seed();mixed.watcom_seed=mixed_random.seed();
    if(read_save(make_save(mixed))!=mixed)throw std::runtime_error("mixed-country fully initialized human rounds failed snapshot roundtrip");
    std::cout<<combined.players.size()<<" actual combined players, four German/English managers with both supplied country rule images, "<<mixed_matches<<" human/AI fixtures and isolated league-round snapshot roundtrip passed\n";
    world.managers={{0,"Manager",world.membership[0].clubs[0],{0},0}};
    expected.manager_teams={0};initialize_career_manager_team_settings(expected,0);expected.manager_records[0x288]=0x16;
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
    for(size_t i=0;i<round.matches.size();++i)human.results.result(human.world.catalogue,{0},1,int(i))=round.matches[i].score;
    human.league_rounds[0]=1;human.game_seed=hg.seed();human.watcom_seed=hw.seed();
    if(read_save(make_save(human))!=human)throw std::runtime_error("source human round snapshot mismatch");
    int goals=0;for(const auto& match:round.matches){if(!match.settled||match.end_minute<90)throw std::runtime_error("source human match did not complete");goals+=match.score.home+match.score.away;}
    std::cout<<round.matches.size()<<" source-data human/AI league matches, goals="<<goals<<", human events="<<presented<<"; actual scoring/settlement/snapshot passed (isolated probe)\n";
    std::cout<<"Source manager board/contracts/account phase and ages match direct Core calls; this excludes earlier club/stadium selection\n";
    std::cout<<"Thirty stock days, six funds, every initial club mood/balance and both RNG streams match direct Core initialization\n";
    std::cout<<"Roster ages/familiarity, 54 regenerated extras, bounded free pools and overflow AI transfers match direct Core initialization\n";
    std::cout<<world.players.size()<<" real-data player records and both RNG streams match direct Original startup_players; initialized world roundtrip passed\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
