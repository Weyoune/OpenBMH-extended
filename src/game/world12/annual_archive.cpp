// SPDX-License-Identifier: GPL-3.0-only
#include "game/world12/annual_archive.hpp"
#include "game/world12/national_final_facts.hpp"
#include <algorithm>
#include <set>
#include <tuple>
namespace bmh::world12 {
namespace {
[[noreturn]]void invalid(const char* text){throw FormatError(text);}
auto title_key(const AnnualTitleRow& row){return std::tuple{uint8_t(row.kind),row.league.value,row.competition,row.country?uint8_t(*row.country):uint8_t(255)};}
AnnualCupFinal domestic_final(const CareerSnapshot& state,DomesticCup cup) {
    const auto final=std::find_if(state.cups.rbegin(),state.cups.rend(),[&](const auto& stage){return stage.cup==cup;});
    if(final==state.cups.rend()||!final->complete()||final->next_entry||final->rounds.back().fixtures.size()!=1)
        invalid("world12: title archive requires a completed domestic final");
    const auto winners=final->qualifiers();if(winners.size()!=1)invalid("world12: title archive domestic winner is unresolved");
    const auto& fixture=final->rounds.back().fixtures.front();
    if(winners[0]!=fixture.home&&winners[0]!=fixture.away)invalid("world12: title archive domestic final ownership differs");
    return {winners[0],winners[0]==fixture.home?fixture.away:fixture.home};
}
std::optional<uint8_t> source_category(const World& world,const AnnualTitleRow& row) {
    switch(row.kind) {
    case AnnualTitleKind::league:{const auto& league=world.catalogue.league(row.league);return league.playable&&league.local_index==0?std::optional<uint8_t>{0}:std::nullopt;}
    case AnnualTitleKind::domestic:switch(DomesticCup(row.competition)){
        case DomesticCup::german_cup:case DomesticCup::fa_cup:return 1;
        case DomesticCup::german_supercup:case DomesticCup::english_supercup:return 8;
        case DomesticCup::league_cup:return {};}
        break;
    case AnnualTitleKind::european:return uint8_t(row.competition+1);
    case AnnualTitleKind::indoor:return 7;
    case AnnualTitleKind::national:if(row.competition==4)return 5;if(row.competition==6)return 6;return {};
    }
    invalid("world12: title archive has an invalid competition");
}
}
void validate_annual_title_archive(const World& world) {
    if(!world.annual_title_archive)return;const auto& archive=*world.annual_title_archive;
    if(!world.settings||(!archive.earlier_history_unknown&&archive.first_retained_season)||
       uint64_t(archive.first_retained_season)+archive.seasons.size()>uint64_t(UINT32_MAX))
        invalid("world12: invalid annual title archive retained range");
    uint64_t expected=archive.first_retained_season;
    for(const auto& season:archive.seasons) {
        if(season.season!=expected++||season.titles.empty())invalid("world12: annual title archive is duplicated, incomplete or out of order");
        std::optional<decltype(title_key(AnnualTitleRow{}))> previous;
        for(const auto& row:season.titles) {
            if(uint8_t(row.kind)>uint8_t(AnnualTitleKind::national))invalid("world12: unknown archived title kind");
            const auto key=title_key(row);if(previous&&*previous>=key)invalid("world12: duplicate or unordered archived title competition");previous=key;
            const auto winner=world.clubs.find(row.winner),runner=world.clubs.find(row.runner_up);
            if(winner==world.clubs.end()||row.winner==row.runner_up)invalid("world12: archived title has no actual distinct winner");
            const auto kind=row.kind==AnnualTitleKind::national?ClubKind::national_team:ClubKind::club;
            if(winner->second.kind!=kind||(row.runner_up.valid()&&(runner==world.clubs.end()||runner->second.kind!=kind)))invalid("world12: archived title identity has the wrong entity kind");
            if(!row.runner_up.valid()&&!(row.kind==AnnualTitleKind::national&&row.competition==7))invalid("world12: archived title is missing its actual runner-up");
            if(row.kind==AnnualTitleKind::league) {
                if(!row.league.valid()||row.competition||!row.country||world.catalogue.league(row.league).country!=*row.country)
                    invalid("world12: archived league title has invalid country/league metadata");
            } else if(row.league.valid())invalid("world12: non-league title carries a league identity");
            if(row.kind==AnnualTitleKind::domestic) {
                if(row.competition>uint8_t(DomesticCup::english_supercup)||!row.country||cup_country(DomesticCup(row.competition))!=*row.country)
                    invalid("world12: archived domestic title has invalid country/competition metadata");
            }
            if(row.kind==AnnualTitleKind::european&&(row.country||row.competition<1||row.competition>3))invalid("world12: archived European title has invalid metadata");
            if(row.kind==AnnualTitleKind::indoor&&(row.country!=Country::germany||row.competition!=11))invalid("world12: archived indoor title has invalid metadata");
            if(row.kind==AnnualTitleKind::national&&(row.country||row.competition<4||row.competition>7))invalid("world12: archived national title has invalid metadata");
            if(row.country&&(winner->second.country!=*row.country||runner->second.country!=*row.country))invalid("world12: archived domestic title crosses countries");
            if(row.source_category!=source_category(world,row))invalid("world12: archived Source award category differs from its competition");
            if(row.kind==AnnualTitleKind::national&&row.competition==4) {
                if(row.podium[0]!=row.winner||row.podium[1]!=row.runner_up)invalid("world12: archived World Cup podium differs from its final");
                std::set<ClubId> unique;for(auto id:row.podium)if(!world.clubs.contains(id)||world.clubs.at(id).kind!=ClubKind::national_team||!unique.insert(id).second)invalid("world12: archived World Cup podium is incomplete or repeated");
            } else if(std::any_of(row.podium.begin(),row.podium.end(),[](auto id){return id.valid();}))invalid("world12: unrelated archived title carries a World Cup podium");
        }
    }
    for(const auto& [season,labels]:archive.unresolved_source_labels){if(season>32767)invalid("world12: unresolved Source title season exceeds native history range");for(const auto& label:labels)if(label.size()>65535)invalid("world12: unresolved Source title label exceeds native string range");}
}
AnnualTitleArchive stage_annual_title_archive(const CareerSnapshot& state) {
    const auto facts=completed_annual_titles(state);const auto before=state;
    AnnualTitleArchive result;
    if(state.world.annual_title_archive)result=*state.world.annual_title_archive;
    else {result.earlier_history_unknown=true;result.first_retained_season=state.season;}
    if(uint64_t(result.first_retained_season)+result.seasons.size()!=state.season)
        invalid("world12: title archive cannot duplicate or skip a completed season");
    AnnualSeasonTitles season;season.season=state.season;
    const auto append=[&](AnnualTitleRow row){row.source_category=source_category(state.world,row);season.titles.push_back(std::move(row));};
    for(const auto& league:state.world.catalogue.leagues)if(league.active) {
        const auto& ranks=facts.final_rankings[league.id.value];if(ranks.size()<2)invalid("world12: title archive has no actual league finalists");
        AnnualTitleRow row;row.kind=AnnualTitleKind::league;row.country=league.country;row.league=league.id;row.winner=ranks[0];row.runner_up=ranks[1];append(row);
    }
    for(auto country:{Country::germany,Country::england})if(state.world.catalogue.selectable(country))for(auto cup:state.world.catalogue.rules(country).cups) {
        const auto final=domestic_final(state,cup);AnnualTitleRow row;row.kind=AnnualTitleKind::domestic;row.country=country;row.competition=uint8_t(cup);row.winner=final.winner;row.runner_up=final.runner_up;append(row);
    }
    for(size_t k=0;k<3;++k){AnnualTitleRow row;row.kind=AnnualTitleKind::european;row.competition=uint8_t(k+1);row.winner=facts.european_titles[k].winner;row.runner_up=facts.european_titles[k].runner_up;append(row);}
    if(state.world.catalogue.scope.germany) {
        if(!state.world.indoor)invalid("world12: title archive has unknown indoor ownership");
        if(state.world.indoor->masters_winner.valid()) {
            if(state.world.indoor->tournaments.size()!=6||std::any_of(state.world.indoor->tournaments.begin(),state.world.indoor->tournaments.end(),[](const auto& tournament){return !tournament.complete;}))
                invalid("world12: title archive requires actual completed indoor Masters");
            const auto& final=state.world.indoor->tournaments.back();if(final.places[0]!=state.world.indoor->masters_winner)invalid("world12: indoor title differs from actual Masters final");
            AnnualTitleRow row;row.kind=AnnualTitleKind::indoor;row.country=Country::germany;row.competition=11;row.winner=final.places[0];row.runner_up=final.places[1];append(row);
        } else if(!state.world.economy||!state.world.economy->presentation_options||(*state.world.economy->presentation_options)[16*9+4]==0)
            invalid("world12: title archive requires an actual Masters final or known suppressed indoor season");
    }
    if(state.world.catalogue.scope.international) {
        if(!state.world.national_campaigns||!state.world.national_campaigns->seasons.contains(state.season))invalid("world12: title archive needs an owned national season");
        const auto& national=state.world.national_campaigns->seasons.at(state.season);if(!national.complete)invalid("world12: title archive requires the actual completed national tournament");
        const auto facts=national_final_facts(national);
        AnnualTitleRow row;row.kind=AnnualTitleKind::national;row.competition=facts.competition;row.winner=facts.winner;row.runner_up=facts.runner_up;row.podium=facts.places;append(row);
    }
    std::sort(season.titles.begin(),season.titles.end(),[](const auto& a,const auto& b){return title_key(a)<title_key(b);});result.seasons.push_back(std::move(season));
    auto candidate=state.world;candidate.annual_title_archive=result;candidate.validate();
    if(state!=before)invalid("world12: title archive source changed while staging");return result;
}
void retain_unresolved_source_titles(CareerSnapshot& state,const bmh::TitleHistoryArchive& labels) {
    state.validate();auto candidate=state;auto& world=candidate.world;
    if(!world.annual_title_archive){world.annual_title_archive.emplace();world.annual_title_archive->earlier_history_unknown=true;world.annual_title_archive->first_retained_season=state.season;}
    for(const auto& [season,names]:labels) {
        const auto [at,inserted]=world.annual_title_archive->unresolved_source_labels.emplace(season,names);
        if(!inserted&&at->second!=names)invalid("world12: conflicting unresolved Source title labels");
    }
    candidate.validate();state=std::move(candidate);
}
}
