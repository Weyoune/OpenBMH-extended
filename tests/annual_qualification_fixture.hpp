// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "world12_fixture.hpp"
#include "game/world12/european.hpp"
#include "game/world12/european_campaign.hpp"
#include "game/world12/season_awards.hpp"
#include "game/calendar.hpp"
#include <algorithm>

namespace bmh::test::world12 {
// Authored complete score programmes, not a played season or a production
// fallback. All knockout transitions/draws and European finals use shared Core.
inline bmh::world12::CareerSnapshot completed_qualification_closing(
    bmh::world12::GameScope scope={true,true,false},bool cw_champion=false) {
    using namespace bmh::world12;
    auto master=complete_world();uint32_t extra=0xe1000000u;
    for(uint8_t c=5;c<32;++c)for(int slot=0;slot<7;++slot) {
        ClubRecord club;club.id={extra++};club.country=Country(c);club.name="Reserve "+std::to_string(c)+"/"+std::to_string(slot);
        club.crest={club.id.value};master.crests[club.crest]={"W/TEST.VGA",0};master.clubs[club.id]=club;
    }
    for(uint8_t c=0;c<62;++c) {
        ClubRecord club;club.id={extra++};club.country=Country(c);club.kind=ClubKind::national_team;club.name="Nation "+std::to_string(c);
        club.crest={club.id.value};master.crests[club.crest]={"W/TEST.VGA",0};master.clubs[club.id]=club;
    }
    master.validate();CareerSnapshot s;s.world=select_world_scope(master,scope);auto& w=s.world;
    w.players_initialized=true;w.markets.emplace();w.free_players.emplace();w.settings=CareerSettings{3,2,5,1,false};w.economy.emplace();w.annual_awards_completed=0;
    for(const auto& league:w.catalogue.leagues)if(league.active) {
        for(auto id:w.membership[league.id.value].clubs)w.clubs.at(id).fairplay_points=20;
        w.clubs.at(w.membership[league.id.value].clubs[2]).fairplay_points=1;
    }
    const auto primary=LeagueId{scope.germany?0u:5u};const auto& primary_clubs=w.membership[primary.value].clubs;
    const auto uefa_target=primary_clubs[3],cl_target=primary_clubs[0],cw_target=primary_clubs[7];
    GameRandom game(17);WatcomRandom random(91);
    w.initial_competitions=draw_initial_shared_fields(w,game,random);
    Manager manager{3,"Manager",uefa_target,primary,3};manager.finance=ManagerFinance{1234567,100000,0,0,0};manager.tactics.emplace();manager.match_state.emplace();manager.career.emplace();
    manager.career->season_records.emplace();manager.career->cup_titles.emplace();manager.career->annual_records.emplace();manager.career->season_services.emplace();
    w.managers.push_back(manager);w.initial_competitions->manager_services_complete=true;
    const auto de=country_rules(false),en=country_rules(true);
    const auto score=[](const Fixture& f,ClubId first,ClubId second,bool return_leg) {
        if(f.home==first)return uint16_t(3<<5);if(f.away==first)return uint16_t(3);
        if(f.home==second)return uint16_t(3<<5);if(f.away==second)return uint16_t(3);
        return return_leg?uint16_t(1):uint16_t(2<<5);
    };
    // Keep both selected finalists alive under genuine Source draws; retry a
    // fixture seed when their bracket meets before the final.
    for(int attempt=0;;++attempt) {
        if(attempt>100)throw std::runtime_error("authored domestic finalist fixture did not resolve");
        auto cups=std::vector<KnockoutStage>{};GameRandom cup_game(17+attempt);WatcomRandom cup_random(91+attempt);
        for(auto country:{Country::germany,Country::england})if(w.catalogue.selectable(country)) {
            const auto source=country==Country::germany?de:en;
            auto country_cups=initial_domestic_cup_fields(w,country,membership_ranking(w,country),source,cup_game);
            cups.insert(cups.end(),country_cups.begin(),country_cups.end());
            const auto league=w.catalogue.starting_league(country,0);const auto& ids=w.membership[league.value].clubs;
            cups.push_back(KnockoutStage::from_source(country==Country::germany?DomesticCup::german_supercup:DomesticCup::english_supercup,
                source,23,{{ids[0],ids[7]}},w));
        }
        for(size_t i=0;i<cups.size();++i)while(!cups[i].complete()) {
            const auto source=cup_country(cups[i].cup)==Country::germany?de:en;
            const auto league=w.catalogue.starting_league(cup_country(cups[i].cup),0);const auto& ids=w.membership[league.value].clubs;
            std::vector<uint16_t> scores;for(const auto& f:cups[i].next_fixtures())scores.push_back(score(f,ids[7],ids[0],cups[i].next_leg()==2));
            const auto result=draw_after_cup_leg(w,cups[i],scores,source,cup_random);commit_cup_leg(cups[i],result,w);
            if(result.next_stage)cups.push_back(*result.next_stage);
        }
        bool selected=true;
        for(auto country:{Country::germany,Country::england})if(w.catalogue.selectable(country)) {
            const auto kind=country==Country::germany?DomesticCup::german_cup:DomesticCup::fa_cup;
            const auto final=std::find_if(cups.rbegin(),cups.rend(),[&](const auto& c){return c.cup==kind;});
            const auto& ids=w.membership[w.catalogue.starting_league(country,0).value].clubs;
            const auto& f=final->rounds.back().fixtures.front();
            selected&=(f.home==ids[0]&&f.away==ids[7])||(f.home==ids[7]&&f.away==ids[0]);
        }
        if(selected){s.cups=std::move(cups);break;}
    }
    w.european_campaigns=initial_european_campaigns(w,de);
    const ClubId targets[]={uefa_target,cl_target,cw_target};
    for(size_t k=0;k<3;++k)while(!(*w.european_campaigns)[k].complete) {
        const auto groups=prepare_european_groups((*w.european_campaigns)[k]);EuropeanResults result;
        result.competition=groups.front().competition;result.revision=groups.front().revision;
        for(const auto& group:groups){std::vector<uint16_t> scores;
            for(const auto& f:group.matches)scores.push_back(score(f,targets[k],{},group.leg==2));
            result.groups.push_back({group.source_entry,group.played,std::move(scores)});
        }
        commit_european_results(w,result,de,random);
    }
    s.results=SeasonResults::empty(w.catalogue);SeasonCalendar schedule;
    const auto champion=cw_champion?cw_target:uefa_target;
    for(const auto& league:w.catalogue.leagues)if(league.active) {
        s.league_rounds[league.id.value]=uint8_t(league.rounds());
        for(int round=1;round<=league.rounds();++round) {
            schedule.leagues.push_back({uint16_t(7+(round-1)*8),league.id,uint8_t(round)});
            const auto matches=league_fixtures(w,league.id,round);
            for(size_t m=0;m<matches.size();++m) {
                auto& value=s.results.result(w.catalogue,league.id,round,int(m));value={true,0,0};
                const auto league_champion=league.id==primary?champion:w.membership[league.id.value].clubs.back();
                if(matches[m].home==league_champion)value.home=3;if(matches[m].away==league_champion)value.away=3;
            }
        }
    }
    std::sort(schedule.leagues.begin(),schedule.leagues.end(),[](const auto& a,const auto& b){return a.day!=b.day?a.day<b.day:a.league<b.league;});
    s.calendar=CalendarCursor{schedule,365,365};s.desktop.emplace();s.desktop->managers.push_back({3});
    const auto epoch=calendar_epoch();s.date=365;s.progress=CareerProgress{ProgressPhase::season_end,DayEntry::plain_day,364,0,uint16_t(epoch.year),uint8_t(epoch.weekday),3};
    s.game_seed=101;s.watcom_seed=707;s.validate();
    s.world=stage_annual_awards(s,GameState{},TextDb{}).world;s.validate();return s;
}
}
