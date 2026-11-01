// SPDX-License-Identifier: GPL-3.0-only
#include "world12_fixture.hpp"
#include "game/world12/daily_services.hpp"
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
    auto rules=reference();auto state=prepared(rules);state.date=0;const auto initial=state;
    TextDb texts;texts.strings.resize(TEXT_COUNT,"Synthetic day notice");texts.offsets.resize(TEXT_COUNT);texts.blob.assign(200,0);
    DailyServiceHooks service;service.auctions=hooks();service.incidents.answer=[](const GameState&,uint8_t,const WeeklyOfferQuestion&)->std::optional<bool>{return false;};service.contracts.renew=[](GameState&,GameRandom&,WatcomRandom&,uint8_t,PlayerId,int,const AiMatchProjection&)->std::optional<bool>{return false;};
    size_t weekly=0,trained=0,monthly=0;
    for(uint32_t day=1;day<=35;++day){state.date=day;auto resumed=read_save(make_save(state));
        DailyServiceContext context;context.previous_date=day-1;context.season=state.season;context.epoch=calendar_epoch();context.entry=day%2?DailyEconomyKind::after_event:DailyEconomyKind::plain_day;context.weekly_relations={day,state.season,{}};
        for(const auto& m:state.world.managers)context.weekly_relations.clubs.push_back({m.club,{}});
        const auto wd=weekday_code(day-1);const bool training=wd==2||wd==6;
        const bool month=month_code(day-1)!=month_code(day);
        const auto out=process_daily_services(state,rules,texts,context,service);weekly+=out.weekly;trained+=out.trained;monthly+=out.monthly;
        const auto replay=process_daily_services(resumed,rules,texts,context,service);
        check(state==resumed&&out.game_draws==replay.game_draws&&out.watcom_draws==replay.watcom_draws,"saved daily continuation changed state or random consumption");
        check(out.trained==training&&out.monthly==month,"daily weekday/month boundary differs");check(out.post_event_stock_pending==(day%2==1),"daily owner lost post-event market tick");
        check(read_save(make_save(state))==state,"daily service snapshot failed");
        if(out.post_event_stock_pending){const auto accounts=state.world.managers;const auto wr=state.watcom_seed;const auto tick=update_daily_stock_market(state,rules);check(tick.watcom_draws==0&&tick.game_draws>0&&state.watcom_seed==wr&&state.world.managers==accounts,"post-event stock tick repeated interest or training");}
    }
    check(weekly>=4&&trained>=9&&monthly==1,"daily fixture missed weekly/training/month transition");
    auto rejected=initial;uint32_t prior=0;while(weekday_code(prior)!=2)++prior;rejected.date=prior+1;rejected.world.managers[0].finance->balance=1000000;rejected.world.economy->managers[1].deposit=1000000;
    DailyServiceContext context;context.previous_date=prior;context.season=rejected.season;context.epoch=calendar_epoch();context.entry=DailyEconomyKind::after_event;context.weekly_relations={prior,rejected.season,{}};const auto before=rejected;
    rejects([&]{process_daily_services(rejected,rules,texts,context,service);},"stale weekly facts accepted after daily interest");check(rejected==before,"failed weekly phase leaked earlier daily finance");
    context.entry.reset();rejects([&]{process_daily_services(rejected,rules,texts,context,service);},"missing daily entry policy was inferred");check(rejected==before,"missing day owner changed state");
    context.entry=DailyEconomyKind::plain_day;context.epoch.year=0;rejects([&]{process_daily_services(rejected,rules,texts,context,service);},"unverified calendar epoch accepted");check(rejected==before,"bad epoch leaked state");
    {
        auto closing=initial;uint32_t day=330;while(day<371&&!(month_code(day)==7&&month_code(day-1)!=7))++day;check(day<371,"year-end fixture has no July boundary");closing.date=day;
        DailyServiceContext end;end.previous_date=day-1;end.season=closing.season;end.epoch=calendar_epoch();end.entry=DailyEconomyKind::after_event;end.weekly_relations={day,closing.season,{}};for(const auto& m:closing.world.managers)end.weekly_relations.clubs.push_back({m.club,{}});
        const auto out=process_daily_services(closing,rules,texts,end,service);check(out.season_end&&out.monthly&&!out.post_event_stock_pending,"season-end desk incorrectly queued the post-event stock tick");
    }
    std::cout<<"35 ordered daily service fixtures, explicit event/plain stock phases, weekly/training/month boundaries, snapshots and late rollback passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
