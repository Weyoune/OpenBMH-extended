// SPDX-License-Identifier: GPL-3.0-only
// Implementation of the job offer rule (job_offers.hpp explains the rule itself).
#include "game/world12/job_offers.hpp"
#include <algorithm>
#include <functional>
#include <set>
namespace bmh::world12 {
namespace {
// splitmix64: a fixed mixing function. It only orders equally suitable clubs; it is no random stream.
uint64_t mix(uint64_t x){
    x+=0x9e3779b97f4a7c15ull;x=(x^(x>>30))*0xbf58476d1ce4e5b9ull;x=(x^(x>>27))*0x94d049bb133111ebull;return x^(x>>31);
}
uint64_t key(uint32_t season,uint8_t seat,uint32_t item){return mix(mix(mix(season)^seat)^item);}
bool human(const World& world,ClubId club){
    return std::any_of(world.managers.begin(),world.managers.end(),[&](const auto& manager){return manager.club==club;});
}
// Clubs of a league ordered by team strength, strongest first; equal strengths keep the membership order.
std::vector<ClubId> by_strength(const World& world,LeagueId league){
    auto clubs=world.membership[league.value].clubs;
    std::stable_sort(clubs.begin(),clubs.end(),[&](ClubId a,ClubId b){return club_strength(world,a)>club_strength(world,b);});
    return clubs;
}
int place_of(const std::vector<ClubId>& order,ClubId club){
    const auto found=std::find(order.begin(),order.end(),club);
    return found==order.end()?0:int(found-order.begin())+1;
}
// Leagues of one country at one level that a human manager may lead.
std::vector<LeagueId> manageable_leagues(const World& world,Country country,uint8_t level){
    std::vector<LeagueId> out;
    for(auto league:world.catalogue.player_leagues(country))if(league_level(world,league)==level)out.push_back(league);
    return out;
}
// A club that may make an offer: a real club without a human manager whose loans fit a manager's loan board.
bool may_offer(const World& world,ClubId club,const std::set<ClubId>& taken){
    const auto& record=world.clubs.at(club);
    if(record.kind!=ClubKind::club||!record.league.valid()||human(world,club)||taken.contains(club))return false;
    return std::count_if(world.loans.begin(),world.loans.end(),[&](const auto& loan){return loan.owner==club;})<=3;
}
}

int club_strength(const World& world,ClubId id){
    const auto& club=world.clubs.at(id);
    std::vector<int> values;
    for(const auto& slot:club.roster){
        if(slot.empty())continue;
        const auto& person=world.players.at(slot.player).attributes;
        values.push_back((person.form()+person.technique()+person.condition())/3);
    }
    if(values.empty())return club.rating;
    std::sort(values.begin(),values.end(),std::greater<int>());
    if(values.size()>11)values.resize(11);
    int sum=0;for(int value:values)sum+=value;
    return sum/int(values.size());
}

uint8_t league_level(const World& world,LeagueId league){
    const auto& ladder=world.catalogue.rules(world.catalogue.league(league).country).ladder;
    for(size_t level=0;level<ladder.size();++level)
        if(std::find(ladder[level].leagues.begin(),ladder[level].leagues.end(),league)!=ladder[level].leagues.end())return uint8_t(level);
    throw FormatError("world12: league is not part of its country's pyramid");
}

JobOfferFacts judge_manager(const World& closing,const AlmanacSeason& table,const AnnualSeasonTitles& titles,
    const World& opening,uint8_t seat,bool sanctioned){
    const auto manager=std::find_if(closing.managers.begin(),closing.managers.end(),[&](const auto& m){return m.seat==seat;});
    if(manager==closing.managers.end())throw FormatError("world12: job offer judgement has no manager");
    JobOfferFacts facts;facts.seat=seat;facts.club=manager->club;facts.sanctioned=sanctioned;
    const auto& before=closing.clubs.at(manager->club);
    if(before.kind!=ClubKind::club||!before.league.valid())return facts;   // a national coach without a club
    const auto record=std::find_if(table.leagues.begin(),table.leagues.end(),[&](const auto& league){return league.league==before.league;});
    if(record==table.leagues.end())return facts;
    for(size_t i=0;i<record->table.size();++i)if(record->table[i].club==manager->club)facts.final_place=int(i)+1;
    facts.expected_place=place_of(by_strength(closing,before.league),manager->club);
    if(!facts.final_place||!facts.expected_place)return facts;
    facts.champion=facts.final_place==1;
    for(const auto& row:titles.titles)if(row.kind==AnnualTitleKind::domestic&&row.winner==manager->club)facts.cup_winner=true;
    const auto& after=opening.clubs.at(manager->club);
    if(!after.league.valid())return facts;
    const int old_level=league_level(closing,before.league),new_level=league_level(opening,after.league);
    facts.promoted=new_level<old_level;facts.relegated=new_level>old_level;
    const int performance=facts.expected_place-facts.final_place;
    if(facts.sanctioned||facts.relegated||performance<=-6)facts.offer_class=JobOfferFacts::Class::none;
    else if(facts.champion||facts.promoted||facts.cup_winner||performance>=4)facts.offer_class=JobOfferFacts::Class::top;
    else facts.offer_class=JobOfferFacts::Class::ordinary;
    return facts;
}

SortedMap<uint8_t,std::vector<JobOffer>> make_job_offers(const World& closing,const AlmanacSeason& table,
    const AnnualSeasonTitles& titles,const World& opening,uint32_t target,const std::array<bool,4>& sanctioned){
    SortedMap<uint8_t,std::vector<JobOffer>> out;
    std::set<ClubId> taken;
    std::vector<uint8_t> seats;for(const auto& manager:closing.managers)seats.push_back(manager.seat);
    std::sort(seats.begin(),seats.end());
    for(const auto seat:seats){
        if(seat>3)continue;
        const auto facts=judge_manager(closing,table,titles,opening,seat,sanctioned[seat]);
        if(facts.offer_class==JobOfferFacts::Class::none)continue;
        const auto& own=opening.clubs.at(facts.club);
        const auto level=league_level(opening,own.league);
        const int own_strength=club_strength(opening,facts.club);
        // Takes up to `count` clubs from `pool` in the fixed mixed order and appends them to the offers.
        std::vector<JobOffer> offers;
        const auto take=[&](std::vector<ClubId> pool,size_t count,uint32_t salt){
            pool.erase(std::remove_if(pool.begin(),pool.end(),[&](ClubId club){return club==facts.club||!may_offer(opening,club,taken);}),pool.end());
            std::sort(pool.begin(),pool.end(),[&](ClubId a,ClubId b){
                const auto ka=key(target,seat,a.value^salt),kb=key(target,seat,b.value^salt);return ka!=kb?ka<kb:a<b;});
            for(size_t i=0;i<pool.size()&&count&&offers.size()<kMaximumJobOffers;++i,--count){offers.push_back({pool[i]});taken.insert(pool[i]);}
        };
        // Peers: clubs of the own division within four strength places of the own club.
        const auto peers=[&]{
            std::vector<ClubId> pool;
            for(auto league:manageable_leagues(opening,own.country,level)){
                const auto order=by_strength(opening,league);
                // The own club's place is counted in its own league; in a parallel league of the same level
                // the place of the first club that is not stronger stands in for it.
                int own_place=place_of(order,facts.club);
                if(!own_place){own_place=1;for(auto club:order)if(club_strength(opening,club)>own_strength)++own_place;}
                for(size_t i=0;i<order.size();++i)if(std::abs(int(i)+1-own_place)<=4)pool.push_back(order[i]);
            }
            return pool;
        };
        if(facts.offer_class==JobOfferFacts::Class::top){
            std::vector<ClubId> above;
            if(level>0){
                for(auto league:manageable_leagues(opening,own.country,uint8_t(level-1))){
                    const auto order=by_strength(opening,league);
                    above.insert(above.end(),order.begin()+std::ptrdiff_t(order.size()/2),order.end());   // the weaker half
                }
            }else{
                for(auto league:manageable_leagues(opening,own.country,0))
                    for(auto club:by_strength(opening,league))if(club_strength(opening,club)>own_strength)above.push_back(club);
            }
            take(std::move(above),2,0x1000000u);
            take(peers(),kMaximumJobOffers-offers.size(),0x2000000u);
        }else take(peers(),1+size_t(key(target,seat,0x3000000u)&1),0x2000000u);
        // One season in three: the last offer comes from the other country instead.
        const auto other=own.country==Country::germany?Country::england:Country::germany;
        if(!offers.empty()&&opening.catalogue.selectable(Country::germany)&&opening.catalogue.selectable(Country::england)&&key(target,seat,0x4000000u)%3==0){
            uint8_t deepest=0;for(auto league:opening.catalogue.player_leagues(other))deepest=std::max(deepest,league_level(opening,league));
            std::vector<ClubId> abroad;
            for(auto league:manageable_leagues(opening,other,std::min(level,deepest)))
                for(auto club:opening.membership[league.value].clubs)if(may_offer(opening,club,taken))abroad.push_back(club);
            // The six clubs closest in strength, then the fixed mixed order among them.
            std::stable_sort(abroad.begin(),abroad.end(),[&](ClubId a,ClubId b){
                return std::abs(club_strength(opening,a)-own_strength)<std::abs(club_strength(opening,b)-own_strength);});
            if(abroad.size()>6)abroad.resize(6);
            if(!abroad.empty()){
                taken.erase(offers.back().club);offers.pop_back();
                take(std::move(abroad),1,0x5000000u);
            }
        }
        if(!offers.empty())out.emplace(seat,std::move(offers));
    }
    return out;
}
}
