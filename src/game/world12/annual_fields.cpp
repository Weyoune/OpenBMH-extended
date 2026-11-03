// SPDX-License-Identifier: GPL-3.0-only
#include "game/world12/annual_fields.hpp"
#include "game/world12/european_campaign.hpp"
#include "game/world12/standings.hpp"
#include "game/career.hpp"
#include "game/market_rules.hpp"
#include <algorithm>
namespace bmh::world12 {
AnnualFieldsStage stage_annual_fields(const CareerSnapshot& source,const UnpublishedAnnualQualification& qualification,
    const LeImage& german,const LeImage& english) {
    source.validate();const auto before=source;validate_annual_qualification(qualification.source_binding,qualification);
    if(source.season!=qualification.source_season||source.date!=qualification.source_binding.date||!source.world.initial_competitions||source.world.indoor)
        throw FormatError("world12: annual field reset has no owned quiescent exchange boundary");
    AnnualFieldsStage out;out.world=source.world;auto& world=out.world;
    world.european_campaigns.reset();auto& fields=*world.initial_competitions;
    fields.qualified=qualification.next_european.qualified;fields.fields=qualification.next_european.fields;fields.holders=qualification.next_european.holders;
    fields.champions.clear();fields.domestic_holders.clear();
    for(const auto& title:qualification.domestic_titles){fields.champions.push_back({title.country,title.champion});fields.domestic_holders.push_back({title.country,title.cup.winner});}
    // national_field is the retained startup draw, not a replacement for the
    // active/new national programme or the disabled-scope scratch owner.
    world.european_campaigns=initial_european_campaigns(world,german);
    GameRandom game(source.game_seed);WatcomRandom random(source.watcom_seed);
    for(auto country:{Country::germany,Country::england})if(world.catalogue.selectable(country)) {
        CountryCupRanking ranking;
        for(uint8_t local=0;local<5;++local){const auto league=world.catalogue.starting_league(country,local);TableQuery query;
            for(const auto& manager:world.managers)if(manager.match_state&&world.clubs.at(manager.club).league==league&&manager.match_state->point_deduction>>5==3)query.point_deductions[manager.club]=manager.match_state->point_deduction&31;
            for(const auto& row:compute_standings(world,source.results,league,source.league_rounds[league.value],query))ranking[local].push_back(row.club);
        }
        const auto& rules=country==Country::germany?german:english;
        auto cups=initial_domestic_cup_fields(world,country,ranking,rules,game);
        out.cups.insert(out.cups.end(),cups.begin(),cups.end());
        const auto super=country==Country::germany?DomesticCup::german_supercup:DomesticCup::english_supercup;
        if(domestic_cup_expected(world,super)) {
            const auto champion=std::find_if(fields.champions.begin(),fields.champions.end(),[&](const auto& row){return row.first==country;});
            const auto holder=std::find_if(fields.domestic_holders.begin(),fields.domestic_holders.end(),[&](const auto& row){return row.first==country;});
            out.cups.push_back(KnockoutStage::from_source(super,rules,23,{{champion->second,holder->second}},world));
        }
    }
    const uint8_t european_rounds[]={1,3,2};
    for(auto& manager:world.managers){if(!manager.career||!manager.career->season_records)throw FormatError("world12: annual participation history is unknown");auto& history=*manager.career->season_records;const auto at=size_t(std::min<uint32_t>(qualification.target_season,49))*3;history[at+1]=history[at+2]=0;
        for(size_t kind=0;kind<3;++kind)if(std::find(fields.qualified[kind].begin(),fields.qualified[kind].end(),manager.club)!=fields.qualified[kind].end())history[at+2]=uint8_t(european_rounds[kind]+kind*32);
        const auto country=world.clubs.at(manager.club).country;const auto main=country==Country::germany?DomesticCup::german_cup:DomesticCup::fa_cup;
        for(const auto& stage:out.cups)if(stage.cup==main){bool entered=std::find(stage.seeded.begin(),stage.seeded.end(),manager.club)!=stage.seeded.end();for(const auto& round:stage.rounds)for(const auto& fixture:round.fixtures)entered|=fixture.home==manager.club||fixture.away==manager.club;if(entered)history[at+1]=1;}
    }
    // Source shuffles every actual league in numeric service order. This
    // changes future fixtures while all global identities and squads stay put.
    const auto population_catalogue=world.national_origins&&!world.national_origins->empty()?Catalogue::dual_classic({true,true,true}):world.catalogue;
    std::vector<LeagueId> physical_leagues;for(auto group:{TableGroup::germany,TableGroup::england,TableGroup::europe})for(auto league:population_catalogue.simulation_leagues(group))physical_leagues.push_back(league);
    for(auto identity:physical_leagues) {const auto league=identity.value;
        if(world.catalogue.leagues[league].active) {
            auto& clubs=world.membership[league].clubs;
            shuffle_initial_positions(0,int(clubs.size()),random,[&](int a,int b){std::swap(clubs.at(size_t(a)),clubs.at(size_t(b)));});
        } else if(world.national_origins) {
            auto& origins=*world.national_origins;std::vector<size_t> slots;
            for(size_t i=0;i<origins.size();++i)if(origins[i].club.league==LeagueId{league})slots.push_back(i);
            std::sort(slots.begin(),slots.end(),[&](size_t a,size_t b){return origins[a].rank<origins[b].rank;});
            if(slots.empty())continue;
            if(slots.size()!=world.catalogue.leagues[league].clubs)throw FormatError("world12: annual inactive shuffle lacks its actual complete origin league");
            shuffle_initial_positions(0,int(slots.size()),random,[&](int a,int b){std::swap(slots.at(size_t(a)),slots.at(size_t(b)));});
            for(size_t rank=0;rank<slots.size();++rank)origins[slots[rank]].rank=uint8_t(rank);
        }
    }
    if(world.national_origins)std::sort(world.national_origins->begin(),world.national_origins->end(),[](const auto& a,const auto& b){return a.club.league!=b.club.league?a.club.league<b.club.league:a.rank<b.rank;});
    // Source reorders ties after the league fixture shuffle. The draw module's
    // classification depends on actual divisions, so this preserves its result
    // without selecting another field or consuming another random stream.
    for(const auto& cup:out.cups)if(const auto notice=cup_draw_notice(world,cup))out.draws.push_back(*notice);
    world.validate();validate_cup_programmes(world,out.cups);out.game_seed=game.seed();out.watcom_seed=random.seed();out.game_draws=game.calls();out.watcom_draws=random.calls();
    if(source!=before)throw FormatError("world12: annual field source changed during selection/shuffle");return out;
}
}
