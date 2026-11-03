// SPDX-License-Identifier: GPL-3.0-only
#include "world12_fixture.hpp"
#include "fixture_tables.hpp"
#include "game/world12/ai_match.hpp"
#include "game/lineup.hpp"
#include "ui/lineup.hpp"
#include "ui/transfer_market.hpp"
#include "game/world12/monthly_services.hpp"
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
    auto rules=reference();auto state=prepared(rules);TextDb texts;texts.strings.resize(TEXT_COUNT,"Synthetic sponsor notice");texts.offsets.resize(TEXT_COUNT);texts.blob.assign(200,0);
    auto& first=state.world.managers[0];auto& second=state.world.managers[1];
    first.career->sponsors[0]={120000,1};second.career->sponsors[0]={240000,2};
    auto& contract=first.career->season_services->advertising_contracts[1];contract[0]=64;contract[1]=66;contract[2]=15;contract[5]=1;contract[6]=0;
    auto& renewal=second.career->season_services->advertising_contracts[1];renewal[0]=128;renewal[1]=132;renewal[2]=30;renewal[5]=1;renewal[6]=1;
    first.career->environment.monthly={100,200,300,400,500,600};second.career->environment.monthly={700,800,900,1000,1100,1200};
    state.world.economy->managers[1].monthly->expenses={15000,25000};state.world.economy->managers[3].monthly->expenses={35000,45000};
    state.world.economy->managers[1].loans[0]={100000,105000,35000,3,2,70000};
    state.world.economy->managers[3].loans[2]={50000,55000,11000,5,0,0};
    const auto initial=state;std::vector<ClubId> clubs{first.club,second.club};
    auto p=AiMatchProjection::create_for_humans(state,clubs,rules,{3,6,false,true});install_career_economy(p,state.world);p.state.texts=&texts;p.state.memory.add_block(layout::CareerDuration,1);p.state.memory.set8(layout::CareerDuration,5);
    expire_monthly_advertising(p.state);expire_monthly_sponsors(state,rules,texts);
    check(state.world.managers[0].career->sponsors[0].amount==0&&state.world.managers[1].career->sponsors[0].months==1,"monthly sponsor countdown failed");
    check(state.world.managers[1].career->season_services->advertising_contracts[1][0]==128,"renewable sponsor lost retained amount");
    for(int m=0;m<2;++m)monthly_manager_accounts(p.state,m);
    settle_monthly_accounts(state,rules);monthly_shares_and_loans(p.state,true);settle_monthly_investments(state,rules);
    auto expected=initial;p.commit_match_state(expected.world,true);capture_career_economy(expected.world,p);
    for(size_t fund=0;fund<6;++fund)for(size_t b=0;b<4;++b)expected.world.markets->funds[fund*0x1c+b]=p.state.memory.u8(layout::FundBlock+uint32_t(fund*0x1c+b));
    check(state==expected,"monthly account phases differ from shared Core");
    check(state.game_seed==initial.game_seed&&state.watcom_seed==initial.watcom_seed,"monthly account services consumed random draws");
    check(state.world.economy->managers[1].loans[0]==std::array<uint32_t,6>{},"mature bank credit was not closed");
    check(state.world.economy->managers[3].loans[2][4]==1&&state.world.economy->managers[3].loans[2][5]==11000,"global-seat human credit was not settled");
    for(auto seat:{1,3})check(state.world.economy->managers[seat].interest_income>0,"share dividend was not booked to actual holder");
    check(state.world.economy->managers[0]==initial.world.economy->managers[0]&&state.world.economy->managers[2]==initial.world.economy->managers[2],"monthly finance changed inactive seats");
    check(read_save(make_save(state))==state,"monthly accounting snapshot failed");
    auto old=initial;old.world.economy->managers[3].monthly.reset();const auto before=old;rejects([&]{settle_monthly_accounts(old,rules);},"missing old monthly state was invented");check(old==before,"missing monthly state leaked changes");
    auto simple=initial;simple.world.settings->mode=1;const auto contracts=simple.world.managers;expire_monthly_sponsors(simple,rules,texts);check(simple.world.managers==contracts,"mode1 expired fixed sponsors");
    {
        auto month=initial;const auto owner=month.world.managers[1].club;const auto player=month.world.clubs.at(owner).roster[19].player;
        month.world.managers[1].tactics->backing->scalar_rows[19][0xc]=1;
        month.world.players.at(player).attributes.w364=(month.world.players.at(player).attributes.w364&~(31u<<21))|(10u<<21)|3u;
        const auto before=month;MonthlyContractHooks absent;
        rejects([&]{settle_monthly_roster(month,3,rules,texts,absent);},"expired contract did not require renewal owner");check(month==before,"missing contract screen leaked billing/random state");
        MonthlyContractHooks decline;decline.renew=[](GameState&,GameRandom&,WatcomRandom&,uint8_t actor,PlayerId,int,const AiMatchProjection&)->std::optional<bool>{check(actor==3,"contract dialog used wrong global manager");return false;};
        const auto roster=settle_monthly_roster(month,3,rules,texts,decline);check(roster.released==1&&month.world.free_players->domestic[0]==player,"expired domestic contract did not release into physical pool");
        check(std::none_of(month.world.clubs.at(owner).roster.begin(),month.world.clubs.at(owner).roster.end(),[&](const auto& slot){return slot.player==player;}),"released player stayed in squad");
        check(read_save(make_save(month))==month,"released monthly roster snapshot failed");
        auto cancelled=before;decline.renew=[](GameState&,GameRandom&,WatcomRandom&,uint8_t,PlayerId,int,const AiMatchProjection&)->std::optional<bool>{return {};};rejects([&]{settle_monthly_roster(cancelled,3,rules,texts,decline);},"cancelled monthly contract committed");check(cancelled==before,"cancelled contract leaked monthly effects");
        auto fake=before;decline.renew=[](GameState&,GameRandom&,WatcomRandom&,uint8_t,PlayerId,int,const AiMatchProjection&)->std::optional<bool>{return true;};rejects([&]{settle_monthly_roster(fake,3,rules,texts,decline);},"renewal ACK fabricated a contract");check(fake==before,"fake renewal leaked changes");
        auto renewed=before;decline.renew=[](GameState& g,GameRandom&,WatcomRandom&,uint8_t actor,PlayerId,int row,const AiMatchProjection& p)->std::optional<bool>{size_t m=0;while(p.global_manager(uint8_t(m))!=actor)++m;g.tactics.at(m*0x840+size_t(row)*0x58+0xc)=24;return true;};
        check(settle_monthly_roster(renewed,3,rules,texts,decline).released==0&&renewed.world.managers[1].tactics->backing->scalar_rows[19][0xc]==24,"owned contract renewal did not persist");
        auto compound=before;const auto untouched=compound;rejects([&]{process_monthly_services(compound,rules,texts,{}, {},absent);},"late monthly contract failure was ignored");check(compound==untouched,"late monthly failure did not roll back earlier sponsors/listings/billing");
        MonthlyContractHooks no;no.renew=[](GameState&,GameRandom&,WatcomRandom&,uint8_t,PlayerId,int,const AiMatchProjection&)->std::optional<bool>{return false;};
        const auto full=process_monthly_services(compound,rules,texts,{}, {},no);check(full.released==1&&full.managers==2&&compound.world.free_players->domestic[0]==player,"complete monthly service did not publish shared roster");check(read_save(make_save(compound))==compound,"complete month snapshot failed");
        auto multiple=before;multiple.world.managers[1].tactics->backing->scalar_rows[18][0xc]=1;
        const auto double_release=settle_monthly_roster(multiple,3,rules,texts,no);check(double_release.released==2&&multiple.world.clubs.at(owner).roster[18].empty(),"repeated contract release lost Source restart/board compaction");check(read_save(make_save(multiple))==multiple,"multiple monthly releases failed snapshot");
        auto overflow=before;for(uint32_t n=0;n<80;++n){auto record=overflow.world.players.at(player);record.id={0xf1000000u+n};record.attributes.name="Pool "+std::to_string(n);overflow.world.players[record.id]=record;overflow.world.free_players->domestic[n]=record.id;}
        const auto full_pool=overflow.world.free_players->domestic;const auto sent=settle_monthly_roster(overflow,3,rules,texts,no);check(sent.released==1&&overflow.world.free_players->domestic==full_pool,"full free-pool release overwrote physical entries");
        size_t owners=0;for(const auto& [id,club]:overflow.world.clubs)for(const auto& slot:club.roster)if(slot.player==player){++owners;check(id!=owner&&std::none_of(overflow.world.managers.begin(),overflow.world.managers.end(),[&](const auto& m){return m.club==id;}),"full-pool release chose a human club");}check(owners==1,"full-pool release did not choose one global AI destination");check(read_save(make_save(overflow))==overflow,"full-pool global release snapshot failed");

    }
    {
        auto empty=initial;const auto club=empty.world.managers[1].club;const auto original_players=empty.world.players.size();
        for(size_t row=0;row<20;++row){empty.world.managers[1].tactics->backing->scalar_rows[row][0xc]=1;empty.world.managers[1].tactics->backing->scalar_rows[row][0xb]&=~0x20u;auto& person=empty.world.players.at(empty.world.clubs.at(club).roster[row].player).attributes;person.w364=(person.w364&~(31u<<21))|(10u<<21);}
        for(uint32_t n=0;n<80;++n){auto person=empty.world.players.begin()->second;person.id={0xf2000000u+n};person.attributes.name="Occupied release pool";empty.world.players[person.id]=person;empty.world.free_players->domestic[n]=person.id;}
        const auto expected_players=empty.world.players.size();MonthlyContractHooks decline;decline.renew=[](auto&,auto&,auto&,auto,auto,int,const auto&)->std::optional<bool>{return false;};
        const auto outcome=settle_monthly_roster(empty,3,rules,texts,decline);check(outcome.released==20&&std::all_of(empty.world.clubs.at(club).roster.begin(),empty.world.clubs.at(club).roster.end(),[](const auto& row){return row.empty();})&&empty.world.players.size()==expected_players,"actual Source monthly releases did not preserve an empty runtime squad/identities");
        check(expected_players==original_players+80&&read_save(make_save(empty))==empty,"empty runtime board-month snapshot failed save roundtrip");
        auto projection=AiMatchProjection::create_for_humans(empty,{club},rules,{3,6,false,true});auto& local=projection.state;auto_lineup(local,0,0xe4);prepare_lineup_entry(local,0);open_lineup_screen(local,0);check(initialize_pitch_set_piece_source(local,0,0)&&normalize_pitch_set_piece_source(local,0,0),"empty runtime lineup controls were unusable");
        auto image=LeImage::from_bytes(layout::GermanDataSegmentBegin,Bytes(0x20000));for(auto pair:{std::pair{layout::TeamsWithSquads,uint16_t(runtime_dimensions.big_squad_teams)},std::pair{layout::CareerSession_DispatchLineup_RuntimeField,uint16_t(runtime_dimensions.league_teams)}}){const uint8_t word[]={uint8_t(pair.second),uint8_t(pair.second>>8)};image.patch(pair.first,word,2);}local.memory.add_block(layout::PlainBackground,1);local.memory.add_block(layout::TextGradientDisabled,1);local.memory.add_block(layout::CareerSession_AnyOf_ExecutableField,8);
        const auto lineup=ui::lineup_from_game(local,image,7);const auto market=ui::transfer_market_from_game(local,7);check(lineup.players.size()==24&&std::all_of(lineup.players.begin(),lineup.players.end(),[](const auto& row){return row.player_id<0;})&&market.roster.has_squad,"empty runtime lineup/market views invented or rejected player rows");
        set_fixture_tables(bmh::test::authored_fixture_tables());const auto league=empty.world.clubs.at(club).league;const auto fixtures=league_fixtures(empty.world,league,1);AiLeagueConditions condition;condition.date=empty.date;condition.month=8;condition.temperature=20;condition.spectators=15000;condition.referee_properties={24};condition.presentation_buttons.assign(180,1);GameRandom game(empty.game_seed);WatcomRandom random(empty.watcom_seed);HumanRoundHooks observer;observer.decline_unpresented_events=true;const auto played=simulate_and_settle_human_league_round(empty,rules,league,1,std::vector<AiLeagueConditions>(fixtures.size(),condition),{3,6,false,true},game,random,observer);
        const auto awarded=std::find_if(played.matches.begin(),played.matches.end(),[&](const auto& match){return match.fixture.home==club||match.fixture.away==club;});check(awarded!=played.matches.end()&&awarded->end_minute==0&&awarded->score.home==(awarded->fixture.home==club?0:2)&&awarded->score.away==(awarded->fixture.home==club?2:0)&&read_save(make_save(empty))==empty,"empty runtime squad did not receive actual Source zero-minute forfeit/save");
    }
    std::cout<<"Complete monthly services, contracts/retirements, full-pool global release, sparse-seat accounts/dividends/credits, snapshots and rollback passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
