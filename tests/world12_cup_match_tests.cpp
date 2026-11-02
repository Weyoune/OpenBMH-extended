// SPDX-License-Identifier: GPL-3.0-only
#include "world12_fixture.hpp"
#include "fixture_tables.hpp"
#include "game/world12/cup_match.hpp"
#include "game/world12/manager_tactics.hpp"
#include "game/world12/save.hpp"
#include "app/world12_cups.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
namespace {
using namespace bmh;using namespace bmh::world12;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class F>void rejects(F f,const char* message){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}check(rejected,message);}
GameState rules() {
    GameState g;g.club_ids.resize(445);g.team_flags.resize(445);g.team_morale.resize(445);
    g.manager_records.resize(4*0xb0c);g.tactics.resize(4*0x840);g.manager_options.resize(20);
    g.manager_ratings.resize(240);g.formations.resize(0x580);
    const uint8_t depth[]={0,5,5,5,5,10,10,10,10,17,17},lane[]={4,1,3,5,7,1,3,5,7,3,5};
    for(size_t m=0;m<4;++m){g.manager_options[m*5]=1;
        for(size_t mode=0;mode<2;++mode)for(size_t f=0;f<4;++f)for(size_t r=0;r<11;++r){const auto at=m*0x160+mode*0xb0+f*22+r;g.formations[at]=depth[r];g.formations[at+11]=lane[r];}}
    return g;
}
CareerSnapshot fixture(Country country,bool human,const GameState& base) {
    CareerSnapshot s;s.world=bmh::test::world12::complete_world();s.world.players_initialized=true;
    uint32_t next=1000000;
    for(auto& [id,club]:s.world.clubs)for(size_t row=0;row<20;++row) {
        if(club.roster[row].empty()){PlayerRecord p;p.id={next++};p.attributes.name="Reserve "+std::to_string(p.id.value);s.world.players[p.id]=p;club.roster[row]={p.id,uint16_t(row<17?2:0)};}
        auto& player=s.world.players.at(club.roster[row].player).attributes;
        player.w35c=60u|(75u<<7)|(85u<<14);player.w368=uint32_t(country)<<27;
        player.w364=uint32_t(row==0||row==11?0:row<5?1:row<9?2:3);
    }
    if(human){const auto league=s.world.catalogue.starting_league(country,0);const auto club=s.world.membership[league.value].clubs[0];
        s.world.managers={{3,"Manager",club,league,3}};WatcomRandom random(91);
        initialize_manager_contracts(s.world,3,base,{3,false,false,false,6},random);}
    s.date=17;s.results=SeasonResults::empty(s.world.catalogue);s.validate();return s;
}
AiLeagueConditions conditions(uint32_t date) {
    AiLeagueConditions c;c.date=date;c.month=7;c.temperature=20;c.rain=0;c.spectators=20000;
    c.referee_properties={24};c.presentation_buttons.assign(0xb4,1);return c;
}
void finals() {
    const auto base=rules();HumanRoundHooks hooks;hooks.decline_unpresented_events=true;
    for(auto country:{Country::germany,Country::england})for(bool human:{false,true})for(uint32_t seed:{7u,91u,713u}) {
        auto state=fixture(country,human,base);const auto source=bmh::test::world12::country_rules(country==Country::england);
        const auto a=state.world.membership[state.world.catalogue.starting_league(country,0).value].clubs[0];
        const auto b=state.world.membership[state.world.catalogue.starting_league(country,1).value].clubs[0];
        const auto cup=country==Country::germany?DomesticCup::german_supercup:DomesticCup::english_supercup;
        const auto stage=KnockoutStage::from_source(cup,source,23,{{a,b}},state.world);state.cups={stage};
        const auto before=state;auto repeat=state;GameRandom game(31),again_game(31);WatcomRandom random(seed),again_random(seed);
        const auto result=simulate_and_settle_domestic_cup_round(state,base,stage,source,{conditions(17)},{3,6,false,true},game,random,hooks);
        const auto repeated=simulate_and_settle_domestic_cup_round(repeat,base,stage,source,{conditions(17)},{3,6,false,true},again_game,again_random,hooks);
        check(state==repeat&&result==repeated&&game.seed()==again_game.seed()&&random.seed()==again_random.seed(),"cup simulation changed across identical replay");
        auto watched=before;auto observation=conditions(17);observation.pools_selected=true;GameRandom watch_game(31);WatcomRandom watch_random(seed);
        const auto receipt=simulate_and_settle_domestic_cup_round(watched,base,stage,source,{observation},{3,6,false,true},watch_game,watch_random,hooks);
        check(watched==state&&receipt.scores==result.scores&&receipt.end_minutes==result.end_minutes&&watch_game.seed()==game.seed()&&watch_random.seed()==random.seed()&&receipt.pools.size()==1&&receipt.pools[0].fixture==stage.rounds.back().fixtures[0],"Toto observation changed cup scoring, state or RNG");
        if(receipt.end_minutes[0]>94)check(receipt.pools[0].outcome==0,"Toto used extra-time/shootout winner instead of the regulation draw");
        check(result.scores.size()==1&&result.end_minutes[0]>=90&&result.end_minutes[0]<=124&&result.game_draws&&result.watcom_draws,
              "cup did not run its actual scoring phases");
        check(state.cups==before.cups&&state.date==before.date&&state.league_rounds==before.league_rounds,
              "cup calculation advanced an unowned stage/calendar/league");
        auto completed=stage;commit_cup_leg(completed,{cup,23,0,result.scores,{},{}},state.world);
        check(completed.complete()&&completed.qualifiers().size()==1,"actual cup result did not resolve a winner");
        check(read_save(make_save(state))==state,"cup player/manager effects lost in save/load");
        auto invalid=before;auto bad=conditions(18);const auto stable=invalid;GameRandom g(31);WatcomRandom r(seed);
        rejects([&]{simulate_and_settle_domestic_cup_round(invalid,base,stage,source,{bad},{3,6,false,true},g,r,hooks);},"stale cup conditions accepted");
        check(invalid==stable&&g.calls()==0&&r.calls()==0,"rejected cup request consumed state/randomness");
    }
}
void opening_rounds() {
    const auto base=rules();HumanRoundHooks hooks;hooks.decline_unpresented_events=true;
    for(const auto country:{Country::germany,Country::england})for(bool human:{false,true}) {
        auto state=fixture(country,human,base);const auto source=bmh::test::world12::country_rules(country==Country::england);
        const size_t count=country==Country::germany?64:48;
        std::vector<ClubId> clubs;
        for(auto league:state.world.catalogue.simulation_leagues(country==Country::germany?TableGroup::germany:TableGroup::england))
            if(country!=Country::england||league.value!=5)
            for(auto club:state.world.membership[league.value].clubs)if(clubs.size()<count)clubs.push_back(club);
        std::vector<bmh::world12::Fixture> fixtures;
        for(size_t n=0;n<count;n+=2)fixtures.push_back({clubs[n],clubs[n+1]});
        std::vector<ClubId> seeds;
        if(country==Country::england)seeds=state.world.membership[5].clubs;
        const auto cup=country==Country::germany?DomesticCup::german_cup:DomesticCup::fa_cup;
        const auto stage=KnockoutStage::from_source(cup,source,0,fixtures,state.world,seeds);state.cups={stage};
        GameRandom game(31);WatcomRandom random(713);
        const auto result=simulate_and_settle_domestic_cup_round(state,base,stage,source,
            std::vector<AiLeagueConditions>(fixtures.size(),conditions(17)),{3,6,false,true},game,random,hooks);
        check(result.scores.size()==count/2&&result.end_minutes.size()==count/2,
              "full domestic opening round lost its later fixtures");
        for(auto minute:result.end_minutes)check(minute>=90&&minute<=124,"full opening round left an unfinished match");
        auto completed=stage;const auto receipt=draw_after_cup_leg(state.world,stage,result.scores,source,random);
        commit_cup_leg(completed,receipt,state.world);state.cups.front()=completed;state.watcom_seed=random.seed();
        check(completed.rounds.front().first_leg==result.scores&&read_save(make_save(state))==state,
              "full domestic opening round lost score or player identity on save");
    }
}
void return_leg() {
    const auto base=rules();auto source=bmh::test::world12::country_rules(true);const auto at=cup_entry_address(26);
    const uint8_t teams=2,rounds=1,next=0,flags=29,previous=255;
    source.patch(at+0x1a,&teams,1);source.patch(at+0x1b,&rounds,1);source.patch(at+0x16,&next,1);
    source.patch(at+0x14,&flags,1);source.patch(at+0x15,&previous,1);
    auto state=fixture(Country::england,true,base);
    const auto a=state.world.membership[5].clubs[0],b=state.world.membership[6].clubs[0];
    state.cups={KnockoutStage::from_source(DomesticCup::league_cup,source,26,{{a,b}},state.world)};
    HumanRoundHooks hooks;hooks.decline_unpresented_events=true;GameRandom game(71);WatcomRandom random(99);
    auto stage=state.cups.front();
    const auto first=simulate_and_settle_domestic_cup_round(state,base,stage,source,{conditions(17)},{3,6,false,true},game,random,hooks);
    check(first.end_minutes[0]<=94,"first leg incorrectly played extra time");
    commit_cup_leg(state.cups.front(),{DomesticCup::league_cup,26,0,first.scores,{},{}},state.world);
    stage=state.cups.front();check(stage.next_leg()==2&&stage.next_fixtures()==std::vector<bmh::world12::Fixture>{{b,a}},"return leg lost its home/away order");
    state.date=24;
    const auto second=simulate_and_settle_domestic_cup_round(state,base,stage,source,{conditions(24)},{3,6,false,true},game,random,hooks);
    commit_cup_leg(state.cups.front(),{DomesticCup::league_cup,26,stage.revision,second.scores,{},{}},state.world);
    check(state.cups.front().complete()&&state.cups.front().qualifiers().size()==1,"actual two-leg aggregate did not resolve");
}
void complete_country_day() {
    namespace app=bmh::app::world12;
    const auto base=rules();auto state=fixture(Country::germany,true,base);
    const auto de=bmh::test::world12::country_rules(false),en=bmh::test::world12::country_rules(true);
    for(auto country:{Country::germany,Country::england}) {
        const auto league=state.world.catalogue.starting_league(country,0);
        const auto& members=state.world.membership[league.value].clubs;
        state.cups.push_back(KnockoutStage::from_source(country==Country::germany?DomesticCup::german_supercup:DomesticCup::english_supercup,
            country==Country::germany?de:en,23,{{members[0],members[1]}},state.world));
    }
    SeasonCalendar schedule;
    for(const auto& league:state.world.catalogue.leagues)for(uint8_t round=1;round<=league.rounds();++round)
        schedule.leagues.push_back({uint16_t(50+round),league.id,round});
    schedule.others={{17,CalendarDomain::germany,0x2000},{17,CalendarDomain::england,0x2000}};
    state.calendar=CalendarCursor{schedule,17,7};state.validate();const auto ticket=*prepare_day(state);
    std::vector<app::CupDayConditions> inputs;
    for(const auto& cup:ticket.cups)inputs.push_back({cup.cup,cup.source_entry,cup.revision,{conditions(17)}});
    const auto before=state;app::ManagerActionOwner owner{71,1};
    HumanRoundHooks hooks;hooks.decline_unpresented_events=true;
    auto missing=inputs;missing.pop_back();rejects([&]{app::play_domestic_cup_day(state,owner,owner,ticket,base,{de,en},missing,{3,6,false,true},hooks);},"partial country match batch accepted");
    check(state==before&&owner.revision==1,"partial country batch changed state");
    auto broken_source=en;const uint8_t wrong_size=4;broken_source.patch(cup_entry_address(23)+0x1a,&wrong_size,1);
    rejects([&]{app::play_domestic_cup_day(state,owner,owner,ticket,base,{de,broken_source},inputs,{3,6,false,true},hooks);},
            "late second-country rule mismatch accepted");
    check(state==before&&owner.revision==1,"second-country failure published first-country goals/player effects/RNG");
    auto interrupted=before;app::ManagerActionOwner external{71,1};auto observing=hooks;bool changed=false;
    observing.after_minute=[&](AiMatchProjection&,MatchEngine&,int){if(!changed){++external.revision;changed=true;}};
    rejects([&]{app::play_domestic_cup_day(interrupted,external,{71,1},ticket,base,{de,en},inputs,{3,6,false,true},observing);},
            "live callback changed the day owner without rejection");
    check(interrupted==before&&external.revision==2,"stale-owner rollback overwrote external revision or partial match effects");
    const auto outcome=app::play_domestic_cup_day(state,owner,owner,ticket,base,{de,en},inputs,{3,6,false,true},hooks);
    check(outcome.cups.size()==2&&outcome.game_draws&&outcome.watcom_draws&&owner.revision==2&&
          state.date==18&&state.calendar->next_day==18&&state.calendar->revision==8,
          "real country cup calculations did not publish exactly one calendar day");
    for(const auto& cup:state.cups)check(cup.complete()&&cup.qualifiers().size()==1,"country final remained unresolved");
    check(read_save(make_save(state))==state,"played country cups lost scores/player effects on save/load");
    const auto stable=state;rejects([&]{app::play_domestic_cup_day(state,owner,owner,ticket,base,{de,en},inputs,{3,6,false,true},hooks);},"played cup date was replayed");
    check(state==stable&&owner.revision==2,"replayed cup date changed state");
}
}
int main()try {
    set_fixture_tables(bmh::test::authored_fixture_tables());finals();opening_rounds();return_leg();complete_country_day();
    std::cout<<"Actual German/English cup phases, cross-division finals, human/AI state, replay, return-leg aggregate and rejected-owner rollback passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
