// SPDX-License-Identifier: GPL-3.0-only
#include "game/world12/market_auctions.hpp"
#include "game/world12/economy.hpp"
#include "game/schedule.hpp"
#include <algorithm>
#include <bit>
#include <stdexcept>
namespace bmh::world12 {
namespace {
constexpr auto ledger=layout::Career_RemoveJoinLoans_RuntimeField;
constexpr auto owner_players=layout::Career_CareerManagersEnd_RuntimeField;
constexpr auto owner_boards=layout::ClubInfo_FormatError_RuntimeField;
constexpr auto owner_managers=layout::ClubInfo_FormatError_RuntimeField2;
constexpr auto owner_counts=layout::DeskActions_Unsupported_RuntimeField;
uint32_t word(const Bytes& b,size_t at){return uint32_t(b.at(at))|uint32_t(b.at(at+1))<<8|uint32_t(b.at(at+2))<<16|uint32_t(b.at(at+3))<<24;}
void put(Bytes& b,size_t at,uint32_t v){for(size_t n=0;n<4;++n)b.at(at+n)=uint8_t(v>>(8*n));}
bool human(const World& w,ClubId id){return id.valid()&&std::any_of(w.managers.begin(),w.managers.end(),[&](const auto& m){return m.club==id;});}
uint8_t local_manager(const AiMatchProjection& p,uint8_t seat){for(uint8_t m=0;m<p.human_managers();++m)if(p.global_manager(m)==seat)return m;throw FormatError("world12: loan owner has no projected manager");}
void install_loans(AiMatchProjection& p,const CareerSnapshot& state){
    std::vector<PlayerId> players;std::vector<ClubId> clubs;
    for(const auto& loan:state.world.loans){players.push_back(loan.player);clubs.push_back(loan.owner);clubs.push_back(loan.borrower);}
    if(state.transfers)for(const auto& loan:state.transfers->loans){players.push_back(loan.mark);players.push_back(loan.marked_by);}
    p.include_players(state.world,players);p.include_club_references(state.world,clubs);auto& g=p.state;
    for(const auto& loan:state.world.loans)if(human(state.world,loan.owner)&&(!state.transfers||std::none_of(state.transfers->loans.begin(),state.transfers->loans.end(),[&](const auto& contract){return contract.player==loan.player;})))throw FormatError("world12: existing human-owned loan has no owner board");
    if(state.world.loans.size()>200)throw FormatError("world12: auction loan ledger exhausted");
    g.memory.add_block(ledger,800);g.memory.add_block(layout::ClubPlayerDetail_InvalidArgument_RuntimeField,200);
    for(size_t n=0;n<200;++n){g.memory.set32(ledger+uint32_t(n*4),0);g.memory.set8(layout::ClubPlayerDetail_InvalidArgument_RuntimeField+uint32_t(n),0);}
    for(size_t n=0;n<state.world.loans.size();++n){const auto& l=state.world.loans[n];g.memory.set32(ledger+uint32_t(n*4),p.local_player(l.player)+p.local_club_reference(l.owner)*0x1000u+p.local_club_reference(l.borrower)*0x400000u);g.memory.set8(layout::ClubPlayerDetail_InvalidArgument_RuntimeField+uint32_t(n),l.months);}
    g.memory.add_block(owner_players,24);g.memory.add_block(owner_boards,12*0x58);g.memory.add_block(owner_managers,12);g.memory.add_block(owner_counts,4);
    for(size_t n=0;n<12;++n)g.memory.set16(owner_players+uint32_t(n*2),0xfff);
    for(size_t n=0;n<4;++n)g.memory.set8(owner_counts+uint32_t(n),0);
    if(state.transfers)for(size_t n=0;n<state.transfers->loans.size();++n){const auto& l=state.transfers->loans[n];const auto m=local_manager(p,l.manager);
        g.memory.set16(owner_players+uint32_t(n*2),uint16_t(p.local_player(l.player)));g.memory.set8(owner_managers+uint32_t(n),m);g.memory.set8(owner_counts+m,uint8_t(g.memory.u8(owner_counts+m)+1));
        for(size_t b=0;b<0x58;++b)g.memory.set8(owner_boards+uint32_t(n*0x58+b),l.board[b]);
        g.memory.set16(owner_boards+uint32_t(n*0x58+0x44),l.mark.valid()?uint16_t(p.local_player(l.mark)):0xfff);g.memory.set16(owner_boards+uint32_t(n*0x58+0x46),l.marked_by.valid()?uint16_t(p.local_player(l.marked_by)):0xfff);
    }
}
void capture_transfer_state(CareerSnapshot& state,const AiMatchProjection& p){
    if(!state.transfers)state.transfers.emplace();auto& t=*state.transfers;const auto& g=p.state;
    for(uint8_t m=0;m<p.human_managers();++m){const auto seat=p.global_manager(m);const size_t at=size_t(m)*0xb0c;t.balance[seat]=std::bit_cast<int32_t>(word(g.manager_records,at+0x330));
        const auto identity=word(g.manager_records,at+0x7d8);if(!identity)continue;
        const auto name=uint16_t(identity);const auto it=std::find(g.player_name_offsets.begin(),g.player_name_offsets.end(),name);if(it==g.player_name_offsets.end())throw FormatError("world12: auction history player is unqualified");
        ClubId other;const auto club_name=uint16_t(identity>>16);if(club_name!=0xffff){const auto club=std::find(g.club_ids.begin(),g.club_ids.end(),club_name);if(club==g.club_ids.end())throw FormatError("world12: auction history club is unqualified");other=p.global_club(uint32_t(club-g.club_ids.begin()));}
        auto& rows=t.history[seat];rows.insert(rows.begin(),{p.global_player(uint32_t(it-g.player_name_offsets.begin())),other,word(g.manager_records,at+0x760)});if(rows.size()>30)rows.resize(30);
    }
}
void capture_new_loan(CareerSnapshot& state,const AiMatchProjection& p,PlayerId player,ClubId seller,ClubId buyer){
    const auto& g=p.state;const auto local=p.local_player(player);int found=-1;
    for(int n=0;n<200;++n){const auto raw=g.memory.u32(ledger+uint32_t(n*4));if(raw&&(raw&0xfff)==local){if(p.global_club((raw>>12)&0x3ff)!=seller||p.global_club(raw>>22)!=buyer)throw FormatError("world12: auction loan parties mismatch");found=n;break;}}
    if(found<0)return;
    state.world.loans.push_back({player,seller,buyer,g.memory.u8(layout::ClubPlayerDetail_InvalidArgument_RuntimeField+uint32_t(found))});
    if(!human(state.world,seller))return;
    for(size_t n=0;n<12;++n)if(g.memory.u16(owner_players+uint32_t(n*2))==local){LoanContract contract;contract.player=player;contract.manager=p.global_manager(g.memory.u8(owner_managers+uint32_t(n)));
        for(size_t b=0;b<0x58;++b)contract.board[b]=g.memory.u8(owner_boards+uint32_t(n*0x58+b));
        const auto mark=g.memory.u16(owner_boards+uint32_t(n*0x58+0x44)),marked=g.memory.u16(owner_boards+uint32_t(n*0x58+0x46));contract.mark=mark==0xfff?PlayerId{}:p.global_player(mark);contract.marked_by=marked==0xfff?PlayerId{}:p.global_player(marked);for(size_t b=0x44;b<0x48;++b)contract.board[b]=0;
        state.transfers->loans.push_back(contract);return;
    }
    throw FormatError("world12: auction loan lost its human owner board");
}
}
MarketAuctionOutcome process_market_auctions(CareerSnapshot& state,const GameState& reference,const TextDb& texts,const MarketAuctionHooks& hooks,bool season_end){
    state.validate();if(!state.world.settings||!state.world.transfer_market||!state.world.economy||state.world.managers.empty())throw FormatError("world12: auction phase is unavailable");
    const auto before=state;auto candidate=state;GameRandom game(state.game_seed);WatcomRandom random(state.watcom_seed);MarketAuctionOutcome outcome;
    for(uint8_t row=0;row<40;++row){if(!candidate.world.transfer_market->rows[row])continue;const auto listing=*candidate.world.transfer_market->rows[row];
        std::vector<ClubId> clubs;for(const auto& m:candidate.world.managers)clubs.push_back(m.club);
        const auto add=[&](ClubId id){if(id.valid()&&std::find(clubs.begin(),clubs.end(),id)==clubs.end())clubs.push_back(id);};add(listing.seller);for(const auto& b:listing.bids)add(b.bidder);
        const auto& s=*candidate.world.settings;auto p=AiMatchProjection::create_for_humans(candidate,clubs,reference,{s.mode,s.difficulty,s.foreign_rule,true});
        p.include_players(candidate.world,{listing.player});install_career_economy(p,candidate.world);install_loans(p,candidate);auto& g=p.state;g.texts=&texts;
        const auto active=candidate.desktop&&candidate.desktop->active_seat?candidate.desktop->active_seat:hooks.active_seat;
        if(active)g.memory.set8(layout::ActiveManager,local_manager(p,*active));else if(p.human_managers()!=1)throw FormatError("world12: auction has no active manager identity");
        if(!listing.seller.valid())g.squad_slots.at(size_t(runtime_dimensions.free_first))=uint16_t(p.local_player(listing.player));
        if(candidate.transfers)for(uint8_t m=0;m<p.human_managers();++m){const auto seat=p.global_manager(m);put(g.manager_records,size_t(m)*0xb0c+0x330,uint32_t(candidate.transfers->balance[seat]));g.manager_records[size_t(m)*0xb0c+0x21d]=uint8_t(std::min<size_t>(25,candidate.transfers->history[seat].size()));}
        g.memory.add_block(layout::EditorList,44*0x58);
        for(size_t n=0;n<40;++n){g.squad_slots.at(size_t(runtime_dimensions.pool_first)+n)=0xfff;for(size_t b=0;b<4;++b)g.memory.set16(layout::EditorList+uint32_t(n*0x58+b*2),0xffff);}
        const auto at=layout::EditorList+uint32_t(row*0x58);for(uint32_t n=0;n<0x58;++n)g.memory.set8(at+n,0);
        g.memory.set16(at+8,listing.seller.valid()?uint16_t(p.local_club(listing.seller)|(listing.human_seller?0x8000:0)):0xfff);
        for(size_t b=0;b<4;++b){const auto& bid=listing.bids[b];g.memory.set16(at+uint32_t(b*2),bid.bidder.valid()?uint16_t(p.local_club(bid.bidder)):0xffff);g.memory.set32(at+0xc+uint32_t(b*4),bid.salary);g.memory.set32(at+0x1c+uint32_t(b*4),bid.fee);g.memory.set8(at+0x34+uint32_t(b),bid.months);for(size_t n=0;n<7;++n)g.memory.set8(at+0x38+uint32_t(b*7+n),bid.bonuses[n]);}
        g.memory.set32(at+0x2c,listing.fee);g.memory.set32(at+0x30,listing.salary);g.memory.set8(at+0x54,listing.asking_percent);g.memory.set8(at+0x55,listing.demand_percent);g.memory.set8(at+0x56,listing.weeks);g.squad_slots.at(size_t(runtime_dimensions.pool_first)+row)=uint16_t(p.local_player(listing.player));
        g.transfer_offer_selector=[&](const GameState& local,const TransferOfferListView& view){if(!hooks.select)throw FormatError("world12: missing human auction selector");const auto answer=hooks.select(local,view,p);if(!answer)throw FormatError("world12: auction selector cancelled");return *answer;};
        g.transfer_decision_observer=[&](const TransferDecisionEvent& event){if(!hooks.notice)throw FormatError("world12: missing owned auction notice presenter");hooks.notice(g,event,p);};
        MinuteReport report;transfer_list_countdown(g,game,random,report,season_end,hooks.allocator);if(report.unsupported)throw FormatError(report.reason);
        const auto local_player=p.local_player(listing.player);ClubId buyer;
        for(const auto& bid:listing.bids)if(bid.bidder.valid()&&bid.bidder!=listing.seller){const auto team=p.local_club(bid.bidder);const auto begin=g.squad_slots.begin()+ptrdiff_t(team*24);if(std::any_of(begin,begin+24,[&](auto slot){return (slot&0xfff)==local_player;})){if(buyer.valid()&&buyer!=bid.bidder)throw FormatError("world12: auction player at multiple buyers");buyer=bid.bidder;}}
        const bool resolved=g.squad_slots.at(size_t(runtime_dimensions.pool_first)+row)==0xfff;
        if(resolved)candidate.world.transfer_market->rows[row].reset();else{auto& next=*candidate.world.transfer_market->rows[row];next.weeks=g.memory.u8(at+0x56);for(size_t b=0;b<4;++b){const auto bidder=g.memory.u16(at+uint32_t(b*2));next.bids[b].bidder=bidder==0xffff?ClubId{}:p.global_club(bidder);next.bids[b].fee=g.memory.u32(at+0x1c+uint32_t(b*4));next.bids[b].salary=g.memory.u32(at+0xc+uint32_t(b*4));}}
        if(buyer.valid())p.commit_transfer_state(candidate.world,listing.player,listing.seller,buyer,true);else p.commit_match_state(candidate.world);
        capture_career_economy(candidate.world,p);capture_transfer_state(candidate,p);
        const auto previous=candidate.world.loans.size();if(buyer.valid())capture_new_loan(candidate,p,listing.player,listing.seller,buyer);
        ++outcome.processed;outcome.resolved+=resolved;outcome.moved+=buyer.valid();outcome.loans+=candidate.world.loans.size()-previous;
    }
    candidate.game_seed=game.seed();candidate.watcom_seed=random.seed();candidate.validate();if(state!=before)throw FormatError("world12: auction owner changed during decision");
    outcome.game_draws=game.calls();outcome.watcom_draws=random.calls();state=std::move(candidate);return outcome;
}
}
