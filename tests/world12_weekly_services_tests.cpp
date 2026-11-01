// SPDX-License-Identifier: GPL-3.0-only
#include "world12_fixture.hpp"
#include "game/world12/weekly_services.hpp"
#include "game/world12/monthly_market.hpp"
#include "game/world12/manager_tactics.hpp"
#include "game/world12/codec.hpp"
#include "game/messages.hpp"
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
    auto rules=reference();const auto source=prepared(rules);TextDb texts;texts.strings.resize(TEXT_COUNT,"Synthetic news ");texts.offsets.resize(TEXT_COUNT);texts.blob.assign(200,0);
    WeeklyIncidentContext context{source.date,source.season,{}};for(const auto& m:source.world.managers)context.clubs.push_back({m.club,{}});
    WeeklyIncidentHooks incidents;incidents.answer=[](const GameState&,uint8_t,const WeeklyOfferQuestion&)->std::optional<bool>{return false;};
    MarketAuctionHooks auctions;auctions.active_seat=1;auctions.allocator=TransferAllocatorContext::zero;auctions.select=[](const GameState&,const TransferOfferListView&,const AiMatchProjection&)->std::optional<int>{return -1;};auctions.notice=[](const GameState&,const TransferDecisionEvent&,const AiMatchProjection&){};
    auto complete=source;const auto outcome=process_weekly_services(complete,rules,texts,context,incidents,auctions,{true});
    check(outcome.game_draws>0&&outcome.watcom_draws>0&&outcome.training.managers==2&&complete.world.economy->week==3,"complete weekly sequence skipped service phases");
    check(complete.date==source.date&&complete.results==source.results&&complete.calendar==source.calendar,"weekly services silently moved calendar/results");
    check(read_save(make_save(complete))==complete,"complete weekly state failed snapshot roundtrip");
    auto repeat=source;process_weekly_services(repeat,rules,texts,context,incidents,auctions,{true});check(repeat==complete,"complete weekly repeat changed RNG/state");
    auto pending=source;MarketListing listing;listing.player=pending.world.clubs.at(pending.world.managers[0].club).roster[19].player;listing.seller=pending.world.managers[0].club;listing.human_seller=true;listing.weeks=2;listing.fee=100000;listing.salary=1000;listing.demand_percent=100;listing.asking_percent=100;listing.bids[0]={pending.world.managers[1].club,1000000,100000,24,{}};pending.world.transfer_market->rows[0]=listing;
    const auto before=pending;auto missing=auctions;missing.notice={};rejects([&]{process_weekly_services(pending,rules,texts,context,incidents,missing,{true});},"failed auction acknowledgement committed earlier weekly incidents");check(pending==before,"outer weekly rollback leaked earlier phases/RNG");
    auto stale=source;interrupted=&stale;draw_hook=interrupt;rejects([&]{process_weekly_services(stale,rules,texts,context,incidents,auctions,{true});},"weekly sequence overwrote external owner update");draw_hook=nullptr;interrupted=nullptr;auto external=source;++external.watcom_seed;check(stale==external,"weekly sequence lost external seed or leaked staged changes");
    auto monthly=source;const auto seller=monthly.world.membership[0].clubs[1];const auto sample=monthly.world.clubs.at(seller).roster[19].player;
    for(uint32_t n=0;n<40;++n){auto p=monthly.world.players.at(sample);p.id={0xf0010000u+n};p.attributes.name="Free "+std::to_string(n);monthly.world.players[p.id]=p;monthly.world.free_players->domestic[n*2]=p.id;}
    const auto pool=monthly.world.free_players;const auto month_before=monthly;
    const auto renewed=renew_monthly_market(monthly,rules,texts,{});check(renewed.added>0&&renewed.watcom_draws>0&&monthly.world.free_players==pool,"monthly listing renewal moved/compacted free pools");
    check(monthly.game_seed==month_before.game_seed&&monthly.date==month_before.date&&monthly.world.clubs==month_before.world.clubs,"monthly listing renewal advanced gameplay or moved rosters");
    for(const auto& row:monthly.world.transfer_market->rows)if(row){check(row->weeks>=5&&row->weeks<=8,"monthly listing got invalid lifetime");check(std::none_of(monthly.world.managers.begin(),monthly.world.managers.end(),[&](const auto& m){return m.club==row->seller;}),"automatic monthly renewal listed a human player");}
    check(read_save(make_save(monthly))==monthly,"monthly listings/search news failed snapshot roundtrip");auto again=month_before;renew_monthly_market(again,rules,texts,{});check(again==monthly,"monthly renewal is nondeterministic");
    auto quiet=source;auto quiet_rules=rules;quiet_rules.memory.add_block(layout::Weekly_ExecuteTransfer_RuntimeField,1);quiet_rules.memory.set8(layout::Weekly_ExecuteTransfer_RuntimeField,1);quiet_rules.memory.add_block(layout::Weekly_InterestedManagers_RuntimeField,1);quiet_rules.memory.set8(layout::Weekly_InterestedManagers_RuntimeField,0);
    const auto foreign_seller=quiet.world.membership[12].clubs[0],foreign_buyer=quiet.world.membership[12].clubs[1];const auto foreign_player=quiet.world.clubs.at(foreign_seller).roster[19].player;
    quiet.world.players.at(foreign_player).attributes.w368|=4u<<27;MarketListing outside;outside.player=foreign_player;outside.seller=foreign_seller;outside.fee=100000;outside.salary=1000;outside.demand_percent=100;outside.asking_percent=100;outside.weeks=1;outside.bids[0]={foreign_buyer,1000000,100000,24,{}};quiet.world.transfer_market->rows[0]=outside;
    const auto moved=process_market_auctions(quiet,quiet_rules,texts,auctions);check(moved.moved==1&&quiet.world.economy->messages.empty(),"temporary auction club numbers leaked unrelated international news to domestic managers");
    auto wrong=month_before;MonthlyMarketPolicy invalid;invalid.national_squad.insert({0xfffffffeu});rejects([&]{renew_monthly_market(wrong,rules,texts,invalid);},"unknown national protection player accepted");check(wrong==month_before,"invalid renewal changed state");
    std::cout<<"Atomic complete weekly sequence, late-phase rollback, wide/holey monthly free pools, club listing renewal, snapshots and determinism passed\n";
}catch(const std::exception& e){draw_hook=nullptr;std::cerr<<e.what()<<'\n';return 1;}
