// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <algorithm>
#include <optional>
#include <stdexcept>
#include <vector>

namespace bmh {
// Source promotion ordering operates on draw slots, whose occupants can change
// between groups. Callers supply the frozen final rank order of those slots.
template<class Club> void append_relegation_places(std::vector<Club>& down,
    const std::vector<Club>& ranked,int remaining) {
    for(auto i=ranked.rbegin();remaining!=0&&i!=ranked.rend();++i)
        if(std::find(down.begin(),down.end(),*i)==down.end()) {
            down.push_back(*i);--remaining;
        }
    if(remaining!=0)throw std::invalid_argument("season: relegation places exceed ranked clubs");
}

template<class Club> struct PromotionCandidates {
    size_t count=0;
    std::vector<Club> clubs;
};
// A parallel feeder with no play-off exchanges only its champions. With a
// play-off, Source orders each winner before that feeder's champion.
template<class Club> PromotionCandidates<Club> select_promotion_candidates(
    size_t count,const std::vector<std::vector<Club>>& feeders,
    const std::optional<std::vector<Club>>& playoff_winners={}) {
    if(feeders.empty())throw std::invalid_argument("season: missing promotion feeder");
    PromotionCandidates<Club> out{count,{}};
    size_t per=count/feeders.size();
    for(size_t league=0;league<feeders.size();++league) {
        if(feeders.size()>1) {
            if(playoff_winners) {
                if(playoff_winners->size()!=feeders.size())
                    throw std::invalid_argument("season: incomplete promotion play-off winners");
                out.clubs.push_back(playoff_winners->at(league));
            }else out.count=2; // Original's paired Regionalligen, not a quota inference.
            per=1;
        }
        if(per>feeders[league].size())throw std::invalid_argument("season: insufficient ranked promotion clubs");
        out.clubs.insert(out.clubs.end(),feeders[league].begin(),feeders[league].begin()+per);
    }
    return out;
}
}
