// SPDX-License-Identifier: GPL-3.0-only
#include "annual_transition_fixture.hpp"
#include "fixture_tables.hpp"
#include "app/world12_annual_transition.hpp"
#include "game/world12/inactive_national_draw.hpp"
#include "game/world12/annual_opening.hpp"
#include "game/calendar.hpp"
#include "game/week.hpp"
#include <iostream>
namespace {
using namespace bmh;using namespace bmh::world12;namespace application=::bmh::app::world12;
void check(bool yes,const char* text){if(!yes)throw std::runtime_error(text);}
template<class F>void rejects(F f,const char* text){bool refused=false;try{f();}catch(const std::exception&){refused=true;}check(refused,text);}

}
int main()try {
    set_fixture_tables(bmh::test::authored_fixture_tables());const auto reference=bmh::test::world12::annual_reference();const auto texts=bmh::test::world12::annual_texts();const auto de=bmh::test::world12::annual_transition_rules(false),en=bmh::test::world12::annual_transition_rules(true);
    application::AnnualTransitionHooks hooks;hooks.tax.present=[](const auto&){return true;};hooks.tax.choose=[](const auto&)->std::optional<bool>{return false;};hooks.qualification=[](const auto&){return true;};hooks.promotion=[](const auto&){return true;};hooks.cup_draw=[](const auto&){return true;};
    size_t score_updates=0,final_displays=0;hooks.highscore=[&](const auto& score,bool final){check(!score.ranked,"authored unranked career acquired a ranked signature");if(final)++final_displays;else ++score_updates;return true;};
    const auto source=bmh::test::world12::annual_transition_closing(reference,de,1);auto state=source;application::ManagerActionOwner owner{1991,0};auto cancelled=hooks;cancelled.highscore=[](const auto&,bool){return false;};
    rejects([&]{application::publish_annual_transition(state,owner,owner,reference,de,en,texts,{},cancelled);},"annual cancellation accepted");check(state==source&&owner.revision==0,"annual cancellation leaked accounts, archive, rosters or RNG");
    const auto outcome=application::publish_annual_transition(state,owner,owner,reference,de,en,texts,{},hooks);
    check(outcome.career_complete&&state.season==1&&state.world.settings->duration==0&&state.date==source.date&&!state.calendar&&!state.progress&&owner.revision==1&&score_updates==1&&final_displays==1,"actual fixed-length annual publication skipped final hook or ran next-year day services");
    check(state.world.managers[0].career->statistics_flags==0x40&&state.world.managers[0].career->player_statistics_flags==0x40,"authoritative annual Flags0/2 reset stayed unknown or lost in publication");
    check(outcome.academy==1,"annual coordinator skipped the actual academy roster movement");
    for(const auto& [id,player]:source.world.players)check(state.world.players.at(id).attributes.age()==player.attributes.age()+1,"published annual player lineage aged twice or remained frozen");
    check(state.world.annual_title_archive&&state.world.annual_title_archive->seasons.size()==1&&state.world.annual_title_archive->seasons[0].season==0&&state.world.annual_awards_completed==1&&state.world.inactive_national_draw->season==1,"annual closing facts/paid marker/scratch draw did not publish exactly once");
    auto hidden_before=source.world.membership[9].clubs,hidden_after=state.world.membership[9].clubs;std::sort(hidden_before.begin(),hidden_before.end());std::sort(hidden_after.begin(),hidden_after.end());check(hidden_before==hidden_after,"hidden English simulation division was exchanged as a fifth player division");
    for(auto round:state.league_rounds)check(round==0,"published year retained a completed league round");
    check(read_save(make_save(state))==state,"published completed annual career did not roundtrip");const auto published=state;rejects([&]{application::publish_annual_transition(state,owner,owner,reference,de,en,texts,{},hooks);},"annual transaction replay accepted");check(state==published&&owner.revision==1,"annual replay changed committed state");
    {
        auto lowest=source;const auto facts=completed_annual_titles(lowest);const auto club=facts.final_rankings[8].back();lowest.world.managers[0].club=club;lowest.world.managers[0].start_league={8};lowest.validate();application::ManagerActionOwner protected_owner{1993,0};
        auto protected_hooks=hooks;protected_hooks.highscore=[](const auto&,bool){return true;};
        const auto protected_year=application::publish_annual_transition(lowest,protected_owner,protected_owner,reference,de,en,texts,{},protected_hooks);
        check(protected_year.career_complete&&lowest.world.managers[0].club==club&&lowest.world.clubs.at(club).league==LeagueId{8},"routine lowest-visible English relegation lost the actual protected human club");
    }
    state=bmh::test::world12::annual_transition_closing(reference,de,5);owner={1992,0};auto owned_hooks=hooks;size_t owned_draws=0;owned_hooks.cup_draw=[](const auto&)->bool{throw std::runtime_error("legacy draw callback used despite actual staged owner");};owned_hooks.cup_draw_owned=[&](const auto& candidate,const auto& notice){candidate.validate();const auto cup=std::find_if(candidate.cups.begin(),candidate.cups.end(),[&](const auto& stage){return stage.cup==notice.cup&&stage.source_entry==notice.source_entry;});check(cup!=candidate.cups.end()&&!cup->complete()&&cup->rounds.front().fixtures==notice.fixtures,"annual draw did not receive its actual new cup owner");++owned_draws;return true;};const auto continuing=application::publish_annual_transition(state,owner,owner,reference,de,en,texts,{},owned_hooks);check(owned_draws>0,"actual staged annual draw owner was not presented");
    check(!continuing.career_complete&&state.world.settings->duration==5&&state.season==1&&annual_opening_ready(state)&&score_updates==2&&final_displays==1,"unlimited annual career decremented or did not reach its real first-Wednesday opening owner");
    check(state.calendar->revision==source.calendar->revision+1&&state.calendar->next_day==0&&weekday_code(state.date)==2,"annual calendar/cursor did not reset to the real opening date");
    check(read_save(make_save(state))==state,"continuing annual calendar/first-Wednesday state failed save roundtrip");
    std::cout<<"Atomic annual coordinator: actual accounts/academy, promotion, fields, renewal, archive, highscore, fixed/unlimited length, opening calendar, save/cancellation/replay passed (authored closing scores, not a played season)\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
