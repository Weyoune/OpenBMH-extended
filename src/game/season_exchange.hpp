// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/random.hpp"
#include <algorithm>
#include <bit>
#include <functional>
#include <optional>
#include <stdexcept>
#include <vector>

namespace bmh {
struct SeasonLicenceAccount {
    uint8_t creditworthiness=0,sanction=0;
    int32_t assets=0,remaining_loans=0,transfer_balance=0;
    bool operator==(const SeasonLicenceAccount&)const=default;
};
inline bool season_licence_broke(const SeasonLicenceAccount& account) {
    const auto total=std::bit_cast<int32_t>(uint32_t(account.creditworthiness)*80000u+
        uint32_t(account.assets)-uint32_t(account.remaining_loans));
    return total < -6000000 || ((account.sanction>>5)==2&&
        int32_t(account.sanction&31)*100000<=account.transfer_balance);
}
// Source samples draw positions until an AI club at zero-based rank10..15 is
// found. Flattening the two rejection loops preserves every Watcom draw.
template<class Human> std::optional<size_t> select_ai_licence_withdrawal(
    int local_league,size_t managers,const std::vector<uint8_t>& ranks,Human human,
    GameRandom& game,WatcomRandom& random) {
    if(managers>=6||local_league>=2)return {};
    if(local_league<0||ranks.empty())throw std::invalid_argument("season: invalid licence league");
    if(game.range(0,12-7*local_league)!=0)return {};
    bool eligible=false;
    for(size_t i=0;i<ranks.size();++i)eligible|=!human(i)&&ranks[i]>=10&&ranks[i]<=15;
    if(!eligible)throw std::invalid_argument("season: no eligible AI licence withdrawal");
    size_t selected;
    do selected=size_t(random.range(0,int(ranks.size())-1));
    while(human(selected)||ranks[selected]<10||ranks[selected]>15);
    return selected;
}
// Pool entries represent physical amateur slots, not a sorted stable-ID set.
// Trim the unnamed tail exactly as the existing Original edition adapter does.
template<class Club,class Named> std::vector<Club> draw_season_newcomers(
    size_t count,std::vector<Club> pool,Named named,WatcomRandom& random) {
    while(pool.size()>1&&!named(pool.back()))pool.pop_back();
    const auto available=std::count_if(pool.begin(),pool.end(),named);
    if(count>size_t(available))throw std::invalid_argument("season: insufficient named newcomer slots");
    std::vector<Club> out;
    for(size_t i=0;i<count;++i) {
        Club selected;
        do selected=pool.at(size_t(random.range(0,int(pool.size())-1)));
        while(std::find(out.begin(),out.end(),selected)!=out.end()||!named(selected));
        out.push_back(selected);
    }
    return out;
}

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
