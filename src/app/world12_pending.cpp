// SPDX-License-Identifier: GPL-3.0-only
#include "app/world12_pending.hpp"
#include "game/world12/desktop_projection.hpp"
#include "game/schedule.hpp"
#include <algorithm>
#include <bit>
#include <limits>
namespace bmh::app::world12 {
using namespace bmh::world12;
namespace {
constexpr auto ledger=layout::Career_RemoveJoinLoans_RuntimeField;
constexpr auto owner_players=layout::Career_CareerManagersEnd_RuntimeField;
constexpr auto owner_boards=layout::ClubInfo_FormatError_RuntimeField;
constexpr auto owner_managers=layout::ClubInfo_FormatError_RuntimeField2;
constexpr auto owner_counts=layout::DeskActions_Unsupported_RuntimeField;
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
    // Only canonical loans with their owned contract are admitted to this fresh
    // event projection. Their packed native words remain local to this workspace.
    local.memory.add_block(ledger,200*4);local.memory.add_block(layout::ClubPlayerDetail_InvalidArgument_RuntimeField,200);
    for(uint32_t n=0;n<200;++n){local.memory.set32(ledger+n*4,0);local.memory.set8(layout::ClubPlayerDetail_InvalidArgument_RuntimeField+n,0);}
    local.memory.add_block(owner_players,24);local.memory.add_block(owner_boards,12*0x58);local.memory.add_block(owner_managers,12);local.memory.add_block(owner_counts,4);
    for(uint32_t n=0;n<12;++n)local.memory.set16(owner_players+n*2,0xfff);for(uint32_t n=0;n<4;++n)local.memory.set8(owner_counts+n,0);
    if(!state.world.loans.empty()&&!state.transfers)throw FormatError("world12: loan contracts missing from pending workspace");
    for(size_t n=0;n<state.world.loans.size();++n){if(n>=200)throw FormatError("world12: native loan ledger exhausted");const auto& loan=state.world.loans[n];const auto p=projection.local_player(loan.player);
        local.memory.set32(ledger+uint32_t(n*4),p+projection.local_club(loan.owner)*0x1000u+projection.local_club(loan.borrower)*0x400000u);local.memory.set8(layout::ClubPlayerDetail_InvalidArgument_RuntimeField+uint32_t(n),loan.months);
        const auto contract=std::find_if(state.transfers->loans.begin(),state.transfers->loans.end(),[&](const auto& c){return c.player==loan.player;});if(contract==state.transfers->loans.end())throw FormatError("world12: missing explicit loan contract");
        const size_t slot=size_t(contract-state.transfers->loans.begin());uint8_t manager=0;while(projection.global_manager(manager)!=contract->manager)++manager;
        local.memory.set16(owner_players+uint32_t(slot*2),uint16_t(p));local.memory.set8(owner_managers+uint32_t(slot),manager);local.memory.set8(owner_counts+manager,uint8_t(local.memory.u8(owner_counts+manager)+1));
        for(size_t b=0;b<0x58;++b)local.memory.set8(owner_boards+uint32_t(slot*0x58+b),contract->board[b]);
        local.memory.set16(owner_boards+uint32_t(slot*0x58+0x44),contract->mark.valid()?uint16_t(projection.local_player(contract->mark)):0xfff);local.memory.set16(owner_boards+uint32_t(slot*0x58+0x46),contract->marked_by.valid()?uint16_t(projection.local_player(contract->marked_by)):0xfff);
    }
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
    candidate.world.transfer_market->rows[notice.row].reset();candidate.desktop->initial_event->offer.reset();
    projection.commit_transfer_state(candidate.world,listing.player,seller,buyer,result.transferred==1);
    if(!candidate.transfers)candidate.transfers.emplace();auto& transfers=*candidate.transfers;
    for(size_t n=0;n<count;++n)transfers.balance[projection.global_manager(uint8_t(n))]=std::bit_cast<int32_t>(word(local.manager_records,n*0xb0c+0x330));
    if(result.transferred){const auto rec=size_t(active)*0xb0c;const auto identity=word(local.manager_records,rec+0x7d8);
        if((identity&0xffff)!=local.player_name_offsets.at(player)||(identity>>16)!=local.club_ids.at(local_buyer))throw FormatError("world12: unqualified transfer-history identity");
        auto& rows=transfers.history[notice.seat];rows.insert(rows.begin(),{listing.player,buyer,word(local.manager_records,rec+0x760)});if(rows.size()>30)rows.resize(30);
        if(listing.bids[0].months&128){int found=-1;for(int n=0;n<200;++n)if(local.memory.u32(ledger+uint32_t(n*4))==player+local_seller*0x1000u+local_buyer*0x400000u){found=n;break;}if(found<0)throw FormatError("world12: accepted loan has no native ledger entry");
            candidate.world.loans.push_back({listing.player,seller,buyer,local.memory.u8(layout::ClubPlayerDetail_InvalidArgument_RuntimeField+uint32_t(found))});
            LoanContract contract;contract.player=listing.player;contract.manager=notice.seat;int slot=-1;for(int n=0;n<12;++n)if(local.memory.u16(owner_players+uint32_t(n*2))==player){slot=n;break;}if(slot<0)throw FormatError("world12: accepted loan has no owner board");
            for(size_t b=0;b<0x58;++b)contract.board[b]=local.memory.u8(owner_boards+uint32_t(size_t(slot)*0x58+b));const auto mark=local.memory.u16(owner_boards+uint32_t(slot*0x58+0x44)),marked=local.memory.u16(owner_boards+uint32_t(slot*0x58+0x46));contract.mark=mark==0xfff?PlayerId{}:projection.global_player(mark);contract.marked_by=marked==0xfff?PlayerId{}:projection.global_player(marked);for(size_t b=0x44;b<0x48;++b)contract.board[b]=0;transfers.loans.push_back(contract);
        }
    }
    candidate.game_seed=game.seed();candidate.watcom_seed=random.seed();candidate.validate();
    if(state!=before||owner.scope!=expected.scope||owner.revision!=expected.revision)throw FormatError("world12: pending-offer owner changed during decision");state=std::move(candidate);++owner.revision;return result;
}
}
