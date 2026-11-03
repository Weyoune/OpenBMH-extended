// SPDX-License-Identifier: GPL-3.0-only
#include "game/world12/league_history.hpp"
#include "game/world12/standings.hpp"
#include "game/matchday.hpp"
#include <algorithm>
#include <limits>
#include <set>
namespace bmh::world12 {
namespace {
void require(bool value,const char* message){if(!value)throw FormatError(std::string("world12: league history ")+message);}
GameState workspace(const World& world,const Manager& manager,int rank,uint32_t season){
    const auto& club=world.clubs.at(manager.club);const auto& league=world.catalogue.league(club.league);
    GameState game;game.season=int(std::min(season,49u));game.manager_teams={0};game.manager_records.resize(0xb0c);game.team_morale={club.morale};game.squad_slots.assign(24,kEmptySlot);
    game.projected_match_clubs.push_back({int(league.local_index),int(league.country),3});game.projected_match_clubs[0].table_rank=rank;game.projected_match_clubs[0].league_size=league.clubs;
    game.manager_records[0x210]=league.local_index;
    for(size_t row=0;row<24;++row){const auto& slot=club.roster[row];if(slot.empty())continue;const auto local=uint16_t(game.players.size());game.players.push_back(world.players.at(slot.player).attributes);game.squad_slots[row]=uint16_t(local|((slot.flags&1)?0x8000:0)|((slot.flags&2)?0x4000:0)|((slot.flags&4)?0x2000:0)|((slot.flags&8)?0x1000:0));}
    const auto& career=*manager.career;
    if(career.season_records)std::copy(career.season_records->begin(),career.season_records->end(),game.manager_records.begin()+0xda);
    if(career.season_services){const auto& history=*career.season_services;game.manager_records[0x284]=history.strength_history_count;std::copy(history.strength_history.begin(),history.strength_history.end(),game.manager_records.begin()+0x222);std::copy(history.position_history.begin(),history.position_history.end(),game.manager_records.begin()+0x248);}
    return game;
}
}
uint8_t league_history_position(const League& league,uint8_t rank){
    require(league.active&&rank<league.clubs&&league.local_index<5,"position is outside an actual domestic league");
    if(league.country==Country::germany){const auto tier=league.local_index==3?2:league.local_index==4?3:league.local_index;return uint8_t(tier*18+rank+1);}
    // OpenBMH deviates from the original: FLTD's inherited literal18 collapse
    // aliases its fourth20-club tier with the third. Keep actual English bands
    // distinct; Legacy behavior remains unchanged and internal tier4 stays real.
    require(league.country==Country::england,"position has no domestic country owner");return uint8_t(league.local_index*20+rank+1);
}
void record_completed_league_history(CareerSnapshot& state,const CareerSnapshot& before,const std::vector<CompletedLeagueRound>& completed){
    require(state.season==before.season&&state.date==before.date&&state.calendar==before.calendar&&state.world.catalogue==before.world.catalogue&&state.world.membership==before.world.membership,"day/season/membership owner changed before publication");
    state.world.validate();state.results.validate(state.world.catalogue);before.world.validate();before.results.validate(before.world.catalogue);
    auto candidate=state;std::set<LeagueId> seen;
    for(const auto& round:completed){
        const auto& league=state.world.catalogue.league(round.league);require(seen.insert(round.league).second&&round.round==before.league_rounds.at(round.league.value)+1&&round.round==state.league_rounds.at(round.league.value)&&round.results.size()==size_t(league.clubs/2),"round is stale, repeated or incomplete");
        for(int r=1;r<=league.rounds();++r)for(int m=0;m<league.clubs/2;++m){const auto& actual=state.results.result(state.world.catalogue,round.league,r,m);const auto& old=before.results.result(before.world.catalogue,round.league,r,m);if(r==round.round)require(!old.played&&actual==round.results[size_t(m)]&&actual.played,"real completed score receipt is missing or already consumed");else require(actual==old,"unrelated league results changed");}
        if(!std::any_of(candidate.world.managers.begin(),candidate.world.managers.end(),[&](const auto& manager){return candidate.world.clubs.at(manager.club).league==round.league;}))continue;
        TableQuery query;for(const auto& manager:candidate.world.managers)if(manager.match_state&&candidate.world.clubs.at(manager.club).league==round.league){const auto cut=manager.match_state->point_deduction;if((cut>>5)==3)query.point_deductions[manager.club]=cut&31;}
        const auto table=compute_standings(candidate.world,candidate.results,round.league,round.round,query);
        for(auto club:candidate.world.membership[round.league.value].clubs){
            const auto found=std::find_if(candidate.world.managers.begin(),candidate.world.managers.end(),[&](const auto& manager){return manager.club==club;});if(found==candidate.world.managers.end()||!found->career)continue;
            auto& manager=*found;auto& career=*manager.career;if(!career.season_records&&!career.season_services)continue;
            const auto prior=std::find_if(before.world.managers.begin(),before.world.managers.end(),[&](const auto& old){return old.seat==manager.seat;});require(prior!=before.world.managers.end()&&prior->club==club&&prior->career,"canonical manager owner changed");
            const auto& old=*prior->career;require(career.season_records.has_value()==old.season_records.has_value()&&career.season_services.has_value()==old.season_services.has_value(),"known history ownership changed");
            if(career.season_records)for(size_t season=0;season<50;++season)require((*career.season_records)[season*3]==(*old.season_records)[season*3],"annual position history was already written or changed");
            if(career.season_services)require(career.season_services->strength_history_count==old.season_services->strength_history_count&&career.season_services->strength_history==old.season_services->strength_history&&career.season_services->position_history==old.season_services->position_history,"strength/rank history was already written or changed");
            const auto position=std::find_if(table.begin(),table.end(),[&](const auto& row){return row.club==club;});require(position!=table.end()&&position->rank>0,"completed table lost manager rank");const auto rank=uint8_t(position->rank-1);
            auto native=workspace(candidate.world,manager,rank,state.season);
            if(career.season_records){bmh::store_season_position(native,0,league_history_position(league,rank));std::copy_n(native.manager_records.begin()+0xda,150,career.season_records->begin());}
            if(career.season_services){bmh::store_strength_history(native,0);auto& history=*career.season_services;history.strength_history_count=native.manager_records[0x284];std::copy_n(native.manager_records.begin()+0x222,38,history.strength_history.begin());std::copy_n(native.manager_records.begin()+0x248,38,history.position_history.begin());}
        }
    }
    candidate.world.validate();state=std::move(candidate);
}
}
