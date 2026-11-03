// SPDX-License-Identifier: GPL-3.0-only
#include "app/world12_pending.hpp"
#include "game/world12/desktop_projection.hpp"
#include "game/world12/economy.hpp"
#include "game/world12/market_auctions.hpp"
#include "game/schedule.hpp"
#include <algorithm>
#include <bit>
#include <limits>
namespace bmh::app::world12 {
using namespace bmh::world12;
namespace {
uint32_t word(const Bytes& b,size_t at){return uint32_t(b.at(at))|uint32_t(b.at(at+1))<<8|uint32_t(b.at(at+2))<<16|uint32_t(b.at(at+3))<<24;}
}
PendingOfferResult resolve_initial_offer(CareerSnapshot& state,ManagerActionOwner& owner,
    ManagerActionOwner expected,const GameState& reference,const TextDb& texts,const PendingOfferHooks& hooks) {
    state.validate();
    if(!owner.scope||owner.scope!=expected.scope||owner.revision!=expected.revision||owner.revision==std::numeric_limits<uint64_t>::max()||!state.desktop||!state.desktop->initial_event||!state.desktop->initial_event->offer||!state.desktop->active_seat)
        throw FormatError("world12: stale pending-offer owner");
    const auto before=state;auto candidate=state;const auto notice=*state.desktop->initial_event->offer;const auto listing=*state.world.transfer_market->rows[notice.row];
    const auto buyer=listing.bids[0].bidder;const auto seller=listing.seller;
    if(std::any_of(state.world.managers.begin(),state.world.managers.end(),[&](const auto& m){return m.club==buyer;}))throw FormatError("world12: initial pending offer requires its AI bidder");
    std::vector<ClubId> extra{buyer};for(const auto& loan:state.world.loans){extra.push_back(loan.owner);extra.push_back(loan.borrower);}
    auto projection=project_manager_desktop(candidate,reference,extra);auto& local=projection.state;local.texts=&texts;
    const auto local_seller=projection.local_club(seller),local_buyer=projection.local_club(buyer),player=projection.local_player(listing.player);
    const auto count=projection.human_managers();const auto active=local.memory.u8(layout::ActiveManager);
    local.memory.add_block(layout::DeskPosition,1);local.memory.add_block(layout::NextDeskManager,8);local.memory.add_block(layout::CareerSession_Invalid_RuntimeField2,4);
    local.memory.set8(layout::DeskPosition,state.desktop->initial_event->manager_position);
    for(size_t n=0;n<count;++n){const auto seat=state.desktop->initial_event->manager_order[n];uint8_t manager=0;while(projection.global_manager(manager)!=seat)++manager;local.memory.set16(layout::NextDeskManager+uint32_t(n*2),manager);}
    local.memory.set32(layout::CareerSession_Invalid_RuntimeField2,uint32_t(state.world.transfer_market->checkpoint_game_seed));
    // The shared owner distinguishes AI ledger rows from human owner boards,
    // and retains their actual physical slots and qualified player references.
    install_transfer_loans(projection,candidate);
    const auto at=layout::EditorList+uint32_t(notice.row)*0x58;local.memory.add_block(at,0x58);for(uint32_t n=0;n<0x58;++n)local.memory.set8(at+n,0);
    for(uint32_t n=0;n<4;++n)local.memory.set16(at+n*2,0xffff);local.memory.set16(at,uint16_t(local_buyer));local.memory.set16(at+8,uint16_t(local_seller|0x8000));
    local.memory.set32(at+0xc,listing.bids[0].salary);local.memory.set32(at+0x1c,listing.bids[0].fee);local.memory.set32(at+0x2c,listing.fee);local.memory.set32(at+0x30,listing.salary);local.memory.set8(at+0x34,listing.bids[0].months);
    for(uint32_t n=0;n<7;++n)local.memory.set8(at+0x38+n,listing.bids[0].bonuses[n]);local.memory.set8(at+0x54,listing.asking_percent);local.memory.set8(at+0x55,listing.demand_percent);local.memory.set8(at+0x56,listing.weeks);
    local.squad_slots.at(size_t(runtime_dimensions.pool_first)+notice.row)=uint16_t(player);
    GameRandom game(state.game_seed);WatcomRandom random(state.watcom_seed);MinuteReport report;
    PendingOfferDeskRequest request;request.runtime_mode=PendingOfferDeskRuntimeMode::normal_interactive;request.manager_count=int(count);request.manager_position=state.desktop->initial_event->manager_position;
    const auto result=process_natural_pending_offers_after_desk(local,request,game,random,hooks,report);
    if(report.unsupported||!result.completed||result.processed!=1||result.transferred>1)throw FormatError(report.reason.empty()?"world12: incomplete pending-offer transaction":report.reason);
    // Remove the resolved quote before validating changed roster ownership.
    if(notice.row==40){for(size_t n=0;n<4;++n)candidate.world.transfer_market->last_entry40_bidders[n]=listing.bids[n].bidder;candidate.world.transfer_market->entry40_history_known=true;}
    candidate.world.transfer_market->rows[notice.row].reset();candidate.desktop->initial_event->offer.reset();
    projection.commit_transfer_state(candidate.world,listing.player,seller,buyer,result.transferred==1);
    if(!candidate.transfers)candidate.transfers.emplace();auto& transfers=*candidate.transfers;
    for(size_t n=0;n<count;++n)transfers.balance[projection.global_manager(uint8_t(n))]=std::bit_cast<int32_t>(word(local.manager_records,n*0xb0c+0x330));
    if(result.transferred){const auto rec=size_t(active)*0xb0c;const auto identity=word(local.manager_records,rec+0x7d8);
        if((identity&0xffff)!=local.player_name_offsets.at(player)||(identity>>16)!=local.club_ids.at(local_buyer))throw FormatError("world12: unqualified transfer-history identity");
        auto& rows=transfers.history[notice.seat];rows.insert(rows.begin(),{listing.player,buyer,word(local.manager_records,rec+0x760)});if(rows.size()>30)rows.resize(30);
        if(listing.bids[0].months&128){const auto previous=candidate.world.loans.size();capture_created_loan(candidate,projection,listing.player,seller,buyer);if(candidate.world.loans.size()!=previous+1)throw FormatError("world12: accepted loan has no native ledger entry");}
    }
    if(candidate.world.economy)capture_career_economy(candidate.world,projection);
    candidate.game_seed=game.seed();candidate.watcom_seed=random.seed();candidate.validate();
    if(state!=before||owner.scope!=expected.scope||owner.revision!=expected.revision)throw FormatError("world12: pending-offer owner changed during decision");state=std::move(candidate);++owner.revision;return result;
}
}
