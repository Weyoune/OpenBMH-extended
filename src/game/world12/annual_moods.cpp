// SPDX-License-Identifier: GPL-3.0-only
#include "game/world12/annual_moods.hpp"
#include "game/season.hpp"
#include <algorithm>
#include <set>
namespace bmh::world12 {
UnpublishedAnnualMoods stage_annual_moods(const CareerSnapshot& source,const GameState& reference) {
    const auto facts=completed_annual_titles(source);
    UnpublishedAnnualMoods out;out.source_binding=source;out.snapshot=source;
    const auto& world=out.source_binding.world;auto& candidate=out.snapshot.world;
    if(!world.amateur_slots)throw FormatError("world12: annual moods require known physical amateur order");
    std::set<ClubId> seen,holders;
    const auto append=[&](ClubId id){if(!world.clubs.contains(id)||!seen.insert(id).second)throw FormatError("world12: invalid annual physical club order");out.physical_order.push_back(id);};
    for(auto group:{TableGroup::germany,TableGroup::england,TableGroup::europe})
        for(auto league:world.catalogue.simulation_leagues(group))for(auto id:world.membership[league.value].clubs)append(id);
    for(auto country:{Country::germany,Country::england})if(world.catalogue.scope.domestic_enabled(country)) {
        std::set<ClubId> actual,expected;
        for(const auto& [id,club]:world.clubs)if(club.country==country&&club.kind==ClubKind::club&&!club.league.valid())expected.insert(id);
        for(auto id:(*world.amateur_slots)[country==Country::germany?0:1]){if(!actual.insert(id).second)throw FormatError("world12: repeated annual amateur slot");append(id);}
        if(actual!=expected)throw FormatError("world12: annual amateur order differs from actual country clubs");
    }
    for(const auto& [id,club]:world.clubs)if(club.kind==ClubKind::club&&!seen.contains(id))append(id);
    for(const auto& [id,club]:world.clubs)if(club.kind==ClubKind::national_team)append(id);
    for(const auto& title:facts.domestic_titles)holders.insert(title.cup.winner);
    for(const auto& title:facts.european_titles)holders.insert(title.winner);
    std::map<ClubId,int> ranks;for(const auto& league:facts.final_rankings)for(size_t rank=0;rank<league.size();++rank)ranks[league[rank]]=int(rank);
    auto local=reference;local.date=source.date;local.season=int(source.season+1);local.level=world.settings->difficulty;local.manager_teams.clear();local.players.resize(24);local.squad_slots.assign(24,0xfff);
    const int foreign=runtime_dimensions.record_teams;
    local.team_flags.resize(size_t(foreign)+1);local.team_morale.resize(size_t(foreign)+1);local.foreign_strength.resize(1);
    local.projected_match_clubs.resize(size_t(foreign)+1);
    const auto primary=world.catalogue.scope.england&&!world.catalogue.scope.germany?Country::england:Country::germany;
    GameRandom game(source.game_seed);WatcomRandom random(source.watcom_seed);random.range(0,2);
    for(auto id:out.physical_order) {
        const auto& club=world.clubs.at(id);
        const bool empty_domestic=club.kind==ClubKind::club&&!club.league.valid()&&world.catalogue.scope.domestic_enabled(club.country);
        const bool primary_nation=club.kind==ClubKind::national_team&&club.country==primary;
        const int team=club.league.valid()||empty_domestic||primary_nation?0:foreign;
        const auto squad=std::find_if(world.national_squads.begin(),world.national_squads.end(),[&](const auto& row){return row.team==id;});
        const auto& roster=primary_nation&&squad!=world.national_squads.end()?squad->roster:club.roster;
        std::fill(local.squad_slots.begin(),local.squad_slots.end(),uint16_t(0xfff));
        for(size_t row=0;row<24;++row)if(!roster[row].empty()) {
            local.players[row]=world.players.at(roster[row].player).attributes;
            local.squad_slots[row]=uint16_t(row)|uint16_t((roster[row].flags&1)?0x8000:0);
        }
        local.team_morale[size_t(team)]=club.morale;local.foreign_strength[0]=club.rating;
        local.projected_match_clubs[size_t(team)].table_rank=club.league.valid()?ranks.at(id):2;
        reset_annual_club_mood(local,team,holders.contains(id),game,random);
        auto& updated=candidate.clubs.at(id);updated.morale=local.team_morale[size_t(team)]&63;updated.match_flags=local.team_flags[size_t(team)];
    }
    out.snapshot.game_seed=game.seed();out.snapshot.watcom_seed=random.seed();out.game_draws=game.calls();out.watcom_draws=random.calls();out.snapshot.validate();
    if(source!=out.source_binding)throw FormatError("world12: annual mood source changed during random draws");
    return out;
}
void validate_annual_moods(const UnpublishedAnnualMoods& packet,const GameState& reference) {
    if(stage_annual_moods(packet.source_binding,reference)!=packet)throw FormatError("world12: stale or altered annual mood packet");
}
}
