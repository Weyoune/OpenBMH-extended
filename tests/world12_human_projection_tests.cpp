// SPDX-License-Identifier: GPL-3.0-only
#include "world12_fixture.hpp"
#include "fixture_tables.hpp"
#include "game/world12/match_projection.hpp"
#include "game/world12/manager_tactics.hpp"
#include "game/world12/prediction.hpp"
#include "game/world12/codec.hpp"
#include "game/world12/ai_match.hpp"
#include "app/world12_ai_day.hpp"
#include "game/match.hpp"
#include "game/matchday.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
namespace {
using namespace bmh::world12;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class Function> void rejects(Function function,const char* message){bool rejected=false;try{function();}catch(const std::exception&){rejected=true;}check(rejected,message);}
bmh::GameState rules() {
    bmh::GameState state;state.club_ids.resize(445);state.team_flags.resize(445);state.team_morale.resize(445);
    state.formations.resize(0x580);state.manager_records.resize(4*0xb0c);state.manager_ratings.resize(4*60);state.tactics.resize(4*0x840);state.manager_options.resize(20);
    const uint8_t depth[]={0,5,5,5,5,10,10,10,10,17,17},lane[]={4,1,3,5,7,1,3,5,7,3,5};
    for(size_t m=0;m<4;++m){state.manager_options[m*5]=1;for(size_t mode=0;mode<2;++mode)for(size_t f=0;f<8;++f)for(size_t row=0;row<11;++row){const auto at=m*0x160+mode*0xb0+f*22+row;state.formations[at]=depth[row];state.formations[at+11]=lane[row];}}
    return state;
}
CareerSnapshot snapshot(const bmh::GameState& base) {
    CareerSnapshot snapshot;auto& w=snapshot.world;w=bmh::test::world12::complete_world();w.players_initialized=true;
    for(auto& [id,club]:w.clubs)for(size_t row=0;row<11;++row){auto& p=w.players.at(club.roster[row].player).attributes;p.w364=uint32_t(row==0?0:row<5?1:row<9?2:3);p.w35c=60u|(75u<<7)|(85u<<14);}
    const auto de=w.membership[4].clubs[0],en=w.membership[9].clubs[0];
    for(auto id:{de,en})for(uint32_t row=11;row<20;++row){PlayerRecord p;p.id={1000000+id.value*24+row};p.attributes={"Reserve",60u|(75u<<7)|(85u<<14),0,row==11?0u:1u+row%3};w.players[p.id]=p;w.clubs.at(id).roster[row]={p.id,uint16_t(row<17?2:0)};}
    w.managers={{0,"German",de,{4},0},{3,"English",en,{9},3}};bmh::WatcomRandom random(91);
    initialize_manager_contracts(w,0,base,{3,false,false,false,6},random);initialize_manager_contracts(w,3,base,{3,false,false,false,6},random);
    const auto history_a=w.membership[10].clubs[0],history_b=w.membership[11].clubs[0],outside=w.membership[12].clubs[0];
    w.managers[0].match_state->recent_fixtures[0]=history_a;w.managers[0].match_state->recent_fixtures[1]=history_b;
    std::array<uint8_t,10> scores;scores.fill(255);scores[0]=0x12;w.managers[0].match_state->head_to_head[outside]=scores;
    w.managers[0].match_state->designated_takers=true;w.managers[0].match_state->takers[0]=w.clubs.at(de).roster[1].player;
    w.managers[0].tactics->backing->marks[0]=w.clubs.at(outside).roster[0].player;
    snapshot.results=SeasonResults::empty(w.catalogue);snapshot.date=40;
    for(auto league:{LeagueId{4},LeagueId{9}}){snapshot.league_rounds[league.value]=2;for(int r=1;r<=2;++r)for(int m=0;m<w.catalogue.league(league).clubs/2;++m)snapshot.results.result(w.catalogue,league,r,m)={true,0,0};}
    snapshot.validate();return snapshot;
}
}
int main()try {
    bmh::set_fixture_tables(bmh::test::authored_fixture_tables());const auto base=rules();const auto source=snapshot(base);auto world=source.world;
    const auto de=world.membership[4].clubs[0],en=world.membership[9].clubs[0],de_other=world.membership[4].clubs[1],en_other=world.membership[9].clubs[1];
    const std::vector<ClubId> requested{de,de_other,en,en_other};
    rejects([&]{AiMatchProjection::create(world,requested,base);},"AI-only guard allowed a human participant");
    auto projection=AiMatchProjection::create_for_humans(source,requested,base,{3,6,false,true});
    check(projection.human_managers()==2&&projection.global_manager(0)==0&&projection.global_manager(1)==3,"sparse global manager seats lost in projection");
    check(projection.state.projected_match_clubs[0].league_size==18&&projection.state.projected_match_clubs[2].league_size==10,"same local league class mixed German/English sizes");
    check(bmh::table_position(projection.state,0)==projection.state.projected_match_clubs[0].table_rank&&bmh::table_position(projection.state,2)==projection.state.projected_match_clubs[2].table_rank,"human projected table rank missing");
    std::array<uint8_t,24> ranks;int size=-71;bmh::table_prediction(projection.state,2,1,ranks,&size);const auto expected=predict_league(source,en,1);
    check(size==10&&ranks==expected.result.ranks,"English forecast fell back to German positional tables");
    rejects([&]{bmh::table_prediction(projection.state,2,99,ranks);},"missing projected mode silently used legacy league");
    bmh::MatchSetup setup;setup.home=0;setup.away=2;setup.month=8;setup.temperature=19;setup.spectators=15000;setup.climate_effect=true;
    bmh::GameRandom game(17);bmh::WatcomRandom random(91);const auto sides=bmh::build_side_records(projection.state,setup,random,&game);
    check(game.calls()>0&&random.calls()>0&&sides[0].b.size()==9,"human Source ratings did not execute");
    const auto balance=world.managers[0].finance->balance;check(bmh::book_money(projection.state,0,100,true,true),"human payment could not use projected account");
    projection.state.memory.set16(bmh::layout::DeskRecords_Teams_RuntimeField,123);
    projection.state.manager_ratings[60]=133;projection.state.manager_records[0x1ae]=7;
    projection.state.manager_results[size_t(projection.local_club(de_other))*10]=0x31;
    const auto player=world.clubs.at(de).roster[0].player;projection.state.player_cards.at(projection.local_player(player))=0x8765;
    projection.commit_match_state(world);
    check(world.managers[0].finance->balance==balance-100&&world.managers[0].match_state->timing_stats[0]==7&&world.managers[1].match_state->rating_history[0]==133,"human account/statistic/rating data lost on commit");
    check(world.managers[0].match_state->head_to_head.at(de_other)[0]==0x31&&world.managers[0].match_state->head_to_head.contains(world.membership[12].clubs[0]),"mapped opponent update discarded unmapped history");
    check(world.clubs.at(de).fairplay_points==123&&world.players.at(player).cards==0x8765&&world.managers[0].tactics->backing->marks[0]==source.world.managers[0].tactics->backing->marks[0],"card/mark references lost identity");
    check(decode_world(encode_world(world))==world,"committed human state failed codec roundtrip");
    auto stale=source.world;auto late=AiMatchProjection::create_for_humans(source,requested,base,{3,6,false,true});++stale.managers[1].finance->income;const auto externally_changed=stale;
    rejects([&]{late.commit_match_state(stale);},"late human finance change was overwritten");check(stale==externally_changed,"late human rejection overwrote external state");
    auto invalid=AiMatchProjection::create_for_humans(source,requested,base,{3,6,false,true});invalid.state.manager_records[0x292]=0xfe;invalid.state.manager_records[0x293]=0;
    auto unchanged=source.world;rejects([&]{invalid.commit_match_state(unchanged);},"invalid local recent club accepted");check(unchanged==source.world,"late manager reference failure partially published players");
    auto forged_owner=AiMatchProjection::create_for_humans(source,requested,base,{3,6,false,true});forged_owner.state.manager_records[0x290]=1;
    auto owner_world=source.world;rejects([&]{forged_owner.commit_match_state(owner_world);},"manager record team diverged from typed owner");check(owner_world==source.world,"forged record identity partially committed");
    auto missing=source;missing.world.managers[0].match_state.reset();rejects([&]{AiMatchProjection::create_for_humans(missing,requested,base,{3,6,false,true});},"missing human runtime state simulated as AI");
    {
        auto four=source;four.world.managers.push_back({1,"GermanTwo",de_other,{4},1});four.world.managers.push_back({2,"EnglishTwo",en_other,{9},2});
        bmh::WatcomRandom initial(123);initialize_manager_contracts(four.world,1,base,{3,false,false,false,6},initial);initialize_manager_contracts(four.world,2,base,{3,false,false,false,6},initial);
        auto all=AiMatchProjection::create_for_humans(four,requested,base,{3,6,false,true});check(all.human_managers()==4,"four-manager group lost a participant");
        bmh::GameRandom game(17);bmh::WatcomRandom random(91);bmh::MatchSetup setup;setup.home=0;setup.away=1;setup.month=8;setup.temperature=19;setup.spectators=15000;
        bmh::build_side_records(all.state,setup,random,&game);setup.home=2;setup.away=3;bmh::build_side_records(all.state,setup,random,&game);
        auto copied=four.world;all.commit_match_state(copied);check(copied.managers.size()==4&&decode_world(encode_world(copied))==copied,"four-manager projection/commit lost complete state");
    }
    size_t total_matches=0,events=0;int total_goals=0;
    for(auto league:{LeagueId{4},LeagueId{9}})for(int seed:{17,34,51,68}) {
        auto staged=source,repeated=source;const auto original=staged;const auto fixtures=league_fixtures(staged.world,league,3);
        AiLeagueConditions conditions;conditions.month=8;conditions.temperature=19;conditions.rain=0;conditions.date=staged.date;conditions.spectators=15000;
        conditions.referee_properties={24};conditions.presentation_buttons.assign(0xb4,1);
        std::vector<AiLeagueConditions> inputs(fixtures.size(),conditions);
        bmh::GameRandom a(seed),b(seed);bmh::WatcomRandom ar(91),br(91);int minutes=0;
        HumanRoundHooks hooks;hooks.live_event=[&](auto&,auto&,const auto&){++events;};hooks.after_minute=[&](auto&,auto&,int minute){minutes=std::max(minutes,minute);};
        const auto result=simulate_and_settle_human_league_round(staged,base,league,3,inputs,{3,6,false,true},a,ar,hooks);
        const auto repeat=simulate_and_settle_human_league_round(repeated,base,league,3,inputs,{3,6,false,true},b,br,hooks);
        check(staged==repeated&&a.seed()==b.seed()&&ar.seed()==br.seed()&&result.game_draws==repeat.game_draws,"human Source round changed on repeat");
        check(minutes>=90&&result.matches.size()==fixtures.size()&&result.game_draws>0&&result.watcom_draws>0,"human Source round did not play");
        for(const auto& match:result.matches){check(match.settled&&match.score.played&&match.end_minute>=90&&match.end_minute<=94,"human match did not reach real full time");total_goals+=match.score.home+match.score.away;++total_matches;}
        check(staged.date==original.date&&staged.results==original.results&&staged.league_rounds==original.league_rounds,"calculation bypassed the pending day/result owner");
        check(read_save(make_save(staged))==staged,"human scored state lost salary/statistics/history on snapshot reload");
    }
    check(total_goals>0,"human round tests manufactured empty outcomes");
    {
        auto interrupted=source;const auto fixtures=league_fixtures(interrupted.world,{4},3);AiLeagueConditions c;c.month=8;c.temperature=19;c.date=40;c.spectators=15000;c.referee_properties={24};c.presentation_buttons.assign(0xb4,1);
        std::vector<AiLeagueConditions> inputs(fixtures.size(),c);bmh::GameRandom game(17);bmh::WatcomRandom random(91);
        HumanRoundHooks hooks;bool changed=false;hooks.live_event=[](auto&,auto&,const auto&){};
        hooks.after_minute=[&](auto&,auto&,int){if(!changed){changed=true;++interrupted.date;}};
        rejects([&]{simulate_and_settle_human_league_round(interrupted,base,{4},3,inputs,{3,6,false,true},game,random,hooks);},"external round/calendar change overwritten");
        auto external=source;++external.date;check(interrupted==external&&game.calls()==0&&random.calls()==0,"human round rejection leaked candidate/RNG or lost external state");
    }
    {
        namespace app=bmh::app::world12;
        auto day=source;day.results=SeasonResults::empty(day.world.catalogue);day.league_rounds.fill(0);
        std::vector<uint16_t> de(365),en(365);for(int r=0;r<34;++r)de[size_t(20+r*8)]=15;for(int r=0;r<38;++r)en[size_t(20+r*8)]=15;
        day.calendar=CalendarCursor{SeasonCalendar::from_sources(day.world.catalogue,de,en),20,0};day.game_seed=17;day.watcom_seed=91;
        const auto ticket=*prepare_day(day);std::vector<app::AiDayLeagueConditions> inputs;
        AiLeagueConditions c;c.month=8;c.temperature=19;c.date=day.date;c.spectators=15000;c.referee_properties={24};c.presentation_buttons.assign(0xb4,1);
        for(const auto& league:ticket.leagues)inputs.push_back({league.appointment,std::vector<AiLeagueConditions>(league.matches.size(),c)});
        const auto before=day;app::AiDayOwner owner{71,0};HumanRoundHooks hooks;hooks.live_event=[](auto&,auto&,const auto&){};
        rejects([&]{app::play_ai_league_day(day,owner,{71,0},ticket,base,inputs);},"AI day absorbed a human game");check(day==before&&owner.revision==0,"AI-only human rejection mutated day");
        const auto played=app::play_league_day(day,owner,{71,0},ticket,base,inputs,{3,6,false,true},hooks);
        check(day.date==before.date+1&&day.calendar->next_day==21&&owner.revision==1&&day.league_rounds[4]==1&&played.leagues.size()==ticket.leagues.size(),"mixed human/AI day did not publish one complete transition");
        check(read_save(make_save(day))==day,"committed mixed day lost calendar/results/manager data");const auto after=day;
        rejects([&]{app::play_league_day(day,owner,{71,0},ticket,base,inputs,{3,6,false,true},hooks);},"mixed day replay accepted");check(day==after&&owner.revision==1,"mixed replay changed completed day");
        auto failed=before;app::AiDayOwner failed_owner{72,0};inputs.back().matches.back().month=0;
        rejects([&]{app::play_league_day(failed,failed_owner,{72,0},ticket,base,inputs,{3,6,false,true},hooks);},"bad final group committed earlier human match");check(failed==before&&failed_owner.revision==0,"late group failure leaked mixed day records/RNG/date");
        std::cout<<"Complete mixed human/AI league day, replay/snapshot and late-group atomic rollback passed\n";
    }
    std::cout<<total_matches<<" actual human/AI league fixtures, goals="<<total_goals<<", presented human events="<<events<<"; repeat/snapshot/calendar-owner checks passed\n";
    std::cout<<"Mixed human Source ratings, sparse seats, country-specific forecasts, accounts/stats/history/global references and late rollback passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
