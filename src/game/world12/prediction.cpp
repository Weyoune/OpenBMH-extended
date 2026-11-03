// SPDX-License-Identifier: GPL-3.0-only
#include "game/world12/prediction.hpp"
#include "game/world12/standings.hpp"
#include "game/world12/rescheduling.hpp"
#include <algorithm>
#include <stdexcept>
namespace bmh::world12 {
LeaguePrediction predict_league(const CareerSnapshot& state,ClubId id,int mode,const std::map<ClubId,uint8_t>& deductions) {
    state.validate();const auto club=state.world.clubs.find(id);
    if(club==state.world.clubs.end() || !club->second.league.valid() || mode<0 || mode>255)
        throw std::invalid_argument("world12: unavailable league prediction context");
    const auto league_id=club->second.league;const auto& league=state.world.catalogue.league(league_id);
    if(!league.active)throw std::invalid_argument("world12: disabled league prediction");
    LeaguePrediction prediction;prediction.result.ranks.fill(1);
    const int rounds=visible_league_round(state,league_id);if(rounds<2 || rounds>=league.rounds())return prediction;
    const auto& members=state.world.membership[league_id.value].clubs;
    const auto found=std::find(members.begin(),members.end(),id);
    if(found==members.end())throw std::invalid_argument("world12: prediction club absent from its league");
    const int position=int(found-members.begin());int europe=0;
    if(league.playable) {
        const auto& groups=state.world.catalogue.rules(league.country).ladder;
        const auto group=std::find_if(groups.begin(),groups.end(),[&](const auto& value){return std::find(value.leagues.begin(),value.leagues.end(),league_id)!=value.leagues.end();});
        if(group==groups.end())throw std::invalid_argument("world12: prediction promotion group missing");
        if(group==groups.begin())europe=4;
        else {const auto incoming=(group-1)->relegation;if(incoming%group->leagues.size())throw std::invalid_argument("world12: unequal promotion quota needs explicit prediction rules");europe=int(incoming/group->leagues.size());}
    }
    int from=mode>3?std::max(0,rounds-5*(mode>>2)):0;
    const auto table=[&](int first,bool full) {
        TableQuery query;query.first_round=first+1;query.last_round=rounds;query.three_points=false;
        if(full)query.point_deductions=deductions;
        const auto rows=compute_standings(state.world,state.results,league_id,rounds,query);bmh::LeagueTable result;result.size=league.clubs;
        for(const auto& row:rows){const auto at=std::find(members.begin(),members.end(),row.club)-members.begin();result.points[size_t(at)]=int8_t(row.points);result.rank_word[size_t(at)]=row.games;}
        return result;
    };
    prediction.result=bmh::calculate_table_prediction(table(0,true),table(from,false),position,mode,league.rounds(),europe,league.relegation);
    prediction.source_size_word=league.clubs;return prediction;
}
}
