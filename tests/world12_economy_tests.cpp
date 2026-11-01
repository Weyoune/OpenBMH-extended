// SPDX-License-Identifier: GPL-3.0-only
#include "world12_fixture.hpp"
#include "game/world12/economy.hpp"
#include "game/world12/manager_tactics.hpp"
#include "game/world12/codec.hpp"
#include "game/messages.hpp"
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
CareerSnapshot* interrupted=nullptr;
void interrupt(int){if(interrupted){++interrupted->watcom_seed;interrupted=nullptr;}}
}
int main()try {
    auto rules=reference();rules.memory.add_block(layout::Savestate_Layout_ExecutableField,1);
    rules.memory.add_block(layout::Loans,4*0x78);rules.mailbox.assign(kMailboxEntries*kMailboxEntry,0);
    auto state=fixture(rules);state.date=0;state.world.settings->difficulty=3;
    initialize_career_economy(state,rules);state.date=40;
    auto& economy=*state.world.economy;auto& de=economy.managers[1];auto& en=economy.managers[3];
    de.deposit=2000000;de.interest_income=123;de.overdraft_interest=456;
    de.loans[4]={100,120,12,10,2,24};de.loans[3]={200,240,24,10,2,48};
    economy.managers[0].deposit=77;
    state.world.managers[0].finance->balance=1000000;state.world.managers[0].finance->income=0;
    state.world.managers[1].finance->balance=-300000;state.world.managers[1].finance->credit=100000;en.overdraft_interest=100;
    auto& stocks=state.world.markets->stocks;auto& funds=state.world.markets->funds;
    for(size_t share=0;share<12;++share){const auto at=share*0x5c;uint32_t capital=300000;
        for(size_t n=0;n<4;++n)stocks[at+n]=uint8_t(capital>>(8*n));stocks[at+0x4e]=100;stocks[at+0x58]=1;stocks[at+0x59]=3;
        for(size_t seat=0;seat<4;++seat)stocks[at+0x50+seat*2]=uint8_t(seat+1);
    }
    for(size_t f=0;f<6;++f){const size_t at=f*0x1c;funds[at]=0xe8;funds[at+1]=3;for(size_t seat=0;seat<4;++seat)funds[at+0x14+seat*2]=uint8_t(seat+1);}
    EconomyMessage note;note.recipients=1u<<3;note.read_by=1u<<3;note.date=12;note.text[0]='A';economy.messages.push_back(note);economy.recipient_mask=1u<<3;
    const auto initial=state;state.validate();
    auto projection=AiMatchProjection::create_for_humans(state,{state.world.managers[0].club,state.world.managers[1].club},rules,{1,3,false,true});
    install_career_economy(projection,state.world);
    check(projection.state.memory.u32(layout::Loans+2*0x18)==100,"loan lender seat3 was not mapped to local seat1");
    check(projection.state.mailbox[0]==2&&projection.state.mailbox[4]==2,"mail recipients/read flags lost sparse manager identity");
    capture_career_economy(state.world,projection);check(state==initial,"economy projection roundtrip changed global loan/holding/mail identity");
    check(read_save(make_save(state))==state,"economy did not survive snapshot JSON16 roundtrip");

    const auto daily=daily_finance(state,rules,DailyEconomyKind::plain_day);
    check(state.world.managers[0].finance->balance==1000136&&state.world.economy->managers[1].interest_income==259,"daily deposit/account interest is wrong");
    check(state.world.managers[1].finance->balance==-300130&&state.world.economy->managers[3].overdraft_interest==230,"daily overdraft tiers are wrong");
    check(daily.game_draws>0&&!daily.watcom_draws&&state.date==40,"daily finance skipped shares or advanced calendar");
    check(state.world.economy->managers[0]==initial.world.economy->managers[0],"daily finance invented processing for absent manager0");
    for(size_t share=0;share<12;++share)for(size_t seat=0;seat<4;++seat)
        check(state.world.markets->stocks[share*0x5c+0x50+seat*2]==seat+1,"daily stock update moved another manager's holdings");
    auto event=initial;const auto event_phase=daily_finance(event,rules,DailyEconomyKind::after_event);
    check(!event_phase.game_draws&&event.world.markets==initial.world.markets,"post-event interest moved stock prices a second time");
    auto& bank=state.world.economy->managers[1];bank.camp_weeks=1;bank.camp_bill=100000;bank.offers[2]=3;bank.weekly_count=52;
    for(size_t n=0;n<52;++n)bank.financial_history[0][n]=uint32_t(n+100);
    const auto before=state;const auto settled=settle_weekly_managers(state,rules);
    check(settled.managers==2&&settled.watcom_draws>=2&&state.world.economy->managers[1].camp_weeks==0&&state.world.economy->managers[1].camp_bill==0,"weekly camp settlement was not retained");
    check(state.world.economy->managers[1].weekly_count==52&&state.world.economy->managers[1].financial_history[0][0]==101,"rolling weekly history lost its shift");
    check(state.world.economy->managers[3].weekly_count==1,"second sparse manager's weekly history was skipped");
    check(state.world.economy->managers[1].financial_history[2][51]==uint32_t(state.world.managers[0].finance->balance),"weekly history records a stale balance");
    check(state.world.economy->managers[1].loans==before.world.economy->managers[1].loans,"weekly statistics changed a loan contract");
    check(read_save(make_save(state))==state,"weekly bank/history state failed snapshot roundtrip");

    CareerEconomy mailbox;
    for(uint16_t n=0;n<45;++n){EconomyMessage m;m.recipients=1u<<3;m.date=n;m.text[0]=uint8_t(n);append_economy_message(mailbox,m);}
    check(mailbox.messages.size()==40&&mailbox.messages.back().date==44&&mailbox.messages.back().recipients==(1u<<3)&&mailbox.recipient_mask==(1u<<3),"persistent mailbox lost capacity/order/global recipients");
    state.world.economy->messages=mailbox.messages;state.world.economy->recipient_mask=mailbox.recipient_mask;
    check(read_save(make_save(state))==state,"full persistent mailbox failed snapshot roundtrip");
    auto changed=initial;interrupted=&changed;draw_hook=interrupt;
    rejects([&]{daily_finance(changed,rules,DailyEconomyKind::plain_day);},"daily finance overwrote external state");draw_hook=nullptr;interrupted=nullptr;
    auto expected=initial;++expected.watcom_seed;check(changed==expected,"daily rollback leaked financial/market changes");
    auto missing=initial;missing.world.economy.reset();const auto untouched=missing;
    rejects([&]{daily_finance(missing,rules,DailyEconomyKind::plain_day);},"missing economy was silently initialized");check(missing==untouched,"invalid phase changed state");
    auto invalid=state;invalid.world.economy->messages[0].recipients=1u<<8;rejects([&]{make_save(invalid);},"unknown mailbox manager accepted");
    invalid=state;invalid.world.economy->managers[1].weekly_count=53;rejects([&]{make_save(invalid);},"oversized weekly history accepted");
    auto raw=encode_world(initial.world);std::string legacy(raw.begin(),raw.end());const auto version=legacy.find("\"version\":17");check(version!=std::string::npos,"new economy schema version missing");legacy.replace(version,12,"\"version\":15");
    auto legacy_world=initial.world;legacy_world.economy.reset();check(decode_world(Bytes(legacy.begin(),legacy.end()))==legacy_world,"previous world schema15 ceased to load");
    std::cout<<"Daily interest/shares, sparse global loan/holding/mail maps, weekly camp/history, persistent mailbox, schemas15/17 and rollback passed\n";
}catch(const std::exception& error){draw_hook=nullptr;std::cerr<<error.what()<<'\n';return 1;}
