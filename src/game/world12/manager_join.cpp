// SPDX-License-Identifier: GPL-3.0-only
#include "game/world12/manager_join.hpp"
#include "game/world12/manager_tactics.hpp"
#include "game/world12/manager_finance.hpp"
#include "game/world12/manager_match_state.hpp"
#include "game/world12/economy.hpp"
#include "game/world12/national_coach.hpp"
#include "game/career.hpp"
#include <algorithm>
namespace bmh::world12 {
ManagerJoiningOutcome add_running_manager(CareerSnapshot& state,const InitialManagerChoice& choice,
    const GameState& reference,const LeImage& country_rules) {
    state.validate();
    if(!state.world.settings||!state.world.economy||!state.world.markets||!state.desktop||!state.desktop->active_seat||!state.desktop->initial_event||!state.calendar||!state.transfers||national_roles_active(state.world)||
       state.world.managers.size()>=4||state.world.managers.size()>=state.world.settings->planned_managers||choice.seat>=4||choice.name.empty()||choice.name.size()>20||choice.portrait>11||
       std::any_of(state.world.managers.begin(),state.world.managers.end(),[&](const auto& m){return m.seat==choice.seat||m.club==choice.club||m.portrait==choice.portrait;})||state.desktop->initial_event->offer||
       (state.prepared_event&&(state.prepared_event->gates_booked||state.prepared_event->desk_phase!=PreparedEvent::DeskPhase::desks)))
        throw FormatError("world12: running manager entry lacks its actual club desk owner");
    const auto selected=state.world.clubs.find(choice.club);
    if(selected==state.world.clubs.end()||selected->second.kind!=ClubKind::club||!selected->second.league.valid()||selected->second.league!=choice.start_league||!state.world.catalogue.selectable(selected->second.country))
        throw FormatError("world12: running manager selected an unavailable club");
    const auto visible=state.world.catalogue.player_leagues(selected->second.country);
    if(std::find(visible.begin(),visible.end(),selected->second.league)==visible.end())throw FormatError("world12: hidden league cannot be taken over");
    const auto owner_loans=std::count_if(state.world.loans.begin(),state.world.loans.end(),[&](const auto& loan){return loan.owner==choice.club;});
    if(owner_loans>3||state.transfers->loans.size()+size_t(owner_loans)>12)throw FormatError("world12: takeover exceeds the existing human loan-contract capacity");
    const auto before=state;auto candidate=state;auto& world=candidate.world;const auto settings=*world.settings;WatcomRandom random(candidate.watcom_seed);
    auto initial=reference;initial.date=state.date;initial.season=int(state.season);initial.season_start=state.date-state.calendar->next_day;
    const auto rec=size_t(choice.seat)*0xb0c;
    if(initial.manager_records.size()<5*0xb0c||initial.manager_options.size()<20)throw FormatError("world12: running manager lacks its Source numeric template");
    // Source clean personal record. No former club/person history is attributed.
    std::fill_n(initial.manager_records.begin()+rec,0xb0c,uint8_t(0));
    for(size_t i=0;i<18;++i)initial.manager_records[rec+0x1f6+i]=1;initial.manager_records[rec+0x1f6]=0x40;
    std::fill_n(initial.manager_records.begin()+rec+0x8b6,10,uint8_t(255));initial.manager_records[rec+0x210]=world.catalogue.league(choice.start_league).local_index;
    initial.manager_records[rec+0x221]=choice.portrait;initial.manager_options[choice.seat*5]=1;
    for(size_t i=0;i<4;++i)initial.manager_options[choice.seat*5+1+i]=reference.manager_options[choice.seat*5+1+i];
    initial.memory.add_block(layout::GameMode,1);initial.memory.set8(layout::GameMode,settings.mode);
    initial.memory.add_block(layout::Difficulty,1);initial.memory.set8(layout::Difficulty,settings.difficulty);initial.level=settings.difficulty;
    initial.memory.add_block(layout::CareerDuration,1);initial.memory.set8(layout::CareerDuration,settings.duration);
    initial.memory.add_block(layout::Career_CareerAddManager_RuntimeField2,4*0x24);
    initialize_career_manager_facilities(initial,country_rules,choice.seat,world.catalogue.league(choice.start_league).local_index,settings.duration,random);
    Manager manager{choice.seat,choice.name,choice.club,choice.start_league,choice.portrait};manager.career=manager_career_state_from_legacy(initial,choice.seat);
    world.managers.push_back(std::move(manager));
    const bool indoor=candidate.prepared_event&&std::any_of(candidate.prepared_event->groups.begin(),candidate.prepared_event->groups.end(),[&](const auto& g){return g.indoor&&std::any_of(g.matches.begin(),g.matches.end(),[&](const auto& m){return m.fixture.home==choice.club||m.fixture.away==choice.club;});});
    const TacticalPolicy policy{settings.mode,indoor,false,settings.foreign_rule,settings.difficulty};
    initialize_joining_manager_contracts(world,choice.seat,initial,policy,random);
    auto& added=*std::find_if(world.managers.begin(),world.managers.end(),[&](const auto& m){return m.seat==choice.seat;});
    install_manager_career_state(initial,choice.seat,*added.career,{});install_manager_finance(initial,choice.seat,*added.finance);
    initial.memory.add_block(layout::Loans,4*0x78);for(uint32_t i=0;i<0x78;i+=4)initial.memory.set32(layout::Loans+uint32_t(choice.seat)*0x78+i,0);
    world.economy->managers[choice.seat]=manager_economy_from_legacy(initial,choice.seat);
    std::array<bool,12> board_slots{};
    for(const auto& contract:candidate.transfers->loans)if(contract.source_slot)board_slots[*contract.source_slot]=true;
    for(const auto& contract:candidate.transfers->loans)if(!contract.source_slot){const auto slot=std::find(board_slots.begin(),board_slots.end(),false);if(slot==board_slots.end())throw FormatError("world12: existing loan boards exceed capacity");*slot=true;}
    for(const auto& loan:world.loans)if(loan.owner==choice.club){
        auto board=initialize_joined_owner_loan_board(world,choice.seat,loan.player,policy,random,state.season);
        const auto slot=std::find(board_slots.begin(),board_slots.end(),false);if(slot==board_slots.end())throw FormatError("world12: no Source owner loan-board slot remains");const auto index=uint8_t(slot-board_slots.begin());*slot=true;
        candidate.transfers->loans.push_back({loan.player,choice.seat,board,{},{},index});
    }
    if(world.transfer_market){
        for(auto& row:world.transfer_market->rows)if(row){
            if(row->seller==choice.club){row.reset();continue;}
            std::array<MarketBid,4> remaining{};size_t count=0;for(const auto& bid:row->bids)if(bid.bidder.valid()&&bid.bidder!=choice.club)remaining[count++]=bid;row->bids=remaining;
        }
        for(auto& bidder:world.transfer_market->last_entry40_bidders)if(bidder==choice.club)bidder={};
    }
    ManagerDesktop desk;desk.seat=choice.seat;const auto address=layout::Desktops+uint32_t(choice.seat)*0x360;
    for(size_t row=0;row<54;++row)for(size_t byte=0;byte<16;++byte)desk.layout[row][byte]=reference.memory.u8(address+uint32_t(row*16+byte));
    const auto display=layout::CareerSession_AnyOf_ExecutableField+uint32_t(choice.seat)*2;desk.display_word=reference.memory.has(display+1)?reference.memory.u16(display):0;
    candidate.desktop->managers.push_back(desk);candidate.desktop->manager_order.push_back(choice.seat);candidate.desktop->initial_event->manager_order.push_back(choice.seat);
    candidate.watcom_seed=random.seed();candidate.validate();
    if(state!=before)throw FormatError("world12: running manager source changed during takeover");
    const auto draws=random.calls();state=std::move(candidate);return {choice.seat,draws};
}
}
