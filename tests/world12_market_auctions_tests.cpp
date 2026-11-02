// SPDX-License-Identifier: GPL-3.0-only
#include "world12_fixture.hpp"
#include "game/world12/market_auctions.hpp"
#include "game/world12/economy.hpp"
#include "game/world12/manager_tactics.hpp"
#include "game/world12/codec.hpp"
#include "game/messages.hpp"
#include <iostream>
#include <algorithm>
namespace {
using namespace bmh;using namespace bmh::world12;
void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}
template<class F>void rejects(F f,const char* m){bool bad=false;try{f();}catch(const std::exception&){bad=true;}check(bad,m);}
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
}MarketAuctionHooks hooks(){MarketAuctionHooks h;h.active_seat=1;h.allocator=TransferAllocatorContext::zero;h.select=[](const GameState&,const TransferOfferListView& v,const AiMatchProjection&)->std::optional<int>{return v.preferred_bid_index;};h.notice=[](const GameState&,const TransferDecisionEvent&,const AiMatchProjection&){};return h;}
}
int main()try {
    auto rules=reference();auto source=prepared(rules);TextDb texts;texts.strings.resize(TEXT_COUNT,"Synthetic text ");texts.offsets.resize(TEXT_COUNT);texts.blob.assign(200,0);
    const auto seller=source.world.membership[0].clubs[1],buyer=source.world.membership[1].clubs[1];const auto person=source.world.clubs.at(seller).roster[19].player;
    auto& p=source.world.players.at(person);p.attributes.w368&=0x07ffffffu;p.bonus&=127;
    MarketListing listing;listing.player=person;listing.seller=seller;listing.fee=100000;listing.salary=1000;listing.asking_percent=100;listing.demand_percent=100;listing.weeks=1;
    listing.bids[0]={buyer,1000000,100000,24,{}};source.world.transfer_market->rows[0]=listing;
    const auto before=source;const auto sold=process_market_auctions(source,rules,texts,hooks());
    check(sold.processed==1&&sold.resolved==1&&sold.moved==1&&!source.world.transfer_market->rows[0],"AI auction did not settle its accepted bid");
    check(std::any_of(source.world.clubs.at(buyer).roster.begin(),source.world.clubs.at(buyer).roster.end(),[&](const auto& slot){return slot.player==person;}),"AI auction lost global buyer/player");
    check(read_save(make_save(source))==source,"AI auction snapshot failed");
    auto replay=before;process_market_auctions(replay,rules,texts,hooks());check(replay==source,"auction repeated input is nondeterministic");
    auto human=before;human.world.transfer_market->rows[0]->seller=human.world.managers[0].club;human.world.transfer_market->rows[0]->human_seller=true;human.world.transfer_market->rows[0]->player=human.world.clubs.at(human.world.managers[0].club).roster[19].player;
    const auto pending=human;auto missing=hooks();missing.select={};rejects([&]{process_market_auctions(human,rules,texts,missing);},"human auction silently declined without selector");check(human==pending,"missing selector changed state");
    auto accepted=pending;const auto human_player=accepted.world.transfer_market->rows[0]->player;const auto paid_fee=accepted.world.transfer_market->rows[0]->bids[0].fee;
    const auto human_sold=process_market_auctions(accepted,rules,texts,hooks());check(human_sold.moved==1&&accepted.transfers&&!accepted.transfers->history[1].empty()&&accepted.transfers->history[1].front().player==human_player&&accepted.transfers->history[1].front().other==buyer&&accepted.transfers->balance[1]==-int32_t(paid_fee),"human seller/AI buyer lost Source transfer ledger or canonical history");
    auto no=hooks();no.select=[](const GameState&,const TransferOfferListView&,const AiMatchProjection&)->std::optional<int>{return -1;};const auto declined=process_market_auctions(human,rules,texts,no);check(declined.resolved==1&&!declined.moved&&human.world.clubs==pending.world.clubs,"human auction decline moved player");
    auto cancel=pending;auto cancelled=hooks();cancelled.select=[](const GameState&,const TransferOfferListView&,const AiMatchProjection&)->std::optional<int>{return {};};rejects([&]{process_market_auctions(cancel,rules,texts,cancelled);},"cancelled auction committed");check(cancel==pending,"cancelled auction leaked RNG/state");
    auto free=before;auto record=free.world.players.at(person);record.id={0xf0001234u};record.attributes.name="Free agent";free.world.players[record.id]=record;free.world.free_players->domestic[0]=record.id;
    auto second=record;second.id={0xf0001235u};free.world.players[second.id]=second;free.world.free_players->domestic[1]=second.id;
    auto& entry=*free.world.transfer_market->rows[0];entry.player=record.id;entry.seller={};entry.bids[0].bidder=free.world.managers[1].club;
    auto bought=process_market_auctions(free,rules,texts,hooks());check(bought.moved==1&&!free.world.free_players->domestic[0].valid()&&free.world.free_players->domestic[1]==second.id,"free agent purchase shifted/lost native pool slots");
    check(read_save(make_save(free))==free,"free-agent purchase/history snapshot failed");
    auto missing_gate=before;missing_gate.world.transfer_market->rows[0]->bids[0].bidder=missing_gate.world.managers[1].club;const auto gate_before=missing_gate;auto no_gate=hooks();no_gate.allocator=TransferAllocatorContext::unsupported;
    rejects([&]{process_market_auctions(missing_gate,rules,texts,no_gate);},"human purchase guessed allocator observation");check(missing_gate==gate_before,"missing allocator changed paid state");
    auto human_borrower=before;human_borrower.world.transfer_market->rows[0]->bids[0].bidder=human_borrower.world.managers[1].club;human_borrower.world.transfer_market->rows[0]->bids[0].months=0x8c;
    const auto borrowed_result=process_market_auctions(human_borrower,rules,texts,hooks());check(borrowed_result.moved==1&&borrowed_result.loans==1&&human_borrower.world.loans[0].borrower==human_borrower.world.managers[1].club,"loan to human buyer lost global borrower/ledger");
    auto loan=before;loan.world.transfer_market->rows[0]->bids[0].months=0x8c;const auto leased=process_market_auctions(loan,rules,texts,hooks());check(leased.loans==1&&loan.world.loans.size()==1&&loan.world.loans[0].player==person,"AI loan lost global ledger");
    auto human_loan=pending;human_loan.world.transfer_market->rows[0]->bids[0].months=0x8c;const auto leased_human=process_market_auctions(human_loan,rules,texts,hooks());check(leased_human.loans==1&&human_loan.transfers->loans.size()==1,"human seller loan lost owner board");check(read_save(make_save(human_loan))==human_loan,"human loan snapshot failed");
    auto next=human_loan;const auto old_contracts=next.transfers->loans;auto another=listing;another.seller=next.world.managers[0].club;another.human_seller=true;another.player=next.world.clubs.at(another.seller).roster[18].player;another.bids[0].months=24;next.world.transfer_market->rows[0]=another;
    const auto second_sale=process_market_auctions(next,rules,texts,hooks());check(second_sale.moved==1&&next.transfers->loans==old_contracts&&next.world.loans==human_loan.world.loans,"subsequent auction discarded a prior loan/owner board");
    auto orphan=human_loan;orphan.transfers->loans.clear();orphan.world.transfer_market->rows[0]=another;const auto orphan_before=orphan;rejects([&]{process_market_auctions(orphan,rules,texts,hooks());},"missing previous human loan board was fabricated");check(orphan==orphan_before,"missing owner-board rejection changed state");
    auto blocked=before;blocked.world.transfer_market->rows[0]->bids[0].bidder=blocked.world.managers[1].club;const auto paid_before=blocked.world.managers[1].finance->balance;auto block_hooks=hooks();block_hooks.allocator=TransferAllocatorContext::blocked;
    const auto paid=process_market_auctions(blocked,rules,texts,block_hooks);check(!paid.moved&&paid.resolved==1&&blocked.world.managers[1].finance->balance<paid_before,"allocator-blocked purchase lost Source paid-account outcome");
    auto waiting=before;waiting.world.transfer_market->rows[0]->weeks=2;auto absent=hooks();absent.notice={};waiting.world.transfer_market->rows[0]->bids[0].bidder=waiting.world.managers[1].club;const auto unannounced=waiting;rejects([&]{process_market_auctions(waiting,rules,texts,absent);},"last-week human notice silently skipped");check(waiting==unannounced,"missing notice leaked countdown");
    auto foreign=before;foreign.world.transfer_market->rows[0]->bids[0].bidder=foreign.world.membership[12].clubs[1];const auto foreign_before=foreign;
    const auto refused=process_market_auctions(foreign,rules,texts,hooks());check(!refused.moved&&foreign.world.clubs==foreign_before.world.clubs,"foreign AI buyer used temporary local country for quota");
    foreign=foreign_before;foreign.world.players.at(person).attributes.w368|=4u<<27;check(process_market_auctions(foreign,rules,texts,hooks()).moved==1,"foreign AI buyer rejected its actual domestic nationality");
    auto quiet=before;quiet.world.transfer_market->rows[0]->weeks=3;const auto old_seed=quiet.watcom_seed;const auto countdown=process_market_auctions(quiet,rules,texts,hooks());check(!countdown.resolved&&!countdown.moved&&quiet.world.transfer_market->rows[0]->weeks==2&&quiet.watcom_seed==old_seed,"non-expiring quote lost countdown/RNG behavior");
    auto expired=before;expired.world.transfer_market->rows[0]->bids={};const auto empty_expiry=process_market_auctions(expired,rules,texts,hooks());check(empty_expiry.resolved==1&&!empty_expiry.moved,"unbid expiry failed");
    auto year_end=before;year_end.world.transfer_market->rows[0]->weeks=8;const auto ended=process_market_auctions(year_end,rules,texts,hooks(),true);check(ended.resolved==1&&ended.moved==1,"season-end auction did not settle before normal expiry");
    auto guarded=before;auto extra=AiMatchProjection::create_for_humans(guarded,{guarded.world.managers[0].club},rules,{3,6,false,true});const auto other=guarded.world.membership[12].clubs[0];std::array<uint8_t,10> history;history.fill(1);guarded.world.managers[0].match_state->head_to_head[other]=history;
    extra=AiMatchProjection::create_for_humans(guarded,{guarded.world.managers[0].club},rules,{3,6,false,true});extra.include_club_references(guarded.world,{other});const auto original=guarded.world;extra.commit_match_state(guarded.world);check(guarded.world==original,"extra loan/name reference erased earlier match history");
    std::cout<<"AI/human auction decline/cancel, free-agent native pool slots, AI/human-owner loans, notice ownership, snapshots and extra-reference guards passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
