// SPDX-License-Identifier: GPL-3.0-only
#include "game/season_awards.hpp"
#include "game/lineup.hpp"
#include <algorithm>
#include <numeric>
#include <stdexcept>
namespace bmh {
std::vector<size_t> rank_season_scorers(std::span<const SeasonScoringEntry> entries,uint8_t flags) {
    if(!(flags&6)||(flags&~uint8_t(7)))throw std::invalid_argument("season awards: invalid scoring flags");
    std::vector<size_t> order;
    const auto score=[&](size_t index){const auto& row=entries[index];return uint8_t((flags&2?row.goals:0)+(flags&4?row.assists:0));};
    for(size_t i=0;i<entries.size();++i) {
        if(entries[i].goals>63||entries[i].assists>63)throw std::invalid_argument("season awards: invalid Source counter");
        if(score(i))order.push_back(i);
    }
    std::stable_sort(order.begin(),order.end(),[&](size_t a,size_t b){return score(a)>score(b);});
    if(order.size()>10)order.resize(10);return order;
}
size_t season_fairplay_winner(std::span<const uint16_t> discipline) {
    if(discipline.empty()||discipline.size()>20)throw std::invalid_argument("season awards: invalid fair-play league");
    std::vector<int16_t> clubs(discipline.size());std::iota(clubs.begin(),clubs.end(),int16_t(0));
    std::vector<uint32_t> points;points.reserve(discipline.size());
    for(auto cards:discipline)points.push_back(uint32_t((int16_t(cards)>>8)*4+(cards&255)));
    quicksort_by_key(clubs.data(),points.data(),int(clubs.size()),true);return size_t(clubs.front());
}
int32_t season_award_prize(int league,int kind) {
    static constexpr uint8_t prizes[5][3]={{30,20,25},{20,15,17},{15,10,12},{15,10,12},{10,7,9}};
    if(league<0||league>=5||kind<0||kind>=3)throw std::invalid_argument("season awards: invalid prize category");
    return int32_t(prizes[league][kind])*10000;
}
}
