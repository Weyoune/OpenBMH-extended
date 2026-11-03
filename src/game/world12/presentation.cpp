// SPDX-License-Identifier: GPL-3.0-only
#include "game/world12/presentation.hpp"
#include "game/world12/standings.hpp"
#include "game/world12/national_coach.hpp"
#include <algorithm>
namespace bmh::world12 {
namespace {
std::string cup_label(DomesticCup cup){switch(cup){case DomesticCup::german_cup:return "DFB-Pokal";case DomesticCup::german_supercup:return "Super-Cup";case DomesticCup::fa_cup:return "FA Cup";case DomesticCup::league_cup:return "League Cup";case DomesticCup::english_supercup:return "Charity Shield";}throw FormatError("world12: unknown title competition");}
bool league_complete(const CareerSnapshot& s,LeagueId id){const auto& rows=s.results.leagues.at(id.value);return !rows.empty()&&std::all_of(rows.begin(),rows.end(),[](auto r){return r.played;});}
}
std::vector<std::string> matchday_competition_labels(const CareerSnapshot& state,const DayTicket& ticket){
    const auto expected=prepare_day(state);if(!expected||*expected!=ticket)throw FormatError("world12: stale pre-match presentation ticket");
    std::vector<std::string> de,en,shared;
    for(const auto& group:ticket.leagues){const auto& league=state.world.catalogue.league(group.appointment.league);if(league.country==Country::england&&league.playable&&league.local_index>=4)continue;auto& out=league.playable&&league.country==Country::germany?de:league.playable&&league.country==Country::england?en:shared;out.push_back(league.name+(group.ordinary?"":" (Nachholspiel)"));}
    for(const auto& group:ticket.cups)(cup_country(group.cup)==Country::germany?de:en).push_back(cup_label(group.cup));
    for(const auto& group:ticket.european){const auto label=group.competition==EuropeanCompetition::champions_league?"Landesmeister-Cup":group.competition==EuropeanCompetition::cup_winners_cup?"Pokalsieger-Cup":"UEFA-Cup";if(std::find(shared.begin(),shared.end(),label)==shared.end())shared.push_back(label);}
    if(!ticket.indoor.empty())de.push_back("Hallenmasters");if(ticket.regional_playoff)de.push_back("Aufstiegsspiele");
    const bool coached=!state.world.catalogue.scope.club_play()||!national_desktop_actors(state).empty();if(std::any_of(ticket.friendlies.begin(),ticket.friendlies.end(),[&](const auto& f){return !f.national||coached;}))shared.push_back("Freundschaftsspiele");if(!ticket.national.empty()&&coached)shared.push_back("Laenderspiele");
    std::vector<std::string> out=de;if(!de.empty()&&!en.empty())out.emplace_back();out.insert(out.end(),en.begin(),en.end());out.insert(out.end(),shared.begin(),shared.end());return out;
}
std::vector<TitlePresentation> newly_completed_titles(const CareerSnapshot& before,const CareerSnapshot& after,const LeImage& german,const LeImage& english){
    if(before.season!=after.season)throw FormatError("world12: title presentation crossed annual ownership");std::vector<TitlePresentation> out;
    for(const auto& stage:after.cups)if(stage.complete()&&!stage.next_entry&&stage.rounds.back().fixtures.size()==1&&show_domestic_draw(after.world,stage.cup)){
        const auto old=std::find_if(before.cups.begin(),before.cups.end(),[&](const auto& s){return s.cup==stage.cup&&s.source_entry==stage.source_entry;});if(old!=before.cups.end()&&old->complete())continue;const auto winners=stage.qualifiers();if(winners.size()!=1)throw FormatError("world12: completed title has no winner");const bool super=stage.cup==DomesticCup::german_supercup||stage.cup==DomesticCup::english_supercup;out.push_back({winners[0],uint8_t(super?8:1),cup_label(stage.cup)});
    }
    if(after.world.european_campaigns)for(size_t i=0;i<3;++i){const auto& cup=after.world.european_campaigns->at(i);if(cup.complete&&cup.winner.valid()&&(!before.world.european_campaigns||!before.world.european_campaigns->at(i).complete))out.push_back({cup.winner,uint8_t(i+2),i==0?"UEFA-Cup":i==1?"Landesmeister-Cup":"Pokalsieger-Cup"});}
    for(const auto& league:after.world.catalogue.leagues)if(league.active&&league.playable&&league.local_index==0&&league_complete(after,league.id)&&!league_complete(before,league.id)&&std::any_of(after.world.managers.begin(),after.world.managers.end(),[&](const auto& m){return after.world.clubs.at(m.club).country==league.country;})){
        TableQuery q;const auto& rules=league.country==Country::england?english:german;q.three_points=(rules.u8(layout::CareerSession_BeginFriendlyOpponentTable_RuntimeField+league.local_index)&1)!=0;for(const auto& m:after.world.managers)if(after.world.clubs.at(m.club).league==league.id&&m.match_state&&(m.match_state->point_deduction>>5)==3)q.point_deductions[m.club]=m.match_state->point_deduction&31;const auto table=compute_standings(after.world,after.results,league.id,league.rounds(),q);const auto winner=std::find_if(table.begin(),table.end(),[](const auto& row){return row.rank==1;});if(winner==table.end())throw FormatError("world12: championship has no winner");out.push_back({winner->club,0,league.name});
    }
    if(after.world.indoor&&after.world.indoor->masters_winner.valid()&&(!before.world.indoor||before.world.indoor->masters_winner!=after.world.indoor->masters_winner)&&after.world.catalogue.scope.germany)out.push_back({after.world.indoor->masters_winner,7,"Hallenmasters"});
    if(after.world.national_campaigns){const auto found=after.world.national_campaigns->seasons.find(after.season);if(found!=after.world.national_campaigns->seasons.end()&&found->second.complete&&found->second.winner.valid()&&(!before.world.national_campaigns||!before.world.national_campaigns->seasons.contains(before.season)||!before.world.national_campaigns->seasons.at(before.season).complete)&&(!before.world.catalogue.scope.club_play()||!national_desktop_actors(before).empty())){const auto competition=found->second.competition;if(competition==4||competition==6)out.push_back({found->second.winner,uint8_t(competition==4?5:6),competition==4?"Weltmeisterschaft":"Europameisterschaft"});}}
    return out;
}
}
