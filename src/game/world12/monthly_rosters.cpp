// SPDX-License-Identifier: GPL-3.0-only
#include "game/world12/monthly_rosters.hpp"
#include "game/world12/economy.hpp"
#include "game/world12/market_bids.hpp"
#include "game/month.hpp"
#include "game/season.hpp"
#include "game/schedule.hpp"
#include <algorithm>
#include <set>
namespace bmh::world12 {
namespace {
void forget_loans(CareerSnapshot& state,const std::vector<std::pair<PlayerId,ClubId>>& released){
    const auto removed=[&](PlayerId id){return std::any_of(released.begin(),released.end(),[&](const auto& row){return row.first==id;});};
    auto& loans=state.world.loans;loans.erase(std::remove_if(loans.begin(),loans.end(),[&](const auto& row){return removed(row.player);}),loans.end());
    if(state.transfers){auto& rows=state.transfers->loans;rows.erase(std::remove_if(rows.begin(),rows.end(),[&](const auto& row){return removed(row.player);}),rows.end());}
}
}
MonthlyRosterOutcome settle_monthly_roster(CareerSnapshot& state,uint8_t seat,const GameState& reference,const TextDb& texts,const MonthlyContractHooks& hooks){
    state.validate();if(!state.world.economy||!state.world.settings||!state.world.free_players||seat>=4||!state.world.economy->managers[seat].monthly)
        throw FormatError("world12: monthly roster accounting state is unavailable");
    const auto before=state;auto candidate=state;assign_transfer_loan_slots(candidate);
    std::vector<ClubId> clubs;int manager=-1;for(const auto& m:state.world.managers){if(m.seat==seat)manager=int(clubs.size());clubs.push_back(m.club);}
    if(manager<0)throw FormatError("world12: monthly roster manager is not active");
    const auto& settings=*state.world.settings;auto p=AiMatchProjection::create_for_humans(candidate,clubs,reference,{settings.mode,settings.difficulty,settings.foreign_rule,true});
    install_career_economy(p,candidate.world);install_transfer_loans(p,candidate);
    std::vector<PlayerId> free_players;for(auto id:candidate.world.free_players->domestic)if(id.valid())free_players.push_back(id);for(auto id:candidate.world.free_players->foreign)if(id.valid())free_players.push_back(id);p.include_players(candidate.world,free_players);
    auto& g=p.state;g.texts=&texts;
    const auto install_pool=[&](const auto& pool,size_t first){for(size_t row=0;row<pool.size();++row)g.squad_slots.at(first+row)=pool[row].valid()?uint16_t(p.local_player(pool[row])):0xfff;};
    install_pool(candidate.world.free_players->domestic,size_t(runtime_dimensions.free_first));install_pool(candidate.world.free_players->foreign,size_t(runtime_dimensions.foreign_free_first));
    GameRandom game(state.game_seed);WatcomRandom random(state.watcom_seed);
    g.contract_screen=[&](GameState& current,int local_manager,int row){
        if(!hooks.renew)throw FormatError("world12: expired contract needs an owned renewal screen");
        const auto slot=size_t(current.manager_teams.at(size_t(local_manager))*24+row);const auto player=p.global_player(current.squad_slots.at(slot)&0xfff);
        const auto answer=hooks.renew(current,game,random,p.global_manager(uint8_t(local_manager)),player,row,p);
        if(!answer)throw FormatError("world12: monthly contract decision cancelled");
        if(*answer&&!current.tactics.at(size_t(local_manager)*0x840+size_t(row)*0x58+0xc))throw FormatError("world12: renewal screen did not renew the contract");
        return int(*answer);
    };
    auto working=candidate;std::set<ClubId> changed_ai;std::vector<std::pair<PlayerId,ClubId>> released;
    const auto synchronize=[&]{
        for(size_t local=0;local<clubs.size();++local){auto& roster=working.world.clubs.at(clubs[local]).roster;for(size_t row=0;row<24;++row){const auto raw=g.squad_slots.at(local*24+row);roster[row]=(raw&0xfff)==0xfff?RosterSlot{}:RosterSlot{p.global_player(raw&0xfff),uint16_t(((raw&0x8000)?1:0)|((raw&0x4000)?2:0)|((raw&0x2000)?4:0)|((raw&0x1000)?8:0))};}}
        for(size_t local=0;local<g.players.size();++local){auto& player=working.world.players.at(p.global_player(uint32_t(local)));player.attributes=g.players[local];player.bonus=g.player_bonus[local];player.cards=g.player_cards[local];}
        const auto capture_pool=[&](auto& pool,size_t first){for(size_t row=0;row<pool.size();++row){const auto raw=g.squad_slots.at(first+row)&0xfff;pool[row]=raw==0xfff?PlayerId{}:p.global_player(raw);}};
        capture_pool(working.world.free_players->domestic,size_t(runtime_dimensions.free_first));capture_pool(working.world.free_players->foreign,size_t(runtime_dimensions.foreign_free_first));forget_loans(working,released);
    };
    PlayerReleaseDestination destination;
    destination.pool_slot=[&](const GameState& current,int player){const auto& person=current.players.at(size_t(player));const auto nationality=person.nationality()+((current.player_bonus.at(size_t(player))&128)?32:0);
        const bool domestic=candidate.world.catalogue.scope.domestic_enabled(Country(nationality));const auto first=domestic?runtime_dimensions.free_first:runtime_dimensions.foreign_free_first;const auto count=domestic?80:20;
        for(int n=0;n<count;++n)if(current.squad_slots.at(size_t(first+n))==0xfff)return first+n;return -1;
    };
    destination.overflow=[&](GameState& current,int slot,GameRandom&,WatcomRandom& wr,MinuteReport&){
        synchronize();const auto player=p.global_player(current.squad_slots.at(size_t(slot))&0xfff);auto& roster=working.world.clubs.at(p.global_club(uint32_t(slot/24))).roster;
        const auto found=std::find_if(roster.begin(),roster.end(),[&](const auto& row){return row.player==player;});if(found==roster.end())throw FormatError("world12: release overflow lost source owner");std::move(found+1,roster.end(),found);roster.back()={};
        const auto target=place_unlisted_player(working,player,reference,wr,{true});changed_ai.insert(target);
        // The destination lineup can update card state of a referenced player.
        for(size_t local=0;local<current.players.size();++local){const auto id=p.global_player(uint32_t(local));const auto& person=working.world.players.at(id);current.players[local]=person.attributes;current.player_bonus[local]=person.bonus;current.player_cards[local]=person.cards;}
    };
    MonthlyPlayerRelease release=[&](GameState& current,int slot,GameRandom& gr,WatcomRandom& wr,MinuteReport& report){const auto id=p.global_player(current.squad_slots.at(size_t(slot))&0xfff);released.push_back({id,p.global_club(uint32_t(slot/24))});return release_player(current,slot,gr,wr,report,&destination);};
    monthly_manager_accounts(g,manager);MinuteReport report;monthly_manager_board(g,manager,random,report,&game,release);if(report.unsupported)throw FormatError(report.reason);
    synchronize();forget_loans(candidate,released);
    if(released.empty())p.commit_match_state(candidate.world,true);else p.commit_released_players(candidate.world,released);
    candidate.world.free_players=working.world.free_players;
    for(auto id:changed_ai){candidate.world.clubs.at(id)=working.world.clubs.at(id);for(const auto& row:working.world.clubs.at(id).roster)if(!row.empty())candidate.world.players.at(row.player)=working.world.players.at(row.player);}
    capture_career_economy(candidate.world,p);candidate.game_seed=game.seed();candidate.watcom_seed=random.seed();candidate.validate();
    if(state!=before)throw FormatError("world12: monthly roster owner changed during contract screen");
    const MonthlyRosterOutcome outcome{game.calls(),random.calls(),released.size()};state=std::move(candidate);return outcome;
}
}
