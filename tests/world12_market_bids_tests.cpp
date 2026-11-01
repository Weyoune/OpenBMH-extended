// SPDX-License-Identifier: GPL-3.0-only
#include "world12_fixture.hpp"
#include "game/world12/market_bids.hpp"
#include "game/world12/economy.hpp"
#include "game/world12/manager_tactics.hpp"
#include "game/world12/codec.hpp"
#include "game/messages.hpp"
#include "game/transfer_bid.hpp"
#include <algorithm>
#include <iostream>
namespace {
using namespace bmh;using namespace bmh::world12;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class F>void rejects(F f,const char* message){bool bad=false;try{f();}catch(const std::exception&){bad=true;}check(bad,message);}
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
CareerSnapshot prepared(GameState& rules) {
    rules.memory.add_block(layout::Savestate_Layout_ExecutableField,1);rules.memory.set8(layout::Savestate_Layout_ExecutableField,2);
    rules.memory.add_block(layout::Loans,4*0x78);rules.mailbox.assign(kMailboxEntries*kMailboxEntry,0);
    auto state=fixture(rules);state.date=0;state.world.settings->mode=3;state.world.transfer_market.emplace();
    for(auto& m:state.world.managers){auto& c=m.career.emplace();c.season_records.emplace();c.season_services.emplace();
        c.stadium.facility_values.fill(4);c.stadium.facility_values[0]=85;c.stadium.stand_condition.fill(100);
        c.stadium.facility_levels.fill(1);c.stadium.stand_capacity.fill(1);c.stadium.capacity_sum=8;
        c.sponsors[0]={100000,12};
    }
    auto& fields=state.world.initial_competitions.emplace();fields.manager_services_complete=true;
    std::vector<ClubId> clubs;for(const auto& [id,club]:state.world.clubs)clubs.push_back(id);
    size_t at=0;const size_t sizes[]={64,24,32};for(size_t k=0;k<3;++k){for(size_t n=0;n<sizes[k];++n)fields.fields[k].push_back(clubs.at(at++));fields.holders[k]=fields.fields[k][0];}
    for(uint32_t n=0;n<4;++n){ClubRecord nation;nation.id={0xe0000000u+n};nation.name="Nation "+std::to_string(n);nation.kind=ClubKind::national_team;nation.crest=state.world.clubs.begin()->second.crest;state.world.clubs[nation.id]=nation;fields.national_field.push_back(nation.id);}
    initialize_career_economy(state,rules);
    for(size_t f=0;f<6;++f){const size_t at=f*0x1c;const uint32_t price=1000000;
        for(size_t b=0;b<4;++b)state.world.markets->funds[at+b]=uint8_t(price>>(b*8));
        for(size_t m=0;m<4;++m)state.world.markets->funds[at+0x14+m*2]=2;
    }
    state.date=40;state.validate();return state;
}CareerSnapshot* interrupted=nullptr;
void interrupt(int){if(interrupted){++interrupted->watcom_seed;interrupted=nullptr;}}
}
int main()try {
    auto rules=reference();auto state=prepared(rules);
    const auto seller=state.world.managers[0].club;
    const auto player=state.world.clubs.at(seller).roster[19].player;
    check(player.value>0x7fffffffu&&state.world.players.size()>4095,"market fixture lacks wide player IDs/large world");
    const auto target=state.world.membership[12].clubs.back();uint32_t extra=0xb0000000u;
    for(auto& [id,club]:state.world.clubs)if(club.league.valid()&&id!=target&&std::none_of(state.world.managers.begin(),state.world.managers.end(),[&](const auto& m){return m.club==id;})) {
        for(size_t row=20;row<22;++row){auto p=state.world.players.at(club.roster[1].player);p.id={extra++};p.attributes.name="Surplus "+std::to_string(p.id.value);state.world.players.emplace(p.id,p);club.roster[row]={p.id,0};}
    }
    for(size_t row=15;row<24;++row)state.world.clubs.at(target).roster[row]={};
    auto& person=state.world.players.at(player);person.attributes.w368=(person.attributes.w368&0x07ffffffu)|(4u<<27);person.bonus&=127;
    MarketListing listing;listing.player=player;listing.seller=seller;listing.human_seller=true;listing.fee=1000000;listing.salary=10000;listing.asking_percent=100;listing.demand_percent=100;listing.weeks=4;
    state.world.transfer_market->rows[0]=listing;const auto initial=state;
    const auto bid=add_computer_market_bid(state,0,rules,{true});
    check(bid.added==1&&!bid.game_draws&&bid.watcom_draws>0&&state.world.transfer_market->rows[0]->bids[0].bidder==target,"global bid omitted the only eligible club at end of complete world");
    check(state.world.clubs==initial.world.clubs&&state.world.players==initial.world.players&&state.date==initial.date,"bid generation moved players or calendar");
    check(read_save(make_save(state))==state,"global bid failed snapshot roundtrip");
    const auto accepted=state;const auto duplicate=add_computer_market_bid(state,0,rules,{true});
    check(!duplicate.added&&state.world.transfer_market->rows[0]->bids==accepted.world.transfer_market->rows[0]->bids,"existing AI buyer was duplicated");
    auto full=initial;for(auto& b:full.world.transfer_market->rows[0]->bids)b.bidder=target;
    const auto before_full=full;const auto none=add_computer_market_bid(full,0,rules,{true});check(!none.added&&!none.watcom_draws&&full==before_full,"full row consumed random numbers or changed bids");

    auto free=initial;auto record=free.world.players.at(player);record.id={0xf0012345u};record.attributes.name="Free agent";free.world.players.emplace(record.id,record);free.world.free_players->domestic[0]=record.id;
    auto& free_listing=*free.world.transfer_market->rows[0];free_listing.player=record.id;free_listing.seller={};free_listing.human_seller=false;
    add_computer_market_bid(free,0,rules,{true});check(free.world.transfer_market->rows[0]->bids[0].bidder==target&&!(free.world.transfer_market->rows[0]->bids[0].months&128),"free agent received a loan or lost global bidder");

    auto reserved=initial;const auto borrower=reserved.world.membership[11].clubs[0];const auto borrowed=reserved.world.clubs.at(borrower).roster[0].player;
    reserved.world.loans.push_back({borrowed,target,borrower,12});const auto blocked=add_computer_market_bid(reserved,0,rules,{true});
    check(!blocked.added,"reserved returning player did not count against bidder capacity");
    auto shortage=initial;for(size_t row=11;row<24;++row)shortage.world.clubs.at(target).roster[row]={};
    shortage.world.transfer_market->last_entry40_bidders[0]=target;
    check(read_save(make_save(shortage))==shortage,"retained special bidder references were lost on save");
    auto unknown=shortage;unknown.world.transfer_market->last_entry40_bidders={};unknown.world.transfer_market->entry40_history_known=false;const auto unknown_before=unknown;
    rejects([&]{add_computer_market_bid(unknown,0,rules,{true});},"missing historic entry40 identities were guessed");check(unknown==unknown_before,"unknown-history rejection leaked RNG/bids");
    add_computer_market_bid(shortage,0,rules,{true});check(shortage.world.transfer_market->rows[0]->bids[0].bidder==target,"retained bidder history broke shortage scoring");

    auto empty=initial;empty.world.transfer_market->rows[0].reset();WatcomRandom expected(empty.watcom_seed);for(int n=0;n<40;++n)expected.range(0,1);
    const auto weekly=generate_weekly_market_bids(empty,rules,{true});check(!weekly.added&&weekly.watcom_draws==40&&empty.watcom_seed==expected.seed(),"empty weekly bid phase lost forty Source gates");
    auto replay=initial;generate_weekly_market_bids(replay,rules,{true});auto again=initial;generate_weekly_market_bids(again,rules,{true});check(replay==again,"weekly bidding is nondeterministic");
    auto changed=initial;interrupted=&changed;draw_hook=interrupt;
    rejects([&]{add_computer_market_bid(changed,0,rules,{true});},"market bidding overwrote external state");draw_hook=nullptr;interrupted=nullptr;
    auto externally_changed=initial;++externally_changed.watcom_seed;check(changed==externally_changed,"market rollback lost external change or leaked bid");
    const auto snapshot=initial;rejects([&]{add_computer_market_bid(changed,40,rules,{true});},"special row admitted as regular auction");
    check(transfer_recruitment_need(0,19,20,90)==51&&transfer_recruitment_need(0,19,20,15)==0,"late recruitment targets differ from supplied Core tables");
    std::cout<<"Global last-club bidder, >4095/wide IDs, free-agent terms, duplicate/full rows, reserved loans, retained history, Source weekly gates and rollback passed\n";
}catch(const std::exception& error){draw_hook=nullptr;std::cerr<<error.what()<<'\n';return 1;}
