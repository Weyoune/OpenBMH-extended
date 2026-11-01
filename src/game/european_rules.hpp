// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/random.hpp"
#include <array>
#include <stdexcept>
#include <vector>
namespace bmh {
// Initial four-nation tournament (season zero). The host has Source index 0.
inline std::array<uint8_t,4> initial_national_four(GameRandom& game,WatcomRandom& random) {
    std::array<uint8_t,4> field{};size_t count=1;
    int left=random.range(0,2);
    for(int pool=0;pool<2;++pool) {
        if(pool==1)left=4-int(count);
        while(left){const auto country=uint8_t(random.range(pool?15:0,pool?31:14));bool found=false;
            for(size_t i=0;i<count;++i)found|=field[i]==country;
            if(!found){field[count++]=country;--left;}}
    }
    for(size_t i=0;i<4;++i){const auto j=size_t(game.range(0,3));if(j>=4)throw std::invalid_argument("season: invalid initial national draw");std::swap(field[i],field[j]);}
    return field;
}
// LeagueEntrants in identity-neutral form. Holders are [UEFA,CL,CW].
// find returns zero when absent, otherwise the Source competition index + 1.
template<class Id,class Find,class Add>
void european_league_entrants(const std::vector<Id>& ranks,Id winner,Id finalist,
    const std::array<Id,3>& holders,const std::array<bool,3>& in_league,int uefa,Find find,Add add) {
    if(ranks.size()<2||uefa<0)throw std::invalid_argument("season: invalid European league ranking");
    add(1,ranks[0]);
    if(in_league[1])add(1,find(holders[1])==0?holders[1]:ranks[1]);
    if(find(winner)==0)add(2,winner);else if(find(finalist)==0)add(2,finalist);
    if(in_league[2]){if(find(holders[2])==0)add(2,holders[2]);else if(find(finalist)==0)add(2,finalist);}
    if(in_league[0]){if(find(holders[0])==0)add(0,holders[0]);else ++uefa;}
    for(size_t rank=1;uefa;++rank){if(rank>=ranks.size())throw std::invalid_argument("season: European league quota exceeds ranking");if(find(ranks[rank])==0){add(0,ranks[rank]);--uefa;}}
}
// Exact Original loop bounds, including the discarded UEFA scratch tail.
// Callbacks own identities/storage; no packed reference or address is required.
template<class Count,class Pick,class Add,class Shuffle>
void fill_initial_european_fields(Count count,Pick pick,Add add,Shuffle shuffle) {
    int group=0;
    for(int i=count(2);i<32;++i,++group)add(2,pick(group));
    shuffle(2,0,32);
    group=0;
    for(int i=count(1);i<24;++i,++group)add(1,pick(group));
    shuffle(1,8,16);
    group=0;const int start=count(0);int per=2;
    for(int i=start;i<64;++i,++group){for(int k=0;k<per;++k)add(0,pick(group));if(i-start==16)per=1;}
    shuffle(0,0,64);
}
}
