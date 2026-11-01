// SPDX-License-Identifier: GPL-3.0-only
#include "world12_fixture.hpp"
#include "game/world12/loan_returns.hpp"
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
    auto rules=reference();const auto initial=prepared(rules);TextDb texts;texts.strings.resize(TEXT_COUNT,"Synthetic return ");texts.offsets.resize(TEXT_COUNT);texts.blob.assign(200,0);
    for(int kind=0;kind<4;++kind){auto state=initial;const auto owner=kind&1?state.world.membership[0].clubs[1]:state.world.managers[0].club;const auto borrower=kind&2?state.world.managers[1].club:state.world.membership[1].clubs[1];const auto person=state.world.clubs.at(owner).roster[19].player;
        MarketListing listing;listing.player=person;listing.seller=owner;listing.human_seller=!(kind&1);listing.fee=100000;listing.salary=1000;listing.asking_percent=100;listing.demand_percent=100;listing.weeks=1;listing.bids[0]={borrower,1000000,100000,0x8c,{}};state.world.transfer_market->rows[0]=listing;
        const auto moved=process_market_auctions(state,rules,texts,hooks());check(moved.loans==1,"return fixture did not create real loan");
        LoanReturnHooks returns;returns.active_seat=1;returns.keep=[](const GameState&,uint8_t seat,const std::string&,const AiMatchProjection&)->std::optional<bool>{check(seat==3,"human return used wrong global borrower");return false;};
        auto running=state;const auto waiting=return_monthly_loans(running,rules,texts,returns);check(waiting.visited>=1&&running.world.loans.size()==1&&running.world.loans[0].months==11,"live loan countdown failed");
        state.world.loans[0].months=1;const auto before=state;const auto returned=return_monthly_loans(state,rules,texts,returns);
        check(returned.returned==1&&state.world.loans.empty()&&state.transfers->loans.empty(),"expired loan did not release ledger/owner snapshot");
        check(std::any_of(state.world.clubs.at(owner).roster.begin(),state.world.clubs.at(owner).roster.end(),[&](const auto& slot){return slot.player==person;}),"expired player did not return to global owner");
        check(read_save(make_save(state))==state,"returned roster/board/notice snapshot failed");
        if(kind&2){auto missing=before;LoanReturnHooks absent;absent.active_seat=1;rejects([&]{return_monthly_loans(missing,rules,texts,absent);},"missing human return answer was silently declined");check(missing==before,"missing return answer leaked mutation/RNG");
            auto keep=before;LoanReturnHooks wanted=returns;wanted.keep=[](const GameState&,uint8_t,const std::string&,const AiMatchProjection&)->std::optional<bool>{return true;};
            rejects([&]{return_monthly_loans(keep,rules,texts,wanted);},"missing re-offer editor was fabricated");check(keep==before,"missing re-offer editor leaked return");
            int opened=0;wanted.offer=[&](GameState&,GameRandom&,WatcomRandom&,uint8_t actor,int slot,int entry,const HumanBuyerSaleHooks&,const AiMatchProjection&){check(actor==3&&entry==40&&slot==runtime_dimensions.pool_first+40,"return offer editor lost actor/special slot");++opened;return 0;};
            const auto left=return_monthly_loans(keep,rules,texts,wanted);check(opened==1&&left.returned==1&&keep.world.loans.empty(),"owned return editor exit did not complete actual return");
            auto submitted=before;int submissions=0;
            wanted.offer=[&](GameState& g,GameRandom& gr,WatcomRandom& wr,uint8_t actor,int slot,int entry,const HumanBuyerSaleHooks&,const AiMatchProjection& projection){
                int m=0;while(projection.global_manager(uint8_t(m))!=actor)++m;
                g.memory.add_block(layout::CareerSession_Invalid_RuntimeField2,4);g.memory.set32(layout::CareerSession_Invalid_RuntimeField2,uint32_t(gr.seed()));g.memory.add_block(layout::DeskActions_OpenTransferOfferEditorSource_RuntimeField,1);
                const auto at=layout::EditorList+uint32_t(entry*0x58);
                TransferOfferEditorRequest request{m,g.manager_teams.at(size_t(m)),entry,slot,int(g.squad_slots.at(size_t(slot))&0xfff),g.memory.u16(at+8),0};request.loan_return_entry=true;
                auto editor=open_transfer_offer_editor_source(g,request,gr,wr);if(!editor.supported||!editor.editable)throw std::runtime_error("real re-offer editor unavailable: "+editor.reason);
                editor=edit_transfer_offer_editor_source(g,editor,{TransferOfferEditorField::fee,g.memory.u32(at+0x2c),true},gr,wr);
                editor=edit_transfer_offer_editor_source(g,editor,{TransferOfferEditorField::salary,g.memory.u32(at+0x30),true},gr,wr);
                HumanBuyerSaleHooks sale;sale.allocator_gate=HumanBuyerSaleAllocatorGate::zero;sale.direct_mode=HumanBuyerSaleDirectMode::ordinary_ai_purchase;sale.notice=[](const GameState&,const HumanBuyerSaleNotice&){};
                MinuteReport report;const auto result=submit_transfer_offer_editor_source(g,editor,wr,sale,report);check(!report.unsupported,report.reason.c_str());
                check(result.action.status==TransferActionStatus::accepted||result.action.status==TransferActionStatus::pending,"real re-offer rejected");++submissions;return 1;
            };
            return_monthly_loans(submitted,rules,texts,wanted);check(submissions==1,"return did not submit shared editor");check(read_save(make_save(submitted))==submitted,"submitted return snapshot failed");
            auto stopped=before;wanted.keep=[](const GameState&,uint8_t,const std::string&,const AiMatchProjection&)->std::optional<bool>{return {};};rejects([&]{return_monthly_loans(stopped,rules,texts,wanted);},"cancelled return question committed");check(stopped==before,"cancelled return lost original contract");
        }
    }
    {
        auto state=initial;const auto owner=state.world.managers[0].club,borrower=state.world.managers[1].club;
        const auto add=[&](CareerSnapshot& target){const auto person=target.world.clubs.at(owner).roster[18].player;MarketListing listing;listing.player=person;listing.seller=owner;listing.human_seller=true;listing.fee=100000;listing.salary=1000;listing.asking_percent=100;listing.demand_percent=100;listing.weeks=1;listing.bids[0]={borrower,1000000,100000,0x8c,{}};target.world.transfer_market->rows[0]=listing;check(process_market_auctions(target,rules,texts,hooks()).loans==1,"multiple-loan fixture failed");return person;};
        add(state);add(state);for(auto& loan:state.world.loans)loan.months=1;
        LoanReturnHooks decline;decline.active_seat=1;decline.keep=[](const GameState&,uint8_t,const std::string&,const AiMatchProjection&)->std::optional<bool>{return false;};
        const auto first=return_monthly_loans(state,rules,texts,decline);check(first.returned==1&&state.world.loans.size()==1&&state.world.loans[0].months==1,"return pass lost Original shifted-roster skip");
        check(state.world.loans[0].source_slot==1&&state.transfers->loans[0].source_slot==1,"return compacted physical loan/board holes");
        state=read_save(make_save(state));add(state);check(state.world.loans[0].source_slot==1&&state.world.loans[1].source_slot==0&&state.transfers->loans[1].source_slot==0,"new loan did not reuse first physical hole");
        auto invalid=state;invalid.world.loans[1].source_slot=1;rejects([&]{invalid.validate();},"duplicate physical ledger slot accepted");
        invalid=state;invalid.transfers->loans[1].source_slot=1;rejects([&]{invalid.validate();},"duplicate physical board slot accepted");
        check(read_save(make_save(state))==state,"sparse loan slots failed save roundtrip");
    }
    std::cout<<"Real AI/human owner and borrower loan countdown/return, saved owner boards, explicit questions/editors, snapshots and rollback passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
