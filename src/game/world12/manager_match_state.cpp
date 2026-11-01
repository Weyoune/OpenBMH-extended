// SPDX-License-Identifier: GPL-3.0-only
#include "game/world12/manager_match_state.hpp"
#include <algorithm>
#include <stdexcept>
namespace bmh::world12 {
namespace {
constexpr size_t record_size=0xb0c;
uint16_t word(const Bytes& b,size_t at){return uint16_t(b.at(at)|uint16_t(b.at(at+1))<<8);}
void put_word(Bytes& b,size_t at,uint16_t value){b.at(at)=uint8_t(value);b.at(at+1)=uint8_t(value>>8);}
size_t record(const GameState& state,int manager) {
    if(manager<0||manager>3||state.manager_records.size()<size_t(manager+1)*record_size||state.manager_ratings.size()<size_t(manager+1)*60)
        throw std::invalid_argument("world12: incomplete manager match workspace");
    return size_t(manager)*record_size;
}
template<class Id> uint16_t local_of(const std::vector<Id>& map,Id id) {
    const auto found=std::find(map.begin(),map.end(),id);
    if(found==map.end()||size_t(found-map.begin())>=4095)throw std::invalid_argument("world12: manager reference is outside match workspace");
    return uint16_t(found-map.begin());
}
}
ManagerMatchState manager_match_state_from_legacy(const GameState& state,int manager,const std::vector<ClubId>& clubs,
    const std::vector<PlayerId>& players,const ManagerMatchState* previous) {
    const auto at=record(state,manager);ManagerMatchState result;if(previous)result.head_to_head=previous->head_to_head;
    std::copy_n(state.manager_records.begin()+at+0xc5,21,result.preferences.begin());
    result.tax_policy=state.manager_records[at+0x172];result.sponsor_win_bonus=state.manager_records[at+0x173];
    result.crowd_pressure=state.manager_records[at+0x289];result.goal_notices=state.manager_records[at+0x28b];result.point_deduction=state.manager_records[at+0x220];
    std::copy_n(state.manager_records.begin()+at+0x5e,21,result.best_streaks.begin());
    std::copy_n(state.manager_records.begin()+at+0x1ae,72,result.timing_stats.begin());
    for(size_t i=0;i<6;++i)result.staff_ratings[i]=word(state.manager_records,at+0x8aa+i*2);
    std::copy_n(state.manager_ratings.begin()+size_t(manager)*60,60,result.rating_history.begin());
    std::copy_n(state.manager_records.begin()+at+0x212,6,result.recent_scores.begin());
    for(size_t pair=0;pair<6;++pair) {
        const auto home=word(state.manager_records,at+0x292+pair*4),away=word(state.manager_records,at+0x294+pair*4);
        if(home==away)break; // Original drawer's terminator, not two phantom club-zero games.
        if(home>=clubs.size()||away>=clubs.size()||!clubs[home].valid()||!clubs[away].valid())throw std::invalid_argument("world12: unknown recent manager fixture");
        result.recent_fixtures[pair*2]=clubs[home];result.recent_fixtures[pair*2+1]=clubs[away];
    }
    const auto stride=size_t(runtime_dimensions.record_teams)*10;
    if(!clubs.empty()&&state.manager_results.size()<size_t(manager+1)*stride)throw std::invalid_argument("world12: incomplete manager opponent results");
    for(size_t i=0;i<clubs.size()&&i<size_t(runtime_dimensions.record_teams);++i)if(clubs[i].valid()) {
        std::array<uint8_t,10> scores;std::copy_n(state.manager_results.begin()+size_t(manager)*stride+i*10,10,scores.begin());
        if(std::all_of(scores.begin(),scores.end(),[](auto s){return s==255;}))result.head_to_head.erase(clubs[i]);
        else result.head_to_head[clubs[i]]=scores;
    }
    result.designated_takers=word(state.manager_records,at+0xb0a)==0x3253;
    if(result.designated_takers)for(size_t i=0;i<3;++i) {
        const auto id=word(state.manager_records,at+0xb04+i*2);if(!id)continue;
        if(id>4095||size_t(id-1)>=players.size()||!players[id-1].valid())throw std::invalid_argument("world12: unknown designated match player");
        result.takers[i]=players[id-1];
    }
    return result;
}
void install_manager_match_state(GameState& state,int manager,const ManagerMatchState& input,const std::vector<ClubId>& clubs,const std::vector<PlayerId>& players) {
    const auto at=record(state,manager);const auto stride=size_t(runtime_dimensions.record_teams)*10;
    if(state.manager_results.size()<size_t(manager+1)*stride)throw std::invalid_argument("world12: missing opponent result storage");
    // Resolve every reference before touching the workspace.
    std::array<uint16_t,12> fixtures{};std::array<uint16_t,3> takers{};
    bool ended=false;
    for(size_t i=0;i<12;i+=2) {
        if(input.recent_fixtures[i].valid()!=input.recent_fixtures[i+1].valid() || (input.recent_fixtures[i].valid()&&(ended||input.recent_fixtures[i]==input.recent_fixtures[i+1])))throw std::invalid_argument("world12: partial recent fixture");
        if(input.recent_fixtures[i].valid()){fixtures[i]=local_of(clubs,input.recent_fixtures[i]);fixtures[i+1]=local_of(clubs,input.recent_fixtures[i+1]);}
        else ended=true;
    }
    for(size_t i=0;i<3;++i)if(input.takers[i].valid()) {
        if(!input.designated_takers)throw std::invalid_argument("world12: designated player without marker");
        takers[i]=uint16_t(local_of(players,input.takers[i])+1);
    }
    std::copy(input.preferences.begin(),input.preferences.end(),state.manager_records.begin()+at+0xc5);
    state.manager_records[at+0x172]=input.tax_policy;state.manager_records[at+0x173]=input.sponsor_win_bonus;
    state.manager_records[at+0x289]=input.crowd_pressure;state.manager_records[at+0x28b]=input.goal_notices;state.manager_records[at+0x220]=input.point_deduction;
    std::copy(input.best_streaks.begin(),input.best_streaks.end(),state.manager_records.begin()+at+0x5e);
    std::copy(input.timing_stats.begin(),input.timing_stats.end(),state.manager_records.begin()+at+0x1ae);
    for(size_t i=0;i<6;++i)put_word(state.manager_records,at+0x8aa+i*2,input.staff_ratings[i]);
    std::copy(input.rating_history.begin(),input.rating_history.end(),state.manager_ratings.begin()+size_t(manager)*60);
    std::copy(input.recent_scores.begin(),input.recent_scores.end(),state.manager_records.begin()+at+0x212);
    for(size_t i=0;i<12;++i)put_word(state.manager_records,at+0x292+i*2,fixtures[i]);
    std::fill_n(state.manager_results.begin()+size_t(manager)*stride,stride,uint8_t(255));
    for(size_t i=0;i<clubs.size()&&i<size_t(runtime_dimensions.record_teams);++i)if(const auto found=input.head_to_head.find(clubs[i]);found!=input.head_to_head.end())
        std::copy(found->second.begin(),found->second.end(),state.manager_results.begin()+size_t(manager)*stride+i*10);
    put_word(state.manager_records,at+0xb0a,input.designated_takers?0x3253:0);
    for(size_t i=0;i<3;++i)put_word(state.manager_records,at+0xb04+i*2,takers[i]);
}
}
