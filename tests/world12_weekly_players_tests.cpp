// SPDX-License-Identifier: GPL-3.0-only
#include "world12_fixture.hpp"
#include "fixture_tables.hpp"
#include "game/world12/weekly_players.hpp"
#include "game/world12/manager_tactics.hpp"
#include "game/world12/match_projection.hpp"
#include "game/week.hpp"
#include "game/weekly.hpp"
#include <algorithm>
#include <iostream>
namespace {
using namespace bmh;using namespace bmh::world12;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class F>void rejects(F f,const char* message){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}check(rejected,message);}
GameState reference() {
    GameState state;state.club_ids.resize(445);state.team_flags.resize(445);state.team_morale.resize(445);
    state.formations.resize(0x580);state.manager_records.resize(4*0xb0c);state.manager_ratings.resize(4*60);
    state.tactics.resize(4*0x840);state.manager_options.resize(20);
    const uint8_t depth[]={0,5,5,5,5,10,10,10,10,17,17},lane[]={4,1,3,5,7,1,3,5,7,3,5};
    for(size_t m=0;m<4;++m){state.manager_options[m*5]=1;
        for(size_t mode=0;mode<2;++mode)for(size_t formation=0;formation<8;++formation)for(size_t row=0;row<11;++row){
            const auto at=m*0x160+mode*0xb0+formation*22+row;
            state.formations[at]=depth[row];state.formations[at+11]=lane[row];
        }
    }
    return state;
}
CareerSnapshot fixture(const GameState& rules) {
    CareerSnapshot state;auto& world=state.world;world=bmh::test::world12::complete_world();world.players_initialized=true;
    uint32_t next=0x90000000u;
    for(auto& [id,club]:world.clubs)for(size_t row=0;row<20;++row) {
        if(club.roster[row].empty()) {
            PlayerRecord player;player.id={next++};player.attributes.name="Additional "+std::to_string(player.id.value);
            world.players.emplace(player.id,player);club.roster[row]={player.id,uint16_t(row<17?2:0)};
        }
        auto& player=world.players.at(club.roster[row].player).attributes;
        player.w35c=60u|(75u<<7)|(85u<<14);player.w368=0;
        player.w364=uint32_t(row==0||row==11?0:row<5?1:row<9?2:3);
    }
    const auto de=world.membership[0].clubs[0],en=world.membership[9].clubs[0];
    world.managers={{1,"German",de,{0},1},{3,"English",en,{9},3}};
    WatcomRandom random(91);
    initialize_manager_contracts(world,1,rules,{1,false,false,false,6},random);
    initialize_manager_contracts(world,3,rules,{1,false,false,false,6},random);
    world.markets.emplace();world.free_players.emplace();world.settings=CareerSettings{1,6,5,2,false};
    state.results=SeasonResults::empty(world.catalogue);state.date=40;state.game_seed=17;state.watcom_seed=91;
    state.validate();return state;
}
CareerSnapshot* changed_state=nullptr;
void interrupt(int){if(changed_state){++changed_state->watcom_seed;changed_state=nullptr;}}
}
int main()try {
    set_fixture_tables(bmh::test::authored_fixture_tables());
    const auto rules=reference();const auto source=fixture(rules);
    TextDb texts;texts.strings.resize(TEXT_COUNT,"Synthetic notice ");
    check(source.world.players.size()>4095,"weekly fixture did not exceed packed native player capacity");
    auto healing=source;
    const auto last_club=healing.world.membership[12].clubs.back();
    const auto patient=healing.world.clubs.at(last_club).roster[19].player;
    check(patient.value>0x7fffffffu,"last-batch recovery fixture did not use a wide global player ID");
    auto& p=healing.world.players.at(patient).attributes;p.w35c|=uint32_t(5)<<27;p.w364|=uint32_t(1)<<16;
    const auto original=healing;
    const auto recovered=update_weekly_players(healing,rules,texts);
    check(recovered.roster_slots==4680&&recovered.recovered==1&&recovered.injured==0&&recovered.retired==0,"quiet weekly pass lost a roster/batch or recovery");
    check(!(healing.world.players.at(patient).attributes.w35c>>27),"last-batch patient remained injured");
    check(healing.date==original.date&&healing.calendar==original.calendar&&healing.results==original.results,"weekly phase advanced calendar/results");
    check(read_save(make_save(healing))==healing,"weekly state did not survive snapshot roundtrip");
    auto replay=original;const auto replayed=update_weekly_players(replay,rules,texts);
    check(replay==healing&&replayed.game_draws==recovered.game_draws&&replayed.watcom_draws==recovered.watcom_draws,"weekly repeat with identical input is nondeterministic");

    auto retirement=source;const auto human=retirement.world.managers[0].club;
    const auto player=retirement.world.clubs.at(human).roster[0].player;
    auto& heavy=retirement.world.players.at(player).attributes;heavy.w35c|=uint32_t(10)<<27;heavy.w364|=uint32_t(30)<<16;
    uint32_t seed=1;for(;;++seed){WatcomRandom probe(seed);if(probe.range(0,0x25)==0)break;}
    retirement.watcom_seed=seed;const auto ended=update_weekly_players(retirement,rules,texts);
    check(ended.retired==1&&(retirement.world.players.at(player).attributes.w35c>>27)==31,"heavy injury did not end the career");
    check(ended.notices.size()==1&&ended.notices[0].recipients==(1u<<1)&&ended.notices[0].date==40,"weekly notice lost sparse global manager recipient/date");
    const auto stable=retirement.world.players.at(player);
    update_weekly_players(retirement,rules,texts);
    check(retirement.world.players.at(player)==stable,"retired player was healed by another weekly pass");

    auto training=source;const auto trained=train_manager_squads(training,rules);
    check(trained.managers==2&&trained.game_draws>0&&trained.watcom_draws>0,"human training skipped managers or RNG");
    check(training.world.managers[0].tactics!=source.world.managers[0].tactics,"familiarity/training changed no human board");
    check(training.date==source.date&&training.results==source.results&&training.world.clubs==source.world.clubs,"training changed calendar, results or roster ownership");
    check(read_save(make_save(training))==training,"training/development did not survive snapshot roundtrip");
    const auto foreign=source.world.membership[12].clubs[0];
    for(const auto& slot:source.world.clubs.at(foreign).roster)if(!slot.empty())check(training.world.players.at(slot.player)==source.world.players.at(slot.player),"human training changed an unrelated foreign player");

    // Native league arrays are deliberately poisoned: typed training must use
    // each projected club's actual round, never its temporary team number.
    auto zero_rules=rules,late_rules=rules;zero_rules.league_rounds.assign(9,0);late_rules.league_rounds.assign(9,255);
    auto zero_training=source,late_training=source;
    train_manager_squads(zero_training,zero_rules);train_manager_squads(late_training,late_rules);
    check(zero_training==late_training,"typed training depends on positional legacy round cursors");

    auto country_rounds=source;
    country_rounds.league_rounds[9]=18;
    for(int round=1;round<=18;++round)for(int match=0;match<5;++match)
        country_rounds.results.result(country_rounds.world.catalogue,{9},round,match)={true,0,0};
    const auto earlier=source.world.managers[0].club;
    train_manager_squads(country_rounds,late_rules);
    for(const auto& slot:source.world.clubs.at(earlier).roster)if(!slot.empty())
        check(country_rounds.world.players.at(slot.player)==training.world.players.at(slot.player),"English round cursor leaked into earlier German training");

    auto active=source;active.world.settings->mode=3;
    const auto ai=active.world.membership[12].clubs[0];
    auto domestic=AiMatchProjection::create(active.world,{ai},rules).state,foreign_state=domestic;
    const auto id=domestic.squad_slots[0]&0xfff;
    uint32_t game_seed=1;for(;;++game_seed){GameRandom probe{int32_t(game_seed)};if(probe.range(0,0xb4-domestic.level*2)==0)break;}
    uint32_t watcom_seed=1;for(;;++watcom_seed){WatcomRandom probe(watcom_seed);if(probe.range(0,1)==0)break;}
    GameRandom domestic_game{int32_t(game_seed)},foreign_game{int32_t(game_seed)};
    WatcomRandom domestic_random(watcom_seed),foreign_random(watcom_seed);MinuteReport a,b;
    weekly_player_update(domestic,0,false,domestic_game,domestic_random,a);
    weekly_player_update(foreign_state,0,true,foreign_game,foreign_random,b);
    check(!a.unsupported&&!b.unsupported&&(domestic.players[id].w35c>>27)!=0&&foreign_state.players[id].w35c==source.world.players.at(active.world.clubs.at(ai).roster[0].player).attributes.w35c,"explicit foreign injury throttle lost country policy");
    check(foreign_random.calls()==1&&domestic_random.calls()==2&&domestic_game.calls()==foreign_game.calls(),"foreign injury skip changed Source draw placement");

    auto interrupted=source;changed_state=&interrupted;draw_hook=interrupt;
    rejects([&]{train_manager_squads(interrupted,rules);},"training overwrote externally changed career");draw_hook=nullptr;changed_state=nullptr;
    auto externally_changed=source;++externally_changed.watcom_seed;
    check(interrupted==externally_changed,"training rollback lost external seed or leaked player changes");
    interrupted=source;interrupted.world.settings->mode=3;const auto running=interrupted;
    changed_state=&interrupted;draw_hook=interrupt;
    rejects([&]{update_weekly_players(interrupted,rules,texts);},"weekly processing overwrote externally changed career");draw_hook=nullptr;changed_state=nullptr;
    externally_changed=running;++externally_changed.watcom_seed;
    check(interrupted==externally_changed,"weekly rollback lost external seed or leaked injuries");
    auto broken=source;broken.world.players_initialized=false;const auto before=broken;
    rejects([&]{update_weekly_players(broken,rules,texts);},"uninitialized weekly career accepted");check(broken==before,"invalid weekly request changed state");
    std::cout<<"4,680-player weekly batches, wide IDs, sparse manager news, recovery/retirement, training, snapshots and stale-callback rollback passed\n";
}catch(const std::exception& error){draw_hook=nullptr;std::cerr<<error.what()<<'\n';return 1;}
