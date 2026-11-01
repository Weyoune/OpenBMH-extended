// SPDX-License-Identifier: GPL-3.0-only
#include "world12_fixture.hpp"
#include "app/world12_clock.hpp"
#include "game/week.hpp"
#include "game/month.hpp"
#include "game/world12/market_auctions.hpp"
#include "game/world12/economy.hpp"
#include "game/world12/manager_tactics.hpp"
#include "game/world12/codec.hpp"
#include "game/messages.hpp"
#include "game/desk_actions.hpp"
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
}MarketAuctionHooks hooks(){MarketAuctionHooks h;h.active_seat=1;h.allocator=TransferAllocatorContext::zero;h.select=[](const GameState&,const TransferOfferListView& v,const AiMatchProjection&)->std::optional<int>{return v.preferred_bid_index;};h.notice=[](const GameState&,const TransferDecisionEvent&,const AiMatchProjection&){};return h;}}
int main()try {
    namespace app=bmh::app::world12;
    auto rules=reference();rules.memory.add_block(layout::CalendarState_FormatError_RuntimeField2,1);rules.memory.add_block(layout::CalendarState_FormatError_RuntimeField,1);auto state=prepared(rules);state.date=0;
    SeasonCalendar schedule;for(const auto& league:state.world.catalogue.leagues)for(uint8_t round=1;round<=league.rounds();++round)schedule.leagues.push_back({uint16_t(50+round),league.id,round});schedule.others.push_back({35,CalendarDomain::germany,0x2000});schedule.validate(state.world.catalogue);state.calendar=CalendarCursor{schedule};
    state.desktop.emplace();for(const auto& m:state.world.managers){ManagerDesktop d;d.seat=m.seat;state.desktop->managers.push_back(d);}state.desktop->active_seat=3;
    InitialDeskEvent first;first.manager_order={1,3};first.manager_position=1;state.desktop->initial_event=first;
    MarketListing scratch;scratch.player=state.world.clubs.at(state.world.managers[1].club).roster[19].player;scratch.seller=state.world.managers[1].club;scratch.human_seller=true;scratch.weeks=1;state.world.transfer_market->rows[40]=scratch;state.world.transfer_market->entry40_history_known=true;
    const auto marked=state.world.clubs.at(state.world.managers[0].club).roster[1].player;
    state.world.managers[0].tactics->backing->marks[0]=marked;
    state.world.players.at(state.world.clubs.at(state.world.membership[0].clubs[2]).roster[0].player).attributes.w364|=0x1000u;
    state.validate();const auto initial=state;app::ManagerActionOwner owner{51,7};
    app::finish_initial_event(state,owner,owner,rules,calendar_epoch());check(state.date==1&&state.calendar->next_day==1&&state.progress&&state.progress->phase==ProgressPhase::daily_services&&!state.desktop->active_seat&&!state.desktop->initial_event,"initial event did not retain its pending daily boundary");
    check(state.game_seed==initial.game_seed&&state.watcom_seed==initial.watcom_seed&&owner.revision==8&&!state.world.transfer_market->rows[40],"event close consumed RNG, retained an unowned quote or lost revision");
    check(!state.world.managers[0].tactics->backing->marks[0].valid()&&state.world.managers[0].tactics->backing->scalar_rows[11][0x10]==0x7f,"empty match finish did not clear marking and retain lineup/bench status");
    check(!(state.world.players.at(state.world.clubs.at(state.world.membership[0].clubs[2]).roster[0].player).attributes.w364&0x1000u),"empty kickoff skipped canonical AI played flags");
    const auto pending=state;check(read_save(make_save(state))==state&&make_save(state).stream[7]=='C',"SC pending clock/order snapshot failed");
    auto truncated=make_save(state);truncated.stream.pop_back();rejects([&]{read_save(truncated);},"truncated clock accepted");
    auto invalid=state;invalid.progress->phase=ProgressPhase::scheduled_event;rejects([&]{invalid.validate();},"scheduled owner accepted without appointment");
    auto bad=state;app::ManagerActionOwner wrong{52,8};rejects([&]{app::advance_clock_step(bad,owner,wrong,rules,TextDb{},{});},"foreign progression owner accepted");check(bad==pending,"foreign progression changed state");
    TextDb texts;texts.strings.resize(TEXT_COUNT,"Synthetic clock news");texts.offsets.resize(TEXT_COUNT);texts.blob.assign(200,0);
    app::ClockHooks hooks;hooks.services.auctions=::hooks();hooks.services.auctions.active_seat.reset();hooks.services.incidents.answer=[](const GameState&,uint8_t,const WeeklyOfferQuestion&)->std::optional<bool>{return false;};hooks.services.contracts.renew=[](GameState&,GameRandom&,WatcomRandom&,uint8_t,PlayerId,int,const AiMatchProjection&)->std::optional<bool>{return false;};
    hooks.relations=[](const CareerSnapshot& current){WeeklyIncidentContext context{current.date,current.season,{}};for(const auto& m:current.world.managers)context.clubs.push_back({m.club,{}});return context;};
    auto missing=state;app::ManagerActionOwner absent{51,8};rejects([&]{app::advance_clock_step(missing,absent,absent,rules,texts,{});},"missing first weekly context was inferred");check(missing==pending&&absent.revision==8,"failed weekly clock leaked date/finance/RNG/revision");
    auto atomic=state;app::ManagerActionOwner atomic_owner=owner;const auto sought=app::seek_next_event(atomic,atomic_owner,atomic_owner,rules,texts,hooks);check(atomic.date==35&&atomic.calendar->next_day==35&&sought.phase==ProgressPhase::scheduled_event&&atomic_owner.revision==9,"event search skipped appointment or failed to reach it");
    size_t steps=0;while(state.progress->phase!=ProgressPhase::scheduled_event){
        auto resumed=read_save(make_save(state));app::ManagerActionOwner reloaded{91,0};const auto expected=app::advance_clock_step(resumed,reloaded,reloaded,rules,texts,hooks);
        const auto step=app::advance_clock_step(state,owner,owner,rules,texts,hooks);check(resumed==state&&expected.game_draws==step.game_draws&&expected.watcom_draws==step.watcom_draws,"saved phase continuation repeated or skipped a service");if(++steps>150)throw std::runtime_error("unbounded clock search");
    }
    check(state==atomic&&steps>70,"stepwise and atomic event search disagree");
    const auto ready=state;const auto revision=owner.revision;rejects([&]{app::advance_clock_step(state,owner,owner,rules,texts,hooks);},"scheduled competition was silently skipped");check(state==ready&&owner.revision==revision,"scheduled stop lost state");
    auto interrupted=pending;app::ManagerActionOwner other{51,8};auto changing=hooks;changing.relations=[&](const CareerSnapshot& current){++other.revision;return hooks.relations(current);};rejects([&]{app::seek_next_event(interrupted,other,{51,8},rules,texts,changing);},"external owner change was overwritten");check(interrupted==pending&&other.revision>8,"interrupted search leaked staged dates or rewound owner");
    auto late=pending;app::ManagerActionOwner late_owner{51,8};auto bad_week=hooks;size_t weeks=0;bad_week.relations=[&](const CareerSnapshot& current){auto facts=hooks.relations(current);if(++weeks==5)--facts.date;return facts;};rejects([&]{app::seek_next_event(late,late_owner,late_owner,rules,texts,bad_week);},"late invalid week context was ignored");check(late==pending&&late_owner.revision==8&&weeks==5,"failed event search leaked earlier weeks/quiet dates");
    auto epoch=pending;epoch.progress->epoch_year++;app::ManagerActionOwner epoch_owner{51,8};rejects([&]{app::advance_clock_step(epoch,epoch_owner,epoch_owner,rules,texts,hooks);},"saved progression used unrelated calendar rules");
    invalid=pending;invalid.progress->phase=ProgressPhase::season_end;rejects([&]{invalid.validate();},"annual owner accepted on an ordinary date");
    rejects([&]{app::seek_next_event(state,owner,owner,rules,texts,hooks);},"event search resumed past an owned scheduled event");check(state==ready&&owner.revision==revision,"ready event search advanced ownership");
    auto unfinished=initial;unfinished.desktop->initial_event->manager_position=0;unfinished.desktop->active_seat=1;app::ManagerActionOwner first_owner{51,7};const auto waiting=unfinished;rejects([&]{app::finish_initial_event(unfinished,first_owner,first_owner,rules,calendar_epoch());},"earlier manager was skipped");check(unfinished==waiting,"unfinished manager mutated clock");
    std::cout<<"Saved initial-day close, every-phase continuation, exact quiet-day service search, scheduled stops and rollback passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
