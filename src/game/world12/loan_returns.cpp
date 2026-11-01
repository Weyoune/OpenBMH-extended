// SPDX-License-Identifier: GPL-3.0-only
#include "game/world12/loan_returns.hpp"
#include "game/world12/economy.hpp"
#include "game/month.hpp"
#include "game/schedule.hpp"
#include <algorithm>
#include <stdexcept>
namespace bmh::world12 {
namespace {
constexpr auto ledger=layout::Career_RemoveJoinLoans_RuntimeField;
uint8_t manager(const AiMatchProjection& p,uint8_t seat){for(uint8_t m=0;m<p.human_managers();++m)if(p.global_manager(m)==seat)return m;throw FormatError("world12: loan return has an unknown active manager");}
}
LoanReturnOutcome return_monthly_loans(CareerSnapshot& state,const GameState& reference,const TextDb& texts,const LoanReturnHooks& hooks){
    state.validate();if(!state.world.economy||!state.world.settings||!state.world.transfer_market||state.world.managers.empty())throw FormatError("world12: monthly loan owner is unavailable");
    const auto before=state;auto candidate=state;assign_transfer_loan_slots(candidate);GameRandom game(state.game_seed);WatcomRandom random(state.watcom_seed);LoanReturnOutcome result;
    std::vector<ClubId> order;for(auto table:{TableGroup::germany,TableGroup::england,TableGroup::europe})for(auto league:state.world.catalogue.table_leagues(table))for(auto id:state.world.membership[league.value].clubs)order.push_back(id);
    // Source scans physical roster slots, not the loan ledger. A return closes
    // the borrower's gap; the next physical slot observes that shifted roster.
    for(auto club:order)for(size_t row=0;row<24;++row){const auto player=candidate.world.clubs.at(club).roster[row].player;if(!player.valid())continue;
        const auto found=std::find_if(candidate.world.loans.begin(),candidate.world.loans.end(),[&](const auto& l){return l.player==player;});if(found==candidate.world.loans.end())continue;const auto loan=*found;const auto ledger_slot=*loan.source_slot;const auto loan_index=size_t(found-candidate.world.loans.begin());
        std::vector<ClubId> clubs;for(const auto& m:candidate.world.managers)clubs.push_back(m.club);for(auto id:{loan.owner,loan.borrower})if(std::find(clubs.begin(),clubs.end(),id)==clubs.end())clubs.push_back(id);
        const auto& settings=*candidate.world.settings;auto p=AiMatchProjection::create_for_humans(candidate,clubs,reference,{settings.mode,settings.difficulty,settings.foreign_rule,true});install_career_economy(p,candidate.world);install_transfer_loans(p,candidate);auto& g=p.state;g.texts=&texts;
        const auto active=candidate.desktop&&candidate.desktop->active_seat?candidate.desktop->active_seat:hooks.active_seat;if(active)g.memory.set8(layout::ActiveManager,manager(p,*active));else if(p.human_managers()!=1)throw FormatError("world12: loan return lacks active manager identity");
        g.memory.add_block(layout::OfferSlots,16);g.memory.add_block(layout::EditorList,44*0x58);for(uint32_t n=0;n<4;++n){g.memory.set32(layout::OfferSlots+n*4,0xffffffffu);g.squad_slots.at(size_t(runtime_dimensions.pool_first)+40+n)=0xfff;}
        // Existing special offers need their own authenticated editor owner.
        for(size_t n=40;n<44;++n)if(candidate.world.transfer_market->rows[n])throw FormatError("world12: loan return cannot overwrite an owned special listing");
        g.loan_return_screen=[&](GameState& current,int borrower,const std::string& text){if(!hooks.keep)return -1;const auto answer=hooks.keep(current,p.global_manager(uint8_t(borrower)),text,p);return answer?int(*answer):-1;};
        int used_offer=-1;
        g.loan_return_offer_screen=[&](GameState& current,GameRandom& gr,WatcomRandom& wr,int borrower,int slot,int entry,const HumanBuyerSaleHooks& offers){if(!hooks.offer)return -1;used_offer=entry;return hooks.offer(current,gr,wr,p.global_manager(uint8_t(borrower)),slot,entry,offers,p);};
        MinuteReport report;return_loan_at(g,int(p.local_club(club)*24+row),random,&game,report);if(report.unsupported)throw FormatError(report.reason);++result.visited;
        const auto raw=g.memory.u32(ledger+uint32_t(ledger_slot*4));
        if(used_offer==40){for(uint32_t n=0;n<4;++n){const auto local=g.memory.u16(layout::EditorList+40*0x58+n*2);candidate.world.transfer_market->last_entry40_bidders[n]=local==0xffff?ClubId{}:p.global_club(local);}candidate.world.transfer_market->entry40_history_known=true;}
        if(loan.months!=1){candidate.world.loans[loan_index].months=g.memory.u8(layout::ClubPlayerDetail_InvalidArgument_RuntimeField+uint32_t(ledger_slot));p.commit_match_state(candidate.world);capture_career_economy(candidate.world,p);continue;}
        const auto local=p.local_player(player);ClubId final_club;
        for(auto id:clubs){const auto team=p.local_club(id);const auto begin=g.squad_slots.begin()+ptrdiff_t(team*24);if(std::any_of(begin,begin+24,[&](auto slot){return (slot&0xfff)==local;})){if(final_club.valid())throw FormatError("world12: returned player belongs to multiple clubs");final_club=id;}}
        if(final_club!=loan.owner&&final_club!=loan.borrower)throw FormatError("world12: return editor moved player to an unowned party");
        candidate.world.loans.erase(candidate.world.loans.begin()+ptrdiff_t(loan_index));
        if(candidate.transfers){auto& contracts=candidate.transfers->loans;contracts.erase(std::remove_if(contracts.begin(),contracts.end(),[&](const auto& c){return c.player==player;}),contracts.end());}
        if(final_club==loan.owner)p.commit_transfer_state(candidate.world,player,loan.borrower,loan.owner,true);else p.commit_match_state(candidate.world);
        capture_career_economy(candidate.world,p);capture_transfer_history(candidate,p);
        // Re-offers can create a fresh actual contract on the freed ledger slot.
        if(raw&&(raw&0xfff)==local){const auto owner=p.global_club((raw>>12)&0x3ff),borrower=p.global_club(raw>>22);capture_created_loan(candidate,p,player,owner,borrower);++result.retained;}
        if(final_club==loan.owner)++result.returned;
    }
    candidate.game_seed=game.seed();candidate.watcom_seed=random.seed();candidate.validate();if(state!=before)throw FormatError("world12: loan return owner changed during processing");
    result.game_draws=game.calls();result.watcom_draws=random.calls();state=std::move(candidate);return result;
}
}
