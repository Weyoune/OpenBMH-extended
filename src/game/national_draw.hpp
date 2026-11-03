// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/european_rules.hpp"
#include <algorithm>
#include <optional>
namespace bmh {
// Identity-neutral Source tournament selection. The caller owns previous
// results; new countries use Original0..61, never global club IDs. Inactive Native
// campaigns retain previous opaque scratch words; typed callers require known
// completed predecessors before invoking this kernel.
inline std::vector<uint16_t> draw_national_tournament_field(uint32_t season,
    const std::vector<uint16_t>& previous,std::optional<uint16_t> winner,
    std::optional<uint16_t> runner,GameRandom& game,WatcomRandom& random) {
    const int cycle=int(season%4);
    if(!cycle){const auto field=initial_national_four(game,random);return {field.begin(),field.end()};}
    static constexpr uint8_t fixed[16]={2,0,0,0,7,0,0,0,3,0,0,0,10,4,8,0};
    static constexpr uint8_t sizes[4]={4,8,6,24},from[4]={0,15,32,47},to[4]={15,32,47,62};
    std::vector<uint16_t> field;
    if(cycle%2){
        if(!winner||previous.empty()||(cycle==3&&!runner))
            throw std::invalid_argument("national draw: previous field and winners are required");
        field.push_back(*winner);if(cycle==3)field.push_back(*runner);
    }else field.push_back(0);
    for(int pool=0;pool<4;++pool){int left=fixed[pool+cycle*4]?random.range(fixed[pool+cycle*4]-2,fixed[pool+cycle*4]):int(sizes[cycle])-int(field.size());
        while(int16_t(left)!=0){const auto country=uint8_t(random.range(from[pool],to[pool]-1));
            if(std::find(field.begin(),field.end(),country)!=field.end()||
               (cycle%2&&std::find(previous.begin(),previous.end(),country)!=previous.end()))continue;
            field.push_back(country);--left;
        }
    }
    if(field.size()!=sizes[cycle])throw std::invalid_argument("national draw: Source field size disagrees with programme");
    for(size_t i=0;i<field.size();++i){const auto j=size_t(game.range(0,int(field.size())-1));std::swap(field[i],field.at(j));}
    return field;
}
}
