// SPDX-License-Identifier: GPL-3.0-only
#include "world12_fixture.hpp"
#include "app/world12_cups.hpp"
#include "ui/world12_other_games.hpp"
#include "ui/other_games.hpp"
#include "ui/setup.hpp"
#include "game/world12/codec.hpp"
#include <iostream>
#include <stdexcept>

namespace {
using namespace bmh::world12;using namespace bmh::ui;
void check(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
template<class Function> void rejects(Function function,const char* message){bool rejected=false;try{function();}catch(const std::exception&){rejected=true;}check(rejected,message);}
CareerSnapshot snapshot(GameScope scope={}) {
    auto world=select_world_scope(bmh::test::world12::complete_world(),scope);
    const auto country=scope.germany?Country::germany:Country::england;
    const auto league=world.catalogue.starting_league(country,0);
    world.managers={{0,"Manager",world.membership[league.value].clubs[0],league,0}};
    CareerSnapshot state;state.world=world;state.results=SeasonResults::empty(world.catalogue);
    const auto de=bmh::test::world12::country_rules(false),en=bmh::test::world12::country_rules(true);
    CountryCupRanking dr,er;
    if(scope.germany)dr=bmh::test::world12::membership_ranking(world,Country::germany);
    if(scope.england)er=bmh::test::world12::membership_ranking(world,Country::england);
    bmh::app::world12::install_domestic_cup_fields(state,dr,er,{de,en});
    for(const auto cup:{DomesticCup::german_supercup,DomesticCup::english_supercup}) {
        if(!scope.domestic_enabled(cup_country(cup)))continue;
        const auto league=state.world.catalogue.starting_league(cup_country(cup),0);const auto& clubs=state.world.membership[league.value].clubs;
        auto stage=KnockoutStage::from_source(cup,cup_country(cup)==Country::germany?de:en,23,{{clubs[0],clubs[1]}},state.world);
        commit_cup_leg(stage,{cup,23,0,{uint16_t(cup==DomesticCup::german_supercup?65:32)},{},{}},state.world);state.cups.push_back(stage);
    }
    state.validate();return state;
}
World12OtherGamesIdentity owner(const CareerSnapshot& state){return {0,state.world.managers[0].club,42,71,1};}
OtherGamesRequest navigation(const World12OtherGamesIdentity& id,OtherGamesAction action,int value=-1){OtherGamesRequest r;r.world12_identity=id;r.action=action;r.value=value;return r;}
void model_tests() {
    bmh::TextDb texts;texts.strings.resize(bmh::TEXT_COUNT,"Authored competition");
    const auto state=snapshot();const auto id=owner(state);World12OtherSelection selected;selected.page=World12OtherPage::supercups;
    const auto page=world12_other_games_state(state,id,selected);
    check(page.projection.selectors.size()==8 && page.selector_labels[5]=="SUPER-CUP" && page.selector_labels[6]=="FA CUP" && page.selector_labels[7]=="KOKA KOLA CUP","mixed BMH/FLTD selector layout incomplete");
    check(page.sections.size()==2 && page.projection.rows.size()==2 && page.fixture_baselines[1]-page.fixture_baselines[0]>=48,"independent supercups look like a semifinal pair");
    check(page.sections[0].title.find("Deutschland")!=std::string::npos && page.sections[1].title.find("Charity Shield")!=std::string::npos,"supercup country headings missing");
    check(page.rows[0].primary_result=="2:1" && page.rows[1].primary_result=="1:0","independent supercup scores combined or reordered");
    const auto de_team=state.world.membership[0].clubs[0],en_team=state.world.membership[5].clubs[0];
    check(page.world12_teams.at(0)==de_team && page.world12_teams.at(2)==en_team && de_team.value>65535,"world club references narrowed into renderer tokens");
    for(const auto scope:{GameScope{true,false,true},GameScope{false,true,true}}) {
        const auto one=snapshot(scope);const auto visible=world12_other_games_state(one,owner(one),selected);
        check(visible.sections.size()==1 && visible.projection.rows.size()==1,"disabled country's supercup still appears");
        check(visible.selector_labels[0].empty()==!scope.germany && visible.selector_labels[6].empty()==!scope.england,"disabled country's competition still selectable");
    }
    auto missing=state;missing.cups.pop_back();const auto pending=world12_other_games_state(missing,id,selected);
    check(pending.sections.size()==2 && pending.projection.rows.size()==1 && !pending.sections[1].message.empty(),"missing English supercup produced a fictional fixture");
    auto league_only=snapshot({true,true,false});World12OtherSelection qualification;qualification.page=World12OtherPage::national_qualifiers;
    rejects([&]{world12_other_games_state(league_only,owner(league_only),qualification);},"disabled International still exposes a qualifier page");
    for(const auto scope:{GameScope{true,false,true},GameScope{false,true,true},GameScope{true,true,false}}){
        const auto scoped=snapshot(scope);const auto identity=owner(scoped);const auto first=default_world12_other_selection(scoped.world);const auto page=world12_other_games_state(scoped,identity,first);
        check(page.projection.selectors[0].active==scope.germany&&page.projection.selectors[6].active==scope.england&&page.projection.selectors[7].active==scope.england&&page.projection.selectors[4].active==scope.international,"scoped overview reintroduced disabled cup/national pages");
        check(first.page==(scope.germany?World12OtherPage::german_cup:World12OtherPage::fa_cup),"default overview page belongs to a removed country");
        World12OtherSelection final;final.page=World12OtherPage::supercups;const auto finals=world12_other_games_state(scoped,identity,final);
        check(finals.sections.size()==size_t(scope.germany)+size_t(scope.england),"supercups show a disabled country's final");
    }
    auto selection=default_world12_other_selection(state.world);
    for(int page=0;page<8;++page) {
        const auto next=navigate_world12_other_games(state,id,selection,navigation(id,OtherGamesAction::select_competition,page),&texts);
        check(int(next.page)==page,"selector slot interpreted as a legacy competition ID");
        world12_other_games_state(state,id,next,{},&texts);
    }
    selection=navigate_world12_other_games(state,id,selection,navigation(id,OtherGamesAction::select_competition,0));
    check(selection.participant_offset==16,"same competition selector did not cycle participant pages");
    const auto second=world12_other_games_state(state,id,selection);check(second.projection.rows.front().source_match==8,"second detail window contains first matches again");
    selection=navigate_world12_other_games(state,id,selection,navigation(id,OtherGamesAction::toggle_display_mode));
    check(world12_other_games_state(state,id,selection).projection.rows.size()==32,"complete-view toggle lost knockout fixtures");
    auto rounds=default_world12_other_selection(state.world);
    rounds=navigate_world12_other_games(state,id,rounds,navigation(id,OtherGamesAction::previous_round));
    check(rounds.round==5&&rounds.participant_offset==0,"previous round did not wrap to the last planned Source round");
    check(!world12_other_games_state(state,id,rounds).projection.fixtures_available,"uninitialized future draw manufactured fixtures");
    rounds=navigate_world12_other_games(state,id,rounds,navigation(id,OtherGamesAction::next_round));
    check(rounds.round==0,"last round did not wrap back to the first");
    auto league=default_world12_other_selection(state.world);league.page=World12OtherPage::league_cup;
    league=navigate_world12_other_games(state,id,league,navigation(id,OtherGamesAction::select_competition,7));
    check(league.participant_offset==16,"League Cup second participant window missing");
    league=navigate_world12_other_games(state,id,league,navigation(id,OtherGamesAction::select_competition,7));
    check(league.participant_offset==0,"24-player window wrapped to an invalid offset eight");
    rejects([&]{navigate_world12_other_games(state,id,selection,navigation(id,OtherGamesAction::select_competition,5));},"hidden compact-mode selector was accepted");
    auto compact=selection;compact.page=World12OtherPage::supercups;compact.mode=OtherGamesDisplayMode::compact;
    check(world12_other_games_state(state,id,compact).projection.display_mode==OtherGamesDisplayMode::detail,"two finals inherited the preceding full-cup compact mode");
    auto stale=id;++stale.sequence;rejects([&]{navigate_world12_other_games(state,stale,selection,navigation(id,OtherGamesAction::select_competition,5));},"stale typed navigation sequence accepted");
    rejects([&]{navigate_world12_other_games(state,id,selected,navigation(id,OtherGamesAction::next_round));},"merged finals acquired a fake second round");
    auto switched=state;switched.world.managers[0].club=switched.world.membership[0].clubs[1];
    rejects([&]{world12_other_games_state(switched,id,selected);},"stale manager/club identity accepted");
    const auto before=state;world12_other_games_state(state,id,selected);check(state==before,"read-only overview changed world/results/RNG");
}
void saved_view_codec() {
    auto world=bmh::test::world12::complete_world();world.players_initialized=true;world.markets.emplace();world.free_players.emplace();world.settings=CareerSettings{3,3,5,1,false};
    const auto club=world.membership[0].clubs[0];world.managers={{0,"Manager",club,{0},0}};world.managers[0].career.emplace();world.managers[0].career->other_games_view=0x90;
    check(decode_world(encode_world(world))==world,"schema32 lost other-games per-seat Source byte");
    const auto bytes=encode_world(world);auto raw=bmh::cli::parse_json(std::string(bytes.begin(),bytes.end()));raw.object()->at("version")=bmh::cli::Json(31);auto encoded=bmh::cli::dump_json(raw);
    rejects([&]{decode_world(bmh::Bytes(encoded.begin(),encoded.end()));},"old schema accepted a display byte it never stored");
    auto& managers=std::get<bmh::cli::Json::Array>(raw.object()->at("managers").value);managers[0].object()->at("career").object()->erase("other_games_view");encoded=bmh::cli::dump_json(raw);
    check(!decode_world(bmh::Bytes(encoded.begin(),encoded.end())).managers[0].career->other_games_view,"old schema invented an other-games display byte");
}
void render_tests(const std::filesystem::path& data) {
    const auto texts=bmh::load_textd(bmh::ui::asset_path(data,"TEXTD.BLK"));
    const auto state=snapshot();const auto id=owner(state);World12OtherSelection selected;selected.page=World12OtherPage::supercups;
    OtherGamesScreen screen(data,world12_other_games_state(state,id,selected));
    check(screen.render().width==320&&screen.render().height==240,"native classic surface changed");
    check(!screen.pointer_down(40,100),"space between finals is a clickable match");
    check(screen.pointer_down(40,70)&&screen.pending()&&screen.pending()->action==OtherGamesAction::team_info&&screen.pending()->world12_team==state.world.membership[0].clubs[0]&&screen.pending()->value==-1,
        "German final team lookup leaked a UI token as an engine ID");
    OtherGamesScreen english(data,world12_other_games_state(state,id,selected));
    check(english.pointer_down(40,130)&&english.pending()->world12_team==state.world.membership[5].clubs[0],"English final hitbox maps to German semifinal");
    auto too_close=world12_other_games_state(state,id,selected);too_close.sections[1].heading_baseline=85;
    rejects([&]{OtherGamesScreen invalid(data,too_close);},"independent final blocks can overlap visually");
    OtherGamesScreen selector(data,world12_other_games_state(state,id,{}));
    check(selector.pointer_down(130,204)&&selector.release(130,204)&&selector.pending()&&selector.pending()->value==6&&selector.pending()->world12_identity==id,
        "right-column FA Cup control is not wired to its typed selector");
    for(int page=0;page<8;++page){World12OtherSelection choice;choice.page=World12OtherPage(page);OtherGamesScreen candidate(data,world12_other_games_state(state,id,choice,{},&texts));candidate.render();}
}
}
int main(int argc,char** argv)try {
    model_tests();saved_view_codec();if(argc==2)render_tests(argv[1]);
    std::cout<<"Mixed competition selectors, independent supercup sections, scope filtering, wide team IDs and typed navigation passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
