// SPDX-License-Identifier: GPL-3.0-only
// Implementation of the almanac recording (almanac.hpp).
#include "game/world12/almanac.hpp"
#include "game/world12/standings.hpp"
#include "game/season_awards.hpp"
#include <algorithm>
namespace bmh::world12 {
AlmanacSeason stage_almanac_season(const CareerSnapshot& state){
    const auto& world=state.world;
    AlmanacSeason out;out.season=state.season;
    for(const auto& league:world.catalogue.leagues){
        if(!league.active)continue;
        AlmanacLeagueSeason record;record.league=league.id;
        // Same query as completed_annual_titles(): the deduction of a human manager counts in his league.
        TableQuery query;
        for(const auto& manager:world.managers)
            if(manager.match_state&&world.clubs.at(manager.club).league==league.id&&(manager.match_state->point_deduction>>5)==3)
                query.point_deductions[manager.club]=manager.match_state->point_deduction&31;
        for(const auto& row:compute_standings(world,state.results,league.id,state.league_rounds[league.id.value],query))
            record.table.push_back({row.club,row.games,row.wins,row.draws,row.defeats,row.goals_for,row.goals_against,row.points});
        // Scorers: the goal counter of the season is bits 2..7 of the player word w364; the order of equal
        // scorers is the one of the original scorer list (club order, then squad row).
        std::vector<std::pair<ClubId,PlayerId>> identities;
        std::vector<SeasonScoringEntry> counters;
        for(auto club:world.membership[league.id.value].clubs)
            for(const auto& slot:world.clubs.at(club).roster){
                if(slot.empty())continue;
                const auto& person=world.players.at(slot.player).attributes;
                identities.push_back({club,slot.player});
                counters.push_back({uint8_t((person.w364>>2)&63),uint8_t((person.w368>>18)&63)});
            }
        for(const auto index:bmh::rank_season_scorers(counters,2)){
            if(record.scorers.size()==kAlmanacScorers)break;
            record.scorers.push_back({world.players.at(identities[index].second).attributes.name,identities[index].first,counters[index].goals});
        }
        out.leagues.push_back(std::move(record));
    }
    return out;
}

void record_almanac_season(World& world,AlmanacSeason season,const std::vector<Manager>& managers,const World& closing){
    if(!world.extended)world.extended.emplace();
    auto& extended=*world.extended;
    if(!extended.almanac.empty()&&extended.almanac.back().season+1!=season.season)extended.almanac.clear();
    const auto number=season.season;
    extended.almanac.push_back(std::move(season));
    for(const auto& manager:managers){
        const auto club=closing.clubs.find(manager.club);
        if(club==closing.clubs.end()||club->second.kind!=ClubKind::club)continue;   // a national coach without a club
        auto& history=extended.manager_clubs[manager.seat];
        // A history that would not stay ascending (should not happen) is restarted rather than corrupted.
        if(!history.empty()&&history.back().season>=number)history.clear();
        history.push_back({number,manager.club,club->second.league});
    }
}
}
