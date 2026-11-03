// SPDX-License-Identifier: GPL-3.0-only
#include "world12_fixture.hpp"
#include "game/world12/manager_join.hpp"
#include "game/world12/manager_tactics.hpp"
#include "game/world12/codec.hpp"
#include "game/career.hpp"
#include <iostream>
#include <set>
using namespace bmh;using namespace bmh::world12;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class F>void rejects(F f){bool caught=false;try{f();}catch(...){caught=true;}check(caught,"invalid running takeover accepted");}
GameState reference(){
    GameState g;g.club_ids.resize(445);g.team_flags.resize(445);g.team_morale.resize(445);g.formations.resize(0x580);g.tactics.resize(4*0x840);g.manager_records.resize(5*0xb0c);g.manager_options.resize(20);g.manager_ratings.resize(240);
    const uint8_t x[]={0,5,5,5,5,10,10,10,10,17,17},y[]={4,1,3,5,7,1,3,5,7,3,5};
    for(size_t m=0;m<4;++m){g.manager_options[m*5]=1;for(size_t mode=0;mode<2;++mode)for(size_t f=0;f<8;++f)for(size_t r=0;r<11;++r){const auto at=m*0x160+mode*0xb0+f*22+r;g.formations[at]=x[r];g.formations[at+11]=y[r];}}
    g.memory.add_block(layout::EnvironmentYouthTeams,4*0x24);g.memory.add_block(layout::Desktops,4*0x360);g.memory.add_block(layout::Loans,4*0x78);return g;
}
LeImage rules(){auto r=LeImage::from_bytes(layout::GermanDataSegmentBegin,Bytes(0x6000));const auto set=[&](uint32_t a,uint8_t v){r.patch(a,&v,1);};for(uint32_t level=0;level<4;++level){set(layout::Career_BuildStadium_RuntimeField4+level,uint8_t(level+5));for(uint32_t i=0;i<22;++i)set(layout::Career_BuildStadium_RuntimeField+level*0x17+i,1);}for(uint32_t i=0;i<22;++i){set(layout::Career_BuildStadium_RuntimeField2+i*5,2);set(layout::Career_BuildStadium_RuntimeField3+i*5,5);}return r;}
CareerSnapshot base(const GameState& ref,const LeImage& source){
    CareerSnapshot s;s.world=bmh::test::world12::complete_world();s.world.players_initialized=true;s.world.markets.emplace();s.world.free_players.emplace();
    InitialStrengthRanges strength;for(auto& range:strength)range={20,70};GameRandom game(17);WatcomRandom random(91);const auto club=s.world.membership[0].clubs[0];
    add_initial_manager(s.world,{0,club,{0},"Host",0,1},{3,2,5,1,false},strength,ref,source,game,random);
    s.world.economy.emplace();s.world.managers[0].career->season_services.emplace();s.results=SeasonResults::empty(s.world.catalogue);SeasonCalendar calendar;calendar.days=366;
    for(const auto& league:s.world.catalogue.leagues)for(uint8_t round=1;round<=league.rounds();++round)calendar.leagues.push_back({uint16_t(10+round*7),league.id,round});
    s.calendar=CalendarCursor{calendar,0,0};s.desktop.emplace();s.desktop->active_seat=0;s.desktop->manager_order={0};s.desktop->matchday_number=17;s.desktop->initial_event=InitialDeskEvent{17,0,{0},true,{}};s.desktop->managers.push_back({0});s.transfers.emplace();s.game_seed=game.seed();s.watcom_seed=random.seed();s.validate();return s;
}
}
int main()try{
    const auto ref=reference();const auto source=rules();size_t cases=0;
    for(uint32_t target:{0u,1u,2u,3u,4u,5u,6u,7u,8u}){
        auto state=base(ref,source);state.world.settings->planned_managers=2;state.world.economy->managers[1].balance_plan.emplace();
        const auto club=state.world.membership[target].clubs[3];state.world.clubs.at(club).season_stats.counters[0]=4;
        const auto before=state;const auto players=state.world.players;const auto owner=state.world.managers[0];const auto members=state.world.membership;
        const auto outcome=add_running_manager(state,{1,club,{target},"Visitor",1,1},ref,source);
        check(outcome.seat==1&&outcome.watcom_draws>0&&state.world.managers.size()==2,"new personal initialization missing");
        check(state.world.membership==members&&state.results==before.results&&state.cups==before.cups&&state.calendar==before.calendar&&state.date==before.date&&state.game_seed==before.game_seed,"takeover changed membership, results, calendar or game RNG");
        check(state.world.managers[0]==owner,"takeover reset existing human");
        auto unchanged=state.world.players;
        for(auto& [id,p]:unchanged){check((p.cards&0xffe0)==(players.at(id).cards&0xffe0),"takeover reset disciplinary cards");p.cards=players.at(id).cards;}
        check(unchanged==players,"takeover reset player ages/ratings/statistics");
        check(state.desktop->active_seat==0&&state.desktop->initial_event->manager_position==0&&state.desktop->manager_order==std::vector<uint8_t>({0,1})&&state.desktop->initial_event->manager_order==std::vector<uint8_t>({0,1}),"takeover lost real desktop order/host");
        check(state.world.managers.back().career->season_services&&state.world.managers.back().career->first_recorded_season==0,"new manager attributed unknown old service history");
        check(read_save(make_save(state))==state&&decode_world(encode_world(state.world))==state.world,"running manager lost strict saved identity");
        const auto accepted=state;rejects([&]{add_running_manager(state,{1,club,{target},"Replay",2,1},ref,source);});check(state==accepted,"replayed join changed world");++cases;
    }
    {
        auto state=base(ref,source);state.world.settings->planned_managers=2;
        const auto club=state.world.membership[5].clubs[3],borrower=state.world.membership[6].clubs[2];
        std::swap(state.world.clubs.at(club).roster[10],state.world.clubs.at(borrower).roster[10]);
        const auto player=state.world.clubs.at(borrower).roster[10].player;
        state.world.loans.push_back({player,club,borrower,4,uint16_t(150)});state.validate();const auto ledger=state.world.loans;
        add_running_manager(state,{1,club,{5},"Loan Owner",1,1},ref,source);
        check(state.world.loans==ledger&&state.transfers->loans.size()==1&&state.transfers->loans[0].player==player&&state.transfers->loans[0].manager==1&&state.transfers->loans[0].source_slot==0,"takeover lost a live AI-owner loan or confused ledger and board slots");
        check(read_save(make_save(state))==state,"live owner loan did not roundtrip");
    }
    auto state=base(ref,source);state.world.settings->planned_managers=2;const auto before=state;const auto hidden=state.world.membership[9].clubs[0];rejects([&]{add_running_manager(state,{1,hidden,{9},"Hidden",1,1},ref,source);});check(state==before,"hidden league rejection changed state");
    std::cout<<cases<<" actual-tier Source running takeovers preserve player ages/ratings, memberships, identities, prior records, host/order and strict saves; hidden/replay guards passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
