// SPDX-License-Identifier: GPL-3.0-only
#include "game/world12/european.hpp"
#include "game/european_rules.hpp"
#include <algorithm>
#include <set>
#include <stdexcept>
namespace bmh::world12 {
InitialCompetitionFields draw_initial_shared_fields(const World& world,GameRandom& game,WatcomRandom& random) {
    world.validate();if(!world.catalogue.scope.club_play())throw std::invalid_argument("world12: shared club fields require club play");
    auto staged_game=game;auto staged_random=random;InitialCompetitionFields result;
    const auto nations=bmh::initial_national_four(staged_game,staged_random);
    if(world.catalogue.scope.international)for(auto country:nations){ClubId found;for(const auto& [id,c]:world.clubs)if(c.kind==ClubKind::national_team&&uint8_t(c.country)==country){if(found.valid())throw std::invalid_argument("world12: duplicate national identity");found=id;}
        if(!found.valid())throw std::invalid_argument("world12: missing initial national identity");result.national_field.push_back(found);}
    std::vector<LeagueId> leagues;leagues.push_back(world.catalogue.starting_league(world.catalogue.scope.germany?Country::germany:Country::england,0));
    for(uint32_t l:{10u,5u,11u,12u})if(world.catalogue.league({l}).active && std::find(leagues.begin(),leagues.end(),LeagueId{l})==leagues.end())leagues.push_back({l});
    if(leagues.size()<4)throw std::invalid_argument("world12: missing European top leagues");
    result.holders={world.membership[leagues[2].value].clubs.at(7),world.membership[leagues[3].value].clubs.at(7),world.membership[leagues[1].value].clubs.at(7)};
    for(auto country:{Country::germany,Country::england})if(world.catalogue.selectable(country)){const auto& ranked=world.membership[world.catalogue.starting_league(country,0).value].clubs;result.champions[country]=ranked[0];result.domestic_holders[country]=ranked.at(7);}
    const auto find=[&](ClubId id){for(size_t k=0;k<3;++k)if(std::find(result.qualified[k].begin(),result.qualified[k].end(),id)!=result.qualified[k].end())return int(k)+1;return 0;};
    const auto add=[&](int kind,ClubId id){result.qualified[size_t(kind)].push_back(id);};
    // Preserve nineteen regular league UEFA places. If Germany is disabled,
    // its four places move one each to the four surviving top leagues.
    const int extra=world.catalogue.scope.germany?0:1;
    for(size_t i=0;i<leagues.size();++i){const auto league=leagues[i];const auto& ranks=world.membership[league.value].clubs;
        ClubId winner,finalist;
        if(world.catalogue.league(league).playable){winner=ranks.at(7);finalist=ranks[0];}
        else{const auto a=size_t(staged_random.range(0,int(ranks.size())-1));size_t b;do b=size_t(staged_random.range(0,int(ranks.size())-1));while(b==a);winner=ranks.at(a);finalist=ranks.at(b);}
        std::array<bool,3> in{};for(size_t h=0;h<3;++h)in[h]=world.clubs.at(result.holders[h]).league==league;
        const int quota=(world.catalogue.league(league).country==Country::italy?3:4)+extra;
        bmh::european_league_entrants(ranks,winner,finalist,result.holders,in,quota,find,add);
    }
    for(size_t kind=0;kind<3;++kind)if(!find(result.holders[kind]))add(int(kind),result.holders[kind]);
    if(!world.catalogue.scope.germany){
        // Germany's absent cup-winner place belongs to the primary country.
        // Keep it distinct from the redistributed UEFA league places.
        bool assigned=false;for(auto id:world.membership[leagues[0].value].clubs)if(!find(id)){add(2,id);assigned=true;break;}
        if(!assigned)throw std::invalid_argument("world12: missing replacement domestic cup-winner place");
    }
    std::array<std::vector<ClubId>,27> pools;
    for(const auto& [id,c]:world.clubs)if(c.kind==ClubKind::club&&!c.league.valid()&&uint8_t(c.country)>=5&&uint8_t(c.country)<32)pools[uint8_t(c.country)-5].push_back(id);
    for(const auto& pool:pools)if(pool.size()!=7)throw std::invalid_argument("world12: European reserve country must contain seven clubs");
    // Source overfills a scratch UEFA tail beyond the 64 published entrants.
    // Tokens in that tail must never alias newly added real global club IDs.
    using Token=uint64_t;constexpr Token virtual_tag=Token(1)<<63;
    std::array<std::vector<Token>,3> lists;
    for(size_t k=0;k<3;++k)for(auto id:result.qualified[k])lists[k].push_back(id.value);
    const auto contains=[&](Token token){for(const auto& list:lists)if(std::find(list.begin(),list.end(),token)!=list.end())return true;return false;};
    bmh::fill_initial_european_fields([&](int k){return int(lists[size_t(k)].size());},
        [&](int group){Token token;do{const auto slot=size_t(staged_random.range(0,6));token=group<27?pools[size_t(group)].at(slot).value:virtual_tag|Token(group*7+int(slot));}while(contains(token));return token;},
        [&](int k,Token token){lists[size_t(k)].push_back(token);},
        [&](int k,int first,int count){auto& list=lists[size_t(k)];for(int i=0;i<count;++i){const auto j=staged_game.range(0,count-1);if(j<0||j>=count)throw std::invalid_argument("world12: invalid European draw index");std::swap(list.at(size_t(first+i)),list.at(size_t(first+j)));}});
    const size_t sizes[]={64,24,32};
    for(size_t k=0;k<3;++k)for(size_t i=0;i<sizes[k];++i){const auto token=lists[k].at(i);if(token&virtual_tag)throw std::invalid_argument("world12: European quota leaves a non-club entrant");result.fields[k].push_back({uint32_t(token)});}
    game=staged_game;random=staged_random;return result;
}
}
