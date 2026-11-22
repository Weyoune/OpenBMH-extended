// SPDX-License-Identifier: GPL-3.0-only
// Tests of the additions of OpenBMH-Extended to the shared world (no original data needed):
// the saved form of ExtendedFeatures, the almanac recording, the job offer rule and the change of club.
#include "world12_fixture.hpp"
#include "fixture_tables.hpp"
#include "game/world12/almanac.hpp"
#include "game/world12/job_offers.hpp"
#include "game/world12/manager_change.hpp"
#include "game/world12/manager_tactics.hpp"
#include "game/world12/codec.hpp"
#include "game/career.hpp"
#include "cli/json.hpp"
#include <iostream>
#include <set>
using namespace bmh;using namespace bmh::world12;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class F>void rejects(F f,const char* message){bool caught=false;try{f();}catch(...){caught=true;}check(caught,message);}
// Reference state, rule image and base career as in world12_running_join_tests.cpp: one manager ("Host",
// seat 0, first club of league 0) at an empty first desk on calendar day 0.
GameState reference(){
    GameState g;g.club_ids.resize(445);g.team_flags.resize(445);g.team_morale.resize(445);g.formations.resize(0x580);g.tactics.resize(4*0x840);g.manager_records.resize(5*0xb0c);g.manager_options.resize(20);g.manager_ratings.resize(240);
    const uint8_t x[]={0,5,5,5,5,10,10,10,10,17,17},y[]={4,1,3,5,7,1,3,5,7,3,5};
    for(size_t m=0;m<4;++m){g.manager_options[m*5]=1;for(size_t mode=0;mode<2;++mode)for(size_t f=0;f<8;++f)for(size_t r=0;r<11;++r){const auto at=m*0x160+mode*0xb0+f*22+r;g.formations[at]=x[r];g.formations[at+11]=y[r];}}
    g.memory.add_block(layout::EnvironmentYouthTeams,4*0x24);g.memory.add_block(layout::Desktops,4*0x360);g.memory.add_block(layout::Loans,4*0x78);return g;
}
LeImage rules(){auto r=LeImage::from_bytes(layout::GermanDataSegmentBegin,Bytes(0x6000));const auto set=[&](uint32_t a,uint8_t v){r.patch(a,&v,1);};for(uint32_t level=0;level<4;++level){set(layout::Career_BuildStadium_RuntimeField4+level,uint8_t(level+5));for(uint32_t i=0;i<22;++i)set(layout::Career_BuildStadium_RuntimeField+level*0x17+i,1);}for(uint32_t i=0;i<22;++i){set(layout::Career_BuildStadium_RuntimeField2+i*5,2);set(layout::Career_BuildStadium_RuntimeField3+i*5,5);}return r;}
CareerSnapshot base(const GameState& ref,const LeImage& source){
    CareerSnapshot s;s.world=bmh::test::world12::complete_world();s.world.players_initialized=true;s.world.markets.emplace();s.world.free_players.emplace();
    InitialStrengthRanges strength;for(auto& range:strength)range={20,70};GameRandom game(17);WatcomRandom random(91);const auto club=s.world.membership[0].clubs[0];
    add_initial_manager(s.world,{0,club,{0},"Host",0,1},{3,2,5,1,false},strength,ref,source,game,random);
    s.world.economy.emplace();s.world.managers[0].career->season_services.emplace();s.results=SeasonResults::empty(s.world.catalogue);SeasonCalendar calendar;calendar.days=366;
    for(const auto& league:s.world.catalogue.leagues)for(uint8_t round=1;round<=league.rounds();++round)calendar.leagues.push_back({uint16_t(10+round*7),league.id,round});
    s.calendar=CalendarCursor{calendar,0,0};s.desktop.emplace();s.desktop->active_seat=0;s.desktop->manager_order={0};s.desktop->matchday_number=17;s.desktop->initial_event=InitialDeskEvent{17,0,{0},true,{}};s.desktop->managers.push_back({0});s.transfers.emplace();s.game_seed=game.seed();s.watcom_seed=random.seed();s.validate();return s;
}
// Gives every player of a club the same form, technique and condition, so the club has that team strength.
void set_strength(World& world,ClubId club,uint32_t value){
    for(const auto& slot:world.clubs.at(club).roster)if(!slot.empty()){
        auto& w=world.players.at(slot.player).attributes.w35c;w=(w&~0x1fffffu)|value|(value<<7)|(value<<14);
    }
}
// The same career with every league match played: the home club scores (match number mod 4) goals, the
// away club (round mod 3). Only used as input of the recording, never validated as a running career.
CareerSnapshot played(CareerSnapshot s){
    for(const auto& league:s.world.catalogue.leagues){
        if(!league.active)continue;
        for(int round=1;round<=league.rounds();++round){
            const auto fixtures=league_fixtures(s.world,league.id,round);
            for(size_t match=0;match<fixtures.size();++match)s.results.result(s.world.catalogue,league.id,round,int(match))={true,uint8_t(match%4),uint8_t(round%3)};
        }
        s.league_rounds[league.id.value]=uint8_t(league.rounds());
    }
    return s;
}
}
int main()try{
    set_fixture_tables(bmh::test::authored_fixture_tables());   // league pairings without original data
    const auto ref=reference();const auto source=rules();

    // --- recording of a completed season ---
    auto closing=played(base(ref,source));
    const auto host=closing.world.managers[0].club;
    {   // one scorer with 9 goals in league 0 (goal counter = bits 2..7 of w364)
        auto& person=closing.world.players.at(closing.world.clubs.at(closing.world.membership[0].clubs[5]).roster[3].player).attributes;
        person.w364=(person.w364&~(63u<<2))|(9u<<2);person.name="Tor Mann";
    }
    const auto season=stage_almanac_season(closing);
    size_t active=0;for(const auto& league:closing.world.catalogue.leagues)if(league.active)++active;
    check(season.season==closing.season&&season.leagues.size()==active,"almanac does not cover every active league");
    for(const auto& league:season.leagues){
        const auto& rule=closing.world.catalogue.league(league.league);
        check(league.table.size()==rule.clubs,"almanac table has the wrong number of clubs");
        int goals_for=0,goals_against=0;
        for(size_t i=0;i<league.table.size();++i){
            const auto& row=league.table[i];
            check(row.games==rule.rounds()&&row.wins+row.draws+row.defeats==row.games,"almanac table row is incomplete");
            check(i==0||league.table[i-1].points>=row.points,"almanac table is not in final order");
            goals_for+=row.goals_for;goals_against+=row.goals_against;
        }
        check(goals_for==goals_against,"almanac table goals do not balance");
    }
    check(season.leagues[0].scorers.size()>=1&&season.leagues[0].scorers[0].name=="Tor Mann"&&season.leagues[0].scorers[0].goals==9&&
          season.leagues[0].scorers[0].club==closing.world.membership[0].clubs[5]&&season.leagues[0].scorers.size()<=kAlmanacScorers,"almanac missed the best scorer");

    auto world=closing.world;
    record_almanac_season(world,season,closing.world.managers,closing.world);
    check(world.extended&&world.extended->almanac.size()==1&&world.extended->manager_clubs.at(0).size()==1&&
          world.extended->manager_clubs.at(0)[0]==ManagerClubSeason{closing.season,host,{0}},"recording lost the season or the manager's club");
    auto next=season;next.season=season.season+1;record_almanac_season(world,next,closing.world.managers,closing.world);
    check(world.extended->almanac.size()==2&&world.extended->manager_clubs.at(0).size()==2,"second season was not appended");
    auto gap=season;gap.season=season.season+7;record_almanac_season(world,gap,closing.world.managers,closing.world);
    check(world.extended->almanac.size()==1&&world.extended->almanac[0].season==gap.season,"a gap did not restart the almanac");
    world.validate();

    // --- saved form ---
    world.extended->desk_icons.emplace(0,std::array<DeskIconPlace,2>{{{10,200},{60,150}}});
    world.validate();
    const auto bytes=encode_world(world);
    check(decode_world(bytes)==world,"extended features did not survive the world codec");
    check(desk_icon_places(world,0)[1]==DeskIconPlace{60,150}&&desk_icon_places(world,2)==kDefaultDeskIcons,"desk icon places are wrong");
    {   // a world of schema 44 must not carry the features, and loads without them
        auto old=cli::parse_json(std::string(bytes.begin(),bytes.end()));old.object()->at("version")=44;
        auto text=cli::dump_json(old);rejects([&]{decode_world(Bytes(text.begin(),text.end()));},"schema 44 accepted extended features");
        old.object()->erase("extended");text=cli::dump_json(old);
        check(!decode_world(Bytes(text.begin(),text.end())).extended,"schema 44 invented extended features");
    }
    {   auto bad=world;bad.extended->desk_icons[0][0]={300,10};rejects([&]{bad.validate();},"icon outside the desk accepted");
        bad=world;bad.extended->almanac[0].leagues[0].table[1].club=bad.extended->almanac[0].leagues[0].table[0].club;rejects([&]{bad.validate();},"repeated club in a final table accepted");
        bad=world;bad.extended->job_offers.emplace(0,std::vector<JobOffer>{{host}});rejects([&]{bad.validate();},"offer of a club with a manager accepted");
    }

    // --- job offer rule ---
    const AnnualSeasonTitles no_titles{closing.season,{}};
    {
        // The host's club is made the strongest and the champion of league 0: class "top".
        auto strong=closing;for(auto club:strong.world.membership[0].clubs)set_strength(strong.world,club,50);set_strength(strong.world,host,99);
        auto table=season;auto& rows=table.leagues[0].table;
        const auto at=std::find_if(rows.begin(),rows.end(),[&](const auto& row){return row.club==host;});std::rotate(rows.begin(),at,at+1);
        const auto facts=judge_manager(strong.world,table,no_titles,strong.world,0,false);
        check(facts.final_place==1&&facts.expected_place==1&&facts.champion&&facts.offer_class==JobOfferFacts::Class::top,"champion was not judged top");
        const auto offers=make_job_offers(strong.world,table,no_titles,strong.world,closing.season+1,{});
        check(offers.contains(0)&&!offers.at(0).empty()&&offers.at(0).size()<=kMaximumJobOffers,"champion received no offer");
        check(offers==make_job_offers(strong.world,table,no_titles,strong.world,closing.season+1,{}),"job offers are not deterministic");
        std::set<ClubId> seen;
        for(const auto& offer:offers.at(0)){
            const auto& club=strong.world.clubs.at(offer.club);
            check(offer.club!=host&&club.kind==ClubKind::club&&seen.insert(offer.club).second,"offer names the own club, no club or a club twice");
            const auto visible=strong.world.catalogue.player_leagues(club.country);
            check(std::find(visible.begin(),visible.end(),club.league)!=visible.end(),"offer from a league no manager may lead");
        }
        check(make_job_offers(strong.world,table,no_titles,strong.world,closing.season+1,{true,false,false,false}).empty(),"sanctioned manager received an offer");
        // Last place with the strongest squad: far below expectation, no offer.
        std::rotate(rows.begin(),rows.begin()+1,rows.end());
        const auto failed=judge_manager(strong.world,table,no_titles,strong.world,0,false);
        check(failed.final_place==int(rows.size())&&failed.offer_class==JobOfferFacts::Class::none,"last place of the strongest club was not judged none");
        // Exactly as expected: ordinary, one or two offers.
        auto even=closing;const auto order=even.world.membership[0].clubs;
        for(size_t i=0;i<order.size();++i)set_strength(even.world,order[i],uint32_t(90-i));
        set_strength(even.world,host,95);   // the host (somewhere in the membership order) is the strongest
        auto level=season;auto& sorted=level.leagues[0].table;
        std::sort(sorted.begin(),sorted.end(),[&](const auto& a,const auto& b){return club_strength(even.world,a.club)>club_strength(even.world,b.club);});
        const auto ordinary=judge_manager(even.world,level,no_titles,even.world,0,false);
        check(ordinary.offer_class==JobOfferFacts::Class::top&&ordinary.champion,"strongest club in first place is the champion");
        set_strength(even.world,host,uint32_t(90-(std::find(order.begin(),order.end(),host)-order.begin())));   // back into the row of the others
        std::sort(sorted.begin(),sorted.end(),[&](const auto& a,const auto& b){return club_strength(even.world,a.club)>club_strength(even.world,b.club);});
        const auto middle=judge_manager(even.world,level,no_titles,even.world,0,false);
        check(middle.offer_class==JobOfferFacts::Class::ordinary&&middle.expected_place==middle.final_place,"mid-table as expected was not judged ordinary");
        const auto few=make_job_offers(even.world,level,no_titles,even.world,closing.season+1,{});
        check(few.contains(0)&&few.at(0).size()>=1&&few.at(0).size()<=2,"ordinary season must bring one or two offers");
    }
    check(league_level(closing.world,{0})==0&&league_level(closing.world,{1})==1,"league levels are wrong");

    // --- change of club ---
    {
        auto state=base(ref,source);const auto target=state.world.membership[1].clubs[4];
        auto& manager=state.world.managers[0];
        (*manager.career->season_records)[0]=7;manager.career->annual_records->league_titles=3;(*manager.career->cup_titles)[1]=2;manager.match_state->best_streaks[2]=11;
        state.world.markets->stocks[0x50]=5;   // five shares of the first paper held by seat 0
        state.validate();
        check(manager_change_refusal(state,0)=="Es liegt kein Angebot vor.","change without an offer was not refused");
        const auto untouched=state;rejects([&]{change_manager_club(state,0,target,ref,source);},"change without an offer accepted");check(state==untouched,"refused change altered the career");
        state.world.extended.emplace();state.world.extended->job_offer_season=state.season;state.world.extended->job_offers.emplace(0,std::vector<JobOffer>{{target}});state.validate();
        check(manager_change_refusal(state,0).empty(),"valid offer was refused");
        const auto offered=state;const auto other=state.world.membership[1].clubs[5];
        rejects([&]{change_manager_club(state,0,other,ref,source);},"club without an offer accepted");check(state==offered,"refused club altered the career");
        const auto players=state.world.players;const auto members=state.world.membership;const auto old_finance=*state.world.managers[0].finance;
        const auto outcome=change_manager_club(state,0,target,ref,source);
        const auto& moved=state.world.managers[0];
        check(outcome.from==host&&outcome.to==target&&state.world.managers.size()==1&&moved.seat==0&&moved.club==target&&moved.start_league==LeagueId{1}&&moved.name=="Host"&&moved.portrait==0,"manager did not arrive at the new club as the same person");
        check((*moved.career->season_records)[0]==7&&moved.career->annual_records->league_titles==3&&(*moved.career->cup_titles)[1]==2&&moved.match_state->best_streaks[2]==11,"career record did not stay with the person");
        check(moved.finance&&moved.tactics&&moved.match_state,"new club has no personal state");
        check(state.world.markets->stocks[0x50]==0&&state.world.extended->job_offers.empty(),"shares or offers survived the new start");
        check(!state.world.clubs.at(host).presentation_category&&state.world.membership==members,"old club is not a plain computer club or leagues changed");
        auto same=state.world.players;for(auto& [id,p]:same)p.cards=players.at(id).cards;
        check(same==players,"change of club altered players");
        check(state.desktop->managers.size()==1&&state.desktop->active_seat==0&&state.desktop->manager_order==std::vector<uint8_t>({0}),"desk order changed");
        check(read_save(make_save(state))==state,"career after a change of club does not survive saving");
        const auto done=state;rejects([&]{change_manager_club(state,0,target,ref,source);},"offer accepted twice");check(state==done,"replayed change altered the career");
        (void)old_finance;
    }
    {   // two managers: the second one (seat 1, not at the desk) cannot change; the first keeps his list position
        auto state=base(ref,source);state.world.settings->planned_managers=2;
        add_running_manager(state,{1,state.world.membership[2].clubs[1],{2},"Guest",1,1},ref,source);
        const auto a=state.world.membership[0].clubs[7],b=state.world.membership[2].clubs[9];
        state.world.extended.emplace();state.world.extended->job_offer_season=state.season;
        state.world.extended->job_offers.emplace(0,std::vector<JobOffer>{{a}});state.world.extended->job_offers.emplace(1,std::vector<JobOffer>{{b}});state.validate();
        check(!manager_change_refusal(state,1).empty(),"manager who is not at the desk may change");
        const auto guest=state.world.managers[1];
        change_manager_club(state,0,a,ref,source);
        check(state.world.managers.size()==2&&state.world.managers[0].seat==0&&state.world.managers[0].club==a&&state.world.managers[1]==guest,"change disturbed the other manager or the list order");
        check(state.world.extended->job_offers.contains(1)&&!state.world.extended->job_offers.contains(0),"offers of the other manager were lost");
        check(read_save(make_save(state))==state,"two-manager career after a change does not survive saving");
    }
    std::cout<<"extended features: recording, saved form, job offer rule and change of club passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
