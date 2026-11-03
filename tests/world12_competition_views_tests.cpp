// SPDX-License-Identifier: GPL-3.0-only
#include "annual_qualification_fixture.hpp"
#include "fixture_tables.hpp"
#include "ui/world12_other_games.hpp"
#include "ui/world12_draws.hpp"
#include "game/world12/european_campaign.hpp"
#include "game/cup.hpp"
#include <iostream>
using namespace bmh;using namespace bmh::world12;using namespace bmh::ui;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class F>void rejects(F action,const char* message){bool rejected=false;try{action();}catch(const std::exception&){rejected=true;}check(rejected,message);}
void saved_european(){
    auto snapshot=bmh::test::world12::completed_qualification_closing();const auto source=bmh::test::world12::country_rules(false);snapshot.world.european_campaigns.reset();snapshot.world.european_campaigns=initial_european_campaigns(snapshot.world,source);WatcomRandom random(91);size_t views=0;
    const World12OtherGamesIdentity identity{3,snapshot.world.managers[0].club,8,71,1ull<<32};
    for(size_t k=0;k<3;++k){auto& campaign=(*snapshot.world.european_campaigns)[k];while(!campaign.complete){const auto groups=prepare_european_groups(campaign);EuropeanResults results{campaign.competition,campaign.revision,{}};
        for(const auto& group:groups){const auto stage=std::find_if(campaign.stages.begin(),campaign.stages.end(),[&](const auto& value){return value.rules.entry==group.source_entry;});World12OtherSelection selection;selection.page=World12OtherPage(k+1);selection.stage=int(stage-campaign.stages.begin());const auto before=snapshot;const auto seed=random.seed();const auto calls=random.calls();
            const auto page=world12_european_games_state(snapshot,identity,selection);check(page.projection.fixtures_available&&page.projection.entry_index==group.source_entry&&page.projection.rows.size()==std::min<size_t>(8,group.matches.size()),"European saved page lost actual active fixtures/stage");
            for(size_t m=0;m<page.projection.rows.size();++m){const auto& row=page.projection.rows[m];check(page.world12_teams.at(row.home_team)==group.matches[m].home&&page.world12_teams.at(row.away_team)==group.matches[m].away,"European renderer tokens changed canonical club identities");}
            std::vector<uint16_t> scores(group.matches.size(),group.leg==2?uint16_t(1):uint16_t(2<<5));const World12CompetitionOverlay overlay{campaign.revision,group.source_entry,group.played,group.matches,scores};const auto live=world12_european_games_state(snapshot,identity,selection,{overlay});check(live.projection.rows.front().source_result==scores.front(),"actual current score overlay was not projected");
            if(!stage->rules.knockout())check(live.table_rows.size()==4&&live.projection.group_tables.size()==1,"European real group table/ranks are missing");
            auto stale=overlay;++stale.revision;rejects([&]{world12_european_games_state(snapshot,identity,selection,{stale});},"stale live group overlay accepted");auto wrong=overlay;std::swap(wrong.matches[0].home,wrong.matches[0].away);rejects([&]{world12_european_games_state(snapshot,identity,selection,{wrong});},"another live fixture orientation accepted");
            check(snapshot==before&&random.seed()==seed&&random.calls()==calls,"European readonly/live projection advanced campaign/draw/RNG");results.groups.push_back({group.source_entry,group.played,scores});++views;
        }
        commit_european_results(snapshot.world,results,source,random);
    }}std::cout<<views<<" actual saved European stage/group views, canonical fixtures/current-score overlays, stale guards and pure state/RNG passed\n";
}
void captured_draw(){
    CareerSnapshot state;state.world=bmh::test::world12::complete_world();state.results=SeasonResults::empty(state.world.catalogue);state.world.managers={{3,"Draw",state.world.membership[0].clubs[0],{0},3}};const auto rules=bmh::test::world12::country_rules(false);GameRandom game(17);state.cups=initial_domestic_cup_fields(state.world,Country::germany,bmh::test::world12::membership_ranking(state.world,Country::germany),rules,game);const auto notice=*cup_draw_notice(state.world,state.cups[0]);TextDb texts;texts.strings.resize(2000,"Authored");GameState reference;
    for(auto address:{layout::PlainBackground,layout::TextGradientDisabled,layout::SolidPanelBackground,layout::CupDraw_FormatError_RuntimeField})reference.memory.add_block(address,1);
    const auto before=state;const auto view=world12_cup_draw_notice(state,notice,reference,rules,texts);check(view.projection.count==notice.fixtures.size()*2&&view.clubs.size()==view.projection.count,"captured cup draw lost actual participants");for(size_t n=0;n<notice.fixtures.size();++n)for(size_t side=0;side<2;++side){const auto club=side?notice.fixtures[n].away:notice.fixtures[n].home;const auto token=uint16_t(n*2+side);check(view.clubs.at(token)==club&&view.projection.clubs[token].badge_atlas==state.world.crests.at(state.world.clubs.at(club).crest).path,"captured cup draw invented identity or crest path");}check(state==before,"captured cup draw projection changed state");auto stale=notice;std::swap(stale.fixtures[0].home,stale.fixtures[0].away);rejects([&]{world12_cup_draw_notice(state,stale,reference,rules,texts);},"forged captured draw order accepted");
}
}
int main()try{set_fixture_tables(bmh::test::authored_fixture_tables());saved_european();captured_draw();return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
