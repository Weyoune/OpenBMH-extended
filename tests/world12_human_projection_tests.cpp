// SPDX-License-Identifier: GPL-3.0-only
#include "world12_fixture.hpp"
#include "fixture_tables.hpp"
#include "game/world12/match_projection.hpp"
#include "game/world12/manager_tactics.hpp"
#include "game/world12/prediction.hpp"
#include "game/world12/codec.hpp"
#include "game/world12/ai_match.hpp"
#include "game/world12/economy.hpp"
#include "game/messages.hpp"
#include "game/driver.hpp"
#include "app/world12_ai_day.hpp"
#include "app/world12_european.hpp"
#include "app/world12_live_scope.hpp"
#include "app/career_session.hpp"
#include "game/match.hpp"
#include "game/lineup.hpp"
#include "game/matchday.hpp"
#include "game/matchday_setup.hpp"
#include "ui/lineup.hpp"
#include "ui/statistics_rankings.hpp"
#include "cli/json.hpp"
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
void short_squad_source_kickoff(const bmh::GameState& base) {
    for(int unavailable_side:{0,1,2}) {
        auto initial=snapshot(base);const LeagueId league{4};const auto fixtures=league_fixtures(initial.world,league,3);
        // Every fixture forfeits; the first covers home, away and both-short cases.
        for(size_t m=0;m<fixtures.size();++m)for(int side=0;side<2;++side) {
            if(m==0&&unavailable_side!=2&&side!=unavailable_side)continue;
            auto& club=initial.world.clubs.at(side?fixtures[m].away:fixtures[m].home);
            size_t fit=0;for(const auto& row:club.roster)if(!row.empty()) {
                auto& p=initial.world.players.at(row.player).attributes;
                if(fit++>=7)p.w364|=2u<<16;
            }
        }
        std::vector<ClubId> participants;for(const auto& f:fixtures){participants.push_back(f.home);participants.push_back(f.away);}
        auto expected=initial;auto oracle=AiMatchProjection::create_for_humans(initial,participants,base,{3,6,false,true});
        auto& local=oracle.state;local.date=initial.date;local.indoor=false;local.lineups_frozen=false;local.pools_open=false;
        for(size_t n=0;n<participants.size();++n){const auto team=int(oracle.local_club(participants[n]));bmh::prepare_team(local,team,0,int(n));bmh::auto_lineup(local,team,local.manager_of(team)>=0?0xe4:100);if(local.manager_of(team)>=0)bmh::prepare_lineup_entry(local,team);}
        const auto source_league=bmh::match_team_league(local,int(oracle.local_club(fixtures[0].home)));
        local.league_rounds.assign(9,0);local.results.assign(9*0x17c,0xcc);
        bmh::MatchGroup group;group.desc.assign(0xd0,0);group.desc[1]=2;group.desc[2]=uint8_t(fixtures.size());for(size_t b=0;b<4;++b)group.desc[0xcc+b]=uint8_t(source_league>>(8*b));
        for(const auto& f:fixtures){group.fixtures.push_back(int(oracle.local_club(f.home)));group.fixtures.push_back(int(oracle.local_club(f.away)));}
        std::vector<bmh::MatchGroup> groups{group};bmh::GameRandom eg(17),ag(17);bmh::WatcomRandom ew(91),aw(91);bmh::MinuteReport report;bmh::MatchdayContext context;context.month=8;
        bmh::kick_off(local,groups,context,ew,eg,report);check(!report.unsupported,"Source short-squad kickoff failed");
        check(groups[0].kickoff_void_mask==((1u<<fixtures.size())-1),"Source did not exclude every short fixture");
        bmh::MatchEngine native(local,groups,eg,ew,{24});native.month=8;native.buttons.assign(180,1);native.leave_screens=true;
        bmh::play_matchday(native,local,groups,native.buttons,eg,ew,report);bmh::process_group(local,groups[0],eg,ew,report);check(!report.unsupported,"Source short-squad clock/settlement failed");
        oracle.commit_match_state(expected.world);expected.game_seed=eg.seed();expected.watcom_seed=ew.seed();
        AiLeagueConditions c;c.date=initial.date;c.month=8;c.temperature=19;c.spectators=15000;c.referee_properties={24};c.presentation_buttons.assign(180,1);
        auto actual=initial;HumanRoundHooks hooks;hooks.decline_unpresented_events=true;
        const auto result=simulate_and_settle_human_league_round(actual,base,league,3,std::vector<AiLeagueConditions>(fixtures.size(),c),{3,6,false,true},ag,aw,hooks);
        if(actual!=expected||ag.seed()!=eg.seed()||aw.seed()!=ew.seed()||ag.calls()!=eg.calls()||aw.calls()!=ew.calls()){
            std::cerr<<"forfeit side="<<unavailable_side<<" G="<<ag.seed()<<"/"<<eg.seed()<<" calls="<<ag.calls()<<"/"<<eg.calls()<<" W="<<aw.seed()<<"/"<<ew.seed()<<" calls="<<aw.calls()<<"/"<<ew.calls()<<" players="<<(actual.world.players==expected.world.players)<<" clubs="<<(actual.world.clubs==expected.world.clubs)<<" managers="<<(actual.world.managers==expected.world.managers)<<" stats="<<(actual.world.player_statistics==expected.world.player_statistics)<<'\n';
            for(size_t n=0;n<actual.world.managers.size();++n)if(actual.world.managers[n]!=expected.world.managers[n])std::cerr<<"manager="<<n<<" finance="<<(actual.world.managers[n].finance==expected.world.managers[n].finance)<<" tactics="<<(actual.world.managers[n].tactics==expected.world.managers[n].tactics)<<" match="<<(actual.world.managers[n].match_state==expected.world.managers[n].match_state)<<" career="<<(actual.world.managers[n].career==expected.world.managers[n].career)<<'\n';
        }
        check(actual==expected&&ag.seed()==eg.seed()&&aw.seed()==ew.seed()&&ag.calls()==eg.calls()&&aw.calls()==ew.calls(),"typed forfeit differs from actual Source kickoff/fines/restoration/settlement/RNG");
        for(size_t m=0;m<fixtures.size();++m)check(result.matches[m].end_minute==0&&result.matches[m].score.home==(groups[0].results[m]>>4)&&result.matches[m].score.away==(groups[0].results[m]&15),"typed forfeit invented play time or a different Source award");
        check(read_save(make_save(actual))==actual,"forfeit discipline/fines/player restoration lost on save");
    }
    std::cout<<"Actual Source short-squad kickoff: home/away/both, fine, discipline, restored squads, zero minutes, excluded settlement and exact GW passed\n";
}
void annual_donor_workspace(const bmh::GameState& base) {
    const auto source=snapshot(base);
    for(auto country:{Country::germany,Country::england}) {
        const auto projection=AiMatchProjection::create_for_annual_accounts(source,country,base,{3,6,false,true});
        check(projection.human_managers()==2,"annual donor search dropped another country's human manager");
        size_t local=0;
        for(auto league:source.world.catalogue.simulation_leagues(country==Country::germany?TableGroup::germany:TableGroup::england))
            for(auto club:source.world.membership[league.value].clubs)check(projection.global_club(uint32_t(local++))==club,"annual donor search changed physical country slot order");
        check(local==90,"annual donor workspace lost Source's 90 actual clubs");
        auto roundtrip=source.world;projection.commit_match_state(roundtrip);
        check(roundtrip==source.world,"unchanged annual projection changed real rosters or sparse human backing");
    }
    rejects([&]{AiMatchProjection::create_for_annual_accounts(source,Country::spain,base,{3,6,false,true});},"foreign country entered domestic academy donor search");
    std::vector<ClubId> too_many;for(auto club:source.world.membership[0].clubs)too_many.push_back(club);
    for(auto league:{1,2,3})for(auto club:source.world.membership[size_t(league)].clubs)too_many.push_back(club);
    rejects([&]{AiMatchProjection::create_for_humans(source,too_many,base,{3,6,false,true});},"annual workspace widened the match factory");
}
void live_preferences_and_scopes(const bmh::GameState& base) {
    auto saved=snapshot(base);saved.world.markets.emplace();saved.world.free_players.emplace();saved.world.settings=CareerSettings{3,6,5,2,false};
    for(auto& manager:saved.world.managers){manager.career.emplace();manager.career->season_records.emplace();}
    saved.desktop.emplace();ManagerDesktop de_desk;de_desk.seat=0;de_desk.display_word=0x1234;
    ManagerDesktop en_desk;en_desk.seat=3;en_desk.display_word=0x5678;
    // Storage order deliberately differs from local manager order.
    saved.desktop->managers={en_desk,de_desk};saved.validate();
    const auto de=saved.world.managers[0].club,en=saved.world.managers[1].club;
    auto mapped=AiMatchProjection::create_for_humans(saved,{en,de},base,{3,6,false,true});
    check(mapped.state.memory.u16(bmh::layout::CareerSession_AnyOf_ExecutableField)==0x1234&&
          mapped.state.memory.u16(bmh::layout::CareerSession_AnyOf_ExecutableField+2)==0x5678,
          "human projection lost canonical sparse desktop display words");
    mapped.state.memory.set16(bmh::layout::CareerSession_AnyOf_ExecutableField+2,0x9abc);
    mapped.commit_desktop_preferences(saved);
    check(saved.desktop->managers[0].seat==3&&saved.desktop->managers[0].display_word==0x9abc&&
          saved.desktop->managers[1].display_word==0x1234,
          "match preference capture used local seats as canonical desktop owners");
    check(read_save(make_save(saved))==saved,"live display preferences did not survive save/reopen");

    auto rules=bmh::LeImage::from_bytes(bmh::layout::GermanDataSegmentBegin,bmh::Bytes(0x20000));
    const uint8_t maximum=3;rules.patch(bmh::layout::CareerSession_DispatchLiveSubstitution_ExecutableField,&maximum,1);
    auto played=saved;const auto fixtures=league_fixtures(played.world,{4},3);
    AiLeagueConditions conditions;conditions.month=8;conditions.temperature=19;conditions.date=40;
    conditions.spectators=15000;conditions.referee_properties={24};conditions.presentation_buttons.assign(0xb4,1);
    std::vector<AiLeagueConditions> inputs(fixtures.size(),conditions);
    bmh::GameRandom game(17);bmh::WatcomRandom random(91);uint64_t nonce=0;bool checked=false;
    HumanRoundHooks hooks;hooks.live_event=[](auto&,auto&,const auto&){};
    hooks.before_matchday=[](auto& projection,auto& engine){
        for(const auto& group:engine.groups)for(int match=0;match<group.matches();++match)
            bmh::select_live_match_source(projection.state,group.fixtures.at(size_t(match)*2),group.fixtures.at(size_t(match)*2+1));
    };
    bool owned_seen=false;
    hooks.before_owned=[&](auto& projection,auto& engine,const auto& owned){
        check(&projection.state==&engine.state&&owned==saved&&!projection.state.live_matches.empty(),
              "live owned hook did not receive the exact staged source after observation setup");owned_seen=true;
    };
    hooks.after_minute=[&](auto& projection,auto& engine,int minute){
        if(checked||minute!=15)return;checked=true;
        check(owned_seen,"live minute preceded its staged snapshot context");
        // This manager is not in the current German match workspace. A real
        // overview can still retain that canonical viewer's explicit UI words.
        projection.retain_live_preferences(3,uint16_t(0x3411),uint16_t(0x91ff),uint8_t(0x21));
        rejects([&]{projection.retain_live_preferences(2,uint16_t(0x11),{},{});},
                "live UI accepted an absent canonical manager");
        const auto team=int(projection.local_club(de));const auto before=projection.state;const auto before_projection=projection;
        const auto game_seed=engine.game.seed();const auto watcom_seed=engine.rand.seed();
        const auto range=bmh::squad_range(team);
        bmh::ui::LineupRequest action;action.kind=bmh::ui::LineupRequestKind::swap_players;
        action.manager_slot=projection.state.manager_of(team);action.runtime_club=team;
        action.first_row=3;action.second_row=12;
        action.first_slot=before.squad_slots.at(range.first+3);action.second_slot=before.squad_slots.at(range.first+12);
        action.first_player=action.first_slot&0xfff;action.second_player=action.second_slot&0xfff;
        {
            bmh::app::world12::LiveScope scope(projection,engine,rules,minute,team,2,nonce);
            action.revision=scope.revision();
            auto stale=action;++stale.revision;
            check(!scope.apply(stale).applied,"live scope accepted a stale action");
            const auto result=scope.apply(action);
            check(result.applied,"actual human live substitution rejected");
            check(!scope.apply(action).applied,"live scope accepted a replayed row action");
            auto forbidden=bmh::ui::PitchRequest{};forbidden.kind=bmh::ui::PitchRequestKind::apply_formation;
            forbidden.manager_slot=action.manager_slot;forbidden.runtime_club=team;forbidden.revision=scope.revision();
            check(!scope.apply(forbidden).applied,"live scope exposed formation automation");
            auto services=scope.player_services(played.world);check(bool(services.retain_player_statistics_flags),"actual card flags lack an owned-write capability");services.retain_player_statistics_flags(0x55);
            // Destruction aborts the unfinished editor. No calendar or random
            // work may escape this nested synchronous capability.
        }
        check(projection.state.squad_slots==before.squad_slots&&projection.state.team_flags==before.team_flags&&
              projection.state.tactics==before.tactics&&projection.state.manager_records==before.manager_records&&
              projection.state.memory.u8(bmh::layout::ActiveManager)==before.memory.u8(bmh::layout::ActiveManager)&&
              engine.game.seed()==game_seed&&engine.rand.seed()==watcom_seed,
              "aborted live editor leaked a substitution, selected actor or random draw");
        {auto expected=saved.world,actual=saved.world;before_projection.commit_match_state(expected);projection.commit_match_state(actual);check(actual==expected,"aborted card editor leaked retained preference metadata after restoring Source state");}
        rejects([&]{bmh::app::world12::LiveScope early(projection,engine,rules,2,team,2,nonce);},
                "manual intervention admitted the Source opening lock");
        {
            bmh::app::world12::LiveScope scope(projection,engine,rules,minute,team,2,nonce);
            const auto old_revision=scope.revision();auto services=scope.player_services(played.world);services.retain_player_statistics_flags(0x5a);scope.finish();rejects([&]{services.retain_player_statistics_flags(0x66);},"retired card preference capability wrote another value");
            check(nonce>old_revision,"finished live scope did not retire its numeric capability");
            rejects([&]{scope.finish();},"finished live scope could rerate twice");
            check(!scope.apply(action).applied,"retired live scope could accept actions");
        }
    };
    const auto result=simulate_and_settle_human_league_round(played,base,{4},3,inputs,{3,6,false,true},game,random,hooks);
    check(played.world.managers[0].career->player_statistics_flags==0x5a,"accepted real live card preference was not retained for its canonical owner");
    check(checked&&!result.matches.empty()&&played.desktop->managers==saved.desktop->managers,
          "live scope probe missed a real minute or lost another manager's display word");
    const auto& retained=*played.world.managers[1].career;
    check(retained.table_display_word==0x3411&&retained.strengths_display_word==0x91ff&&retained.pairings_display_byte==0x21&&
          played.world.managers[1].finance==saved.world.managers[1].finance,
          "nonparticipating viewer preferences were lost or changed that manager's finance");
    check(read_save(make_save(played))==played,"closed live preferences did not survive complete save/reload");
    std::cout<<"Real human minute scope: stale/replayed actions, substitution abort, actor/RNG rollback, Source opening lock, retirement and sparse desktop preferences passed\n";
}
void known_economy_match_oracle(const bmh::GameState& base){
    auto initial=snapshot(base);initial.world.markets.emplace();initial.world.free_players.emplace();initial.world.settings=CareerSettings{3,6,5,2,false};initial.world.economy.emplace();auto& economy=*initial.world.economy;economy.week=2;economy.presentation_options.emplace();economy.presentation_options->fill(1);
    for(size_t seat=0;seat<4;++seat){auto& manager=economy.managers[seat];manager.deposit=uint32_t(7000+seat*500);manager.monthly.emplace();manager.monthly->history_count=1;manager.depreciation=uint32_t(80+seat);for(size_t n=0;n<15;++n)manager.monthly->counters[n]=uint32_t(10000+seat*1000+n*13);manager.monthly->expenses={uint32_t(111+seat),uint32_t(222+seat)};manager.financial_history[0][0]=uint32_t(400+seat);}
    EconomyMessage kept;kept.recipients=9;kept.date=39;const std::string original="Fixture retained mailbox entry";std::copy(original.begin(),original.end(),kept.text.begin());economy.messages={kept};
    initial.world.economy->managers[0].monthly.reset();initial.world.economy->managers[0].depreciation.reset();initial.validate();
    for(auto league:{LeagueId{4},LeagueId{9}}){auto played=initial;std::optional<CareerSnapshot> expected;std::optional<int32_t> expected_game;std::optional<uint32_t> expected_watcom;const auto fixtures=league_fixtures(played.world,league,3);AiLeagueConditions condition;condition.date=played.date;condition.month=8;condition.temperature=19;condition.spectators=15000;condition.referee_properties={24};condition.presentation_buttons.assign(180,1);std::vector<AiLeagueConditions> conditions(fixtures.size(),condition);bmh::GameRandom game(17);bmh::WatcomRandom random(91);HumanRoundHooks hooks;hooks.decline_unpresented_events=true;
        hooks.before_matchday=[&](auto& projection,auto& engine){
            bmh::post_composed(projection.state,0,"Fixture Source match mailbox mutation");auto oracle=projection;auto native_groups=engine.groups;bmh::GameRandom native_game=engine.game;bmh::WatcomRandom native_random=engine.rand;bmh::MatchEngine native(oracle.state,native_groups,native_game,native_random,engine.referees);native.month=engine.month;native.buttons=engine.buttons;native.leave_screens=true;bmh::MinuteReport report;bmh::play_matchday(native,oracle.state,native_groups,native.buttons,native_game,native_random,report);check(!report.unsupported,"direct Source economy oracle could not play");for(auto& group:native_groups)bmh::process_group(oracle.state,group,native_game,native_random,report);check(!report.unsupported,"direct Source economy oracle could not settle");expected=initial;oracle.commit_match_state(expected->world);capture_career_economy(expected->world,oracle);expected->game_seed=native_game.seed();expected->watcom_seed=native_random.seed();expected_game=native_game.seed();expected_watcom=native_random.seed();
        };
        simulate_and_settle_human_league_round(played,base,league,3,conditions,{3,6,false,true},game,random,hooks);check(expected&&played.world==expected->world&&game.seed()==*expected_game&&random.seed()==*expected_watcom,"common match lost direct Core month/mailbox/finance effects or RNG");check(played.world.economy->messages.size()>initial.world.economy->messages.size(),"Source match mailbox mutation was discarded");check(!played.world.economy->managers[0].monthly&&!played.world.economy->managers[0].depreciation,"known economy capture invented older unknown manager fields");check(played.world.economy->managers[1]==initial.world.economy->managers[1]&&played.world.economy->managers[2]==initial.world.economy->managers[2],"match captured an unrelated canonical seat's economy");check(read_save(make_save(played))==played,"Source match month/mailbox effects did not survive save/reopen");
        auto stale=initial;const auto before=stale;bmh::GameRandom stale_game(17);bmh::WatcomRandom stale_random(91);HumanRoundHooks interrupted;interrupted.decline_unpresented_events=true;bool changed=false;interrupted.after_minute=[&](auto&,auto&,int){if(!changed){changed=true;++stale.world.economy->managers[3].deposit;}};rejects([&]{simulate_and_settle_human_league_round(stale,base,league,3,conditions,{3,6,false,true},stale_game,stale_random,interrupted);},"external economy change was overwritten");auto external=before;++external.world.economy->managers[3].deposit;check(stale==external&&stale_game.calls()==0&&stale_random.calls()==0,"economy owner rejection rewound external state or leaked match RNG");
    }
    std::cout<<"Known Source economy: full month/mailbox and sparse-seat Core oracles, unknown preservation and late-owner rollback passed\n";
}

void shared_human_clock(const bmh::GameState& base) {
    for(const auto leagues:{std::vector<LeagueId>{{0},{5}},std::vector<LeagueId>{{0},{5},{6},{7}},std::vector<LeagueId>{{5},{6},{7},{8}}}) {
        auto initial=snapshot(base);auto& w=initial.world;w.managers.clear();initial.league_rounds.fill(0);initial.results=SeasonResults::empty(w.catalogue);
        w.markets.emplace();w.free_players.emplace();w.settings=CareerSettings{3,6,5,uint8_t(leagues.size()),false};
        w.economy.emplace();w.economy->presentation_options.emplace();w.economy->presentation_options->fill(1);
        for(size_t seat=0;seat<4;++seat){auto& finance=w.economy->managers[seat];finance.deposit=uint32_t(7000+seat);finance.monthly.emplace();finance.monthly->history_count=1;finance.monthly->counters[0]=uint32_t(10000+seat);finance.depreciation=80;}
        uint32_t player_id=3000000;
        // Exercise the real maximum: four English groups have 80 clubs/1920 players.
        for(auto league:leagues)for(auto club:w.membership[league.value].clubs)for(size_t row=11;row<24;++row){
            auto& slot=w.clubs.at(club).roster[row];if(!slot.empty())continue;
            PlayerRecord player;player.id={player_id++};player.attributes={"Extra",60u|(75u<<7)|(85u<<14),0,uint32_t(row%4)};
            w.players[player.id]=player;slot={player.id,uint16_t(row<17?2:0)};
        }
        bmh::WatcomRandom contracts(91);initial.desktop.emplace();
        for(size_t n=0;n<leagues.size();++n){const uint8_t seat=leagues.size()==2&&n==1?3:uint8_t(n);const auto club=w.membership[leagues[n].value].clubs[0];
            w.managers.push_back({seat,"Manager",club,leagues[n],seat});initialize_manager_contracts(w,seat,base,{3,false,false,false,6},contracts);
            w.managers.back().career.emplace();w.managers.back().career->season_records.emplace();ManagerDesktop desk;desk.seat=seat;desk.display_word=uint16_t(0x1200+seat);initial.desktop->managers.push_back(desk);
        }
        initial.game_seed=17;initial.watcom_seed=91;initial.validate();
        AiLeagueConditions condition;condition.date=initial.date;condition.month=8;condition.temperature=19;condition.spectators=15000;condition.referee_properties={24};condition.presentation_buttons.assign(180,1);
        std::vector<HumanLeagueRound> inputs;for(auto league:leagues)inputs.push_back({league,1,std::vector<AiLeagueConditions>(w.catalogue.league(league).clubs/2,condition)});
        auto played=initial;std::optional<CareerSnapshot> expected;std::vector<bmh::MatchGroup> source_groups;uint64_t expected_g=0,expected_w=0;
        bmh::GameRandom game(17);bmh::WatcomRandom random(91);HumanRoundHooks hooks;hooks.decline_unpresented_events=true;
        const bmh::MatchGroup* storage=nullptr;std::vector<const uint8_t*> descriptors;int last_minute=0,starts=0,owned_calls=0;
        hooks.before_matchday=[&](auto& projection,auto& engine){
            check(engine.groups.size()==leagues.size()&&projection.human_managers()==leagues.size(),"shared clock lost a human league/manager");
            storage=engine.groups.data();size_t clubs=0;
            for(size_t i=0;i<leagues.size();++i){const auto& group=engine.groups[i];descriptors.push_back(group.desc.data());const auto fixtures=league_fixtures(initial.world,leagues[i],1);
                check(group.matches()==int(fixtures.size())&&group.desc[0]==0&&group.desc[1]==2,"shared group lost Source league flags");
                const auto local=projection.local_club(fixtures[0].home);check((uint32_t(group.desc[0xcc])|(uint32_t(group.desc[0xcd])<<8)|(uint32_t(group.desc[0xce])<<16)|(uint32_t(group.desc[0xcf])<<24))==uint32_t(bmh::match_team_league(projection.state,int(local))),"canonical league ID leaked into Source calculator class");
                for(size_t m=0;m<fixtures.size();++m)check(projection.global_club(group.fixtures[m*2])==fixtures[m].home&&projection.global_club(group.fixtures[m*2+1])==fixtures[m].away,"shared group changed canonical fixture/order");
                clubs+=fixtures.size()*2;
            }
            check(projection.state.players.size()==clubs*24,"shared projection lost complete roster/player mapping");
            bmh::post_composed(projection.state,0,"Shared clock Source mailbox oracle");
            for(const auto& manager:initial.world.managers)projection.retain_live_preferences(manager.seat,uint16_t(0x3411+manager.seat),uint16_t(0x91f0+manager.seat),uint8_t(0x20+manager.seat));
            auto oracle=projection;source_groups=engine.groups;auto source_game=engine.game;auto source_random=engine.rand;
            bmh::MatchEngine source(oracle.state,source_groups,source_game,source_random,engine.referees);source.month=engine.month;source.buttons=engine.buttons;source.leave_screens=true;bmh::MinuteReport report;
            bmh::play_matchday(source,oracle.state,source_groups,source.buttons,source_game,source_random,report);check(!report.unsupported,"common Source multi-group clock failed");
            for(auto& group:source_groups)bmh::process_group(oracle.state,group,source_game,source_random,report);check(!report.unsupported,"common Source multi-group settlement failed");
            expected=initial;oracle.commit_match_state(expected->world);capture_career_economy(expected->world,oracle);oracle.commit_desktop_preferences(*expected);
            expected->game_seed=source_game.seed();expected->watcom_seed=source_random.seed();expected_g=source_game.calls();expected_w=source_random.calls();
        };
        hooks.before_owned=[&](auto&,auto& engine,const auto& owner){check(owner==initial&&engine.groups.data()==storage,"shared clock did not expose exact staged pre-minute owner");++owned_calls;};
        hooks.after_minute=[&](auto&,auto& engine,int minute){check(owned_calls==1&&minute>last_minute,"human leagues ran separate minute clocks");if(minute==1)++starts;last_minute=minute;check(engine.groups.data()==storage,"shared group vector moved during callback");for(size_t i=0;i<descriptors.size();++i)check(engine.groups[i].desc.data()==descriptors[i],"shared descriptor moved during callback");};
        const auto result=simulate_and_settle_human_league_rounds(played,base,inputs,{3,6,false,true},game,random,hooks);
        check(expected&&played==*expected&&result.game_draws==expected_g&&result.watcom_draws==expected_w&&game.calls()==expected_g&&random.calls()==expected_w,"shared human batch differs from common Source scores/player/finance/monthly/mailbox/preferences/GW");
        check(starts==1&&last_minute>=90&&result.leagues.size()==leagues.size(),"shared human batch never ran one complete clock");
        for(size_t i=0;i<leagues.size();++i){check(result.leagues[i].game_draws==0&&result.leagues[i].watcom_draws==0,"shared RNG incorrectly attributed to individual league");for(size_t m=0;m<result.leagues[i].matches.size();++m){const auto& row=result.leagues[i].matches[m];check(row.score.home==(source_groups[i].results[m]>>4)&&row.score.away==(source_groups[i].results[m]&15)&&row.end_minute==source_groups[i].end_minute[m],"shared score/end-minute differs from Source oracle");}}
        check(read_save(make_save(played))==played,"shared match state lost on save/reopen");
        auto failed=initial;bmh::GameRandom failed_g(17);bmh::WatcomRandom failed_w(91);HumanRoundHooks abort;abort.decline_unpresented_events=true;abort.after_minute=[](auto&,auto&,int minute){if(minute==10)throw std::runtime_error("cancel live clock");};
        rejects([&]{simulate_and_settle_human_league_rounds(failed,base,inputs,{3,6,false,true},failed_g,failed_w,abort);},"cancelled shared clock published");check(failed==initial&&failed_g.calls()==0&&failed_w.calls()==0,"cancelled shared clock leaked candidate or RNG");
        auto stale=inputs;++stale.back().round;rejects([&]{simulate_and_settle_human_league_rounds(failed,base,stale,{3,6,false,true},failed_g,failed_w,abort);},"stale shared round accepted");
        auto repeated=inputs;repeated.back()=repeated.front();rejects([&]{simulate_and_settle_human_league_rounds(failed,base,repeated,{3,6,false,true},failed_g,failed_w,abort);},"duplicate shared league accepted");
        HumanRoundHooks external;external.decline_unpresented_events=true;external.after_minute=[&](auto&,auto&,int minute){if(minute==10)++failed.world.economy->managers[3].deposit;};
        rejects([&]{simulate_and_settle_human_league_rounds(failed,base,inputs,{3,6,false,true},failed_g,failed_w,external);},"shared clock overwrote external economy mutation");auto retained=initial;++retained.world.economy->managers[3].deposit;check(failed==retained&&failed_g.calls()==0&&failed_w.calls()==0,"stale shared batch lost external owner state or leaked RNG");
        std::cout<<leagues.size()<<" human leagues / "<<source_groups.size()<<" common Source groups: scores, full World, finance/monthly, preferences, exact GW and rollback passed\n";
    }
}

void statistics_bridge(const bmh::GameState& base) {
    auto source=snapshot(base);
    const auto de=source.world.managers[0].club,en=source.world.managers[1].club;
    const auto player=source.world.clubs.at(de).roster[0].player;
    const auto other=source.world.clubs.at(en).roster[0].player;
    check(de.value>4095&&player.value>4095,"global identity fixture must exceed legacy packed range");
    source.world.player_statistics[{de,player}]={{255,4,7},65535,9,2};
    source.world.player_statistics[{en,other}]={{700,8,9},200000,20,3};
    auto& row=source.world.managers[0].tactics->backing->scalar_rows[0];row[7]=255;row[0x48]=255;row[0x49]=255;
    auto reference=base;reference.player_statistics[0]={{9,9,9},999,1,1};reference.title_history[0][0]="Unrelated reference history";
    auto projection=AiMatchProjection::create_for_humans(source,{en,de},reference,{3,6,false,true});
    const auto local_club=projection.local_club(de),local_player=projection.local_player(player);
    const auto local_key=local_club<<12|local_player;
    check(projection.state.player_statistics.size()==2&&projection.state.player_statistics.at(local_key).total==65535&&
          projection.state.title_history.empty(),"typed projection leaked legacy reference counters/title history");
    bmh::player_statistic(projection.state,0,int(local_player),0,0,0,0);
    auto committed=source.world;projection.commit_match_state(committed);
    check(committed.player_statistics.at({de,player}).appearances[0]==256&&committed.player_statistics.at({de,player}).total==65536&&
          committed.player_statistics.at({en,other}).total==200000,"human match update lost or mixed global club/player counts");
    auto saved=source;saved.world=committed;
    check(read_save(make_save(saved))==saved,"wide statistics lost in typed CareerSnapshot save/load");
    auto desktop=AiMatchProjection::create_for_desktop(saved,{en,de},reference,{3,6,false,true});
    const auto sorted_key=desktop.local_club(de)<<12|desktop.local_player(player);
    check(desktop.state.player_statistics.at(sorted_key).total==65536,"desktop player sorting lost canonical statistics mapping");
    auto image=bmh::LeImage::from_bytes(bmh::layout::GermanDataSegmentBegin,bmh::Bytes(0x20000));
    const auto put=[&](uint32_t at,uint16_t value){const uint8_t bytes[]={uint8_t(value),uint8_t(value>>8)};image.patch(at,bytes,2);};
    put(bmh::layout::TeamsWithSquads,32);put(bmh::layout::LeagueClubCount,445);
    desktop.state.memory.add_block(bmh::layout::CareerSession_AnyOf_ExecutableField,8);
    desktop.state.memory.add_block(bmh::layout::TextGradientDisabled,1);desktop.state.memory.set8(bmh::layout::TextGradientDisabled,1);
    desktop.state.memory.add_block(bmh::layout::PlainBackground,1);
    const auto lineup=bmh::ui::lineup_from_game(desktop.state,image,1);
    const auto found=std::find_if(lineup.players.begin(),lineup.players.end(),[&](const auto& p){return p.player_id==int(desktop.local_player(player));});
    check(found!=lineup.players.end()&&found->statistics&&found->statistics->total==65536,
          "shared lineup projection did not receive the canonical wide counter after desktop sorting");
    bool unavailable=false;
    try { (void)bmh::ui::statistics_rankings_from_game(desktop.state,image,0,0,0,bmh::Image(320,240)); }
    catch(const bmh::FormatError& error) { unavailable=std::string(error.what()).find("typed title history")!=std::string::npos; }
    check(unavailable,"typed title history did not report its explicit unsupported boundary");
    auto stale=source.world;auto pending=AiMatchProjection::create_for_humans(source,{en,de},reference,{3,6,false,true});
    ++stale.player_statistics.at({en,other}).total;const auto external=stale;
    rejects([&]{pending.commit_match_state(stale);},"concurrent global statistics change was overwritten");
    check(stale==external,"stale statistics rejection was not atomic");
    auto lost=AiMatchProjection::create_for_humans(source,{en,de},reference,{3,6,false,true});lost.state.player_statistics.clear();
    auto intact=source.world;rejects([&]{lost.commit_match_state(intact);},"missing projection counters were silently dropped");check(intact==source.world,"counter-loss rejection published partial state");
    auto decreasing=AiMatchProjection::create_for_humans(source,{en,de},reference,{3,6,false,true});decreasing.state.player_statistics.at(local_key).total=0;
    rejects([&]{decreasing.commit_match_state(intact);},"unowned counter reset was accepted");
    auto forged=AiMatchProjection::create_for_humans(source,{en,de},reference,{3,6,false,true});forged.state.player_statistics[4094u<<12]={{},1,0,0};
    rejects([&]{forged.commit_match_state(intact);},"global ID was admitted as a local packed club key");check(intact==source.world,"invalid local identity partially committed");
    auto exact=source.world;exact.player_statistics.at({en,other}).total=UINT64_MAX;
    auto unknown=exact;unknown.player_statistics[{{4294967294u},player}]={{},1,0,0};
    rejects([&]{encode_world(unknown);},"statistics archive accepted a nonexistent global club");
    const auto bytes=encode_world(exact);check(decode_world(bytes)==exact,"codec rounded unsigned 64-bit statistics");
    const auto raw=std::string(bytes.begin(),bytes.end());
    check(raw.find("18446744073709551615")!=std::string::npos,"codec did not store exact decimal uint64");
    auto json=bmh::cli::parse_json(raw);auto& records=std::get<bmh::cli::Json::Array>(json.object()->at("player_statistics").value);
    auto& values=std::get<bmh::cli::Json::Array>(records[0].object()->at("counters").value);values[0]="18446744073709551616";
    const auto bad=bmh::cli::dump_json(json);rejects([&]{decode_world(bmh::Bytes(bad.begin(),bad.end()));},"overflowing decimal statistics accepted");
    json=bmh::cli::parse_json(raw);json.object()->at("version")=22;const auto downgraded=bmh::cli::dump_json(json);
    rejects([&]{decode_world(bmh::Bytes(downgraded.begin(),downgraded.end()));},"schema22 silently discarded nonempty statistics");
    auto old=source.world;old.player_statistics.clear();bmh::test::world12::omit_recent_manager_records(old);const auto old_encoded=encode_world(old);json=bmh::cli::parse_json(std::string(old_encoded.begin(),old_encoded.end()));
    json.object()->at("version")=22;json.object()->erase("player_statistics");const auto legacy=bmh::cli::dump_json(json);
    check(decode_world(bmh::Bytes(legacy.begin(),legacy.end()))==old,"schema22 without known wide counters failed migration");
    auto migrated=source;migrated.world=decode_world(bmh::Bytes(legacy.begin(),legacy.end()));
    auto first_observation=AiMatchProjection::create_for_humans(migrated,{en,de},reference,{3,6,false,true});
    bmh::player_statistic(first_observation.state,0,int(first_observation.local_player(player)),0,0,0,0);
    first_observation.commit_match_state(migrated.world);
    check(migrated.world.player_statistics.at({de,player}).appearances[0]==256&&
          migrated.world.player_statistics.at({de,player}).total==65536,
          "first schema22 observation lost its surviving tactical byte/word baseline");
    std::cout<<"Global >4095 club/player statistics, Source 256/65536, desktop lineup mapping, exact schema23 snapshot, old-schema and atomic guards passed\n";
}
}
int main()try {
    bmh::set_fixture_tables(bmh::test::authored_fixture_tables());const auto base=rules();shared_human_clock(base);statistics_bridge(base);short_squad_source_kickoff(base);annual_donor_workspace(base);live_preferences_and_scopes(base);known_economy_match_oracle(base);const auto source=snapshot(base);auto world=source.world;
    const auto de=world.membership[4].clubs[0],en=world.membership[9].clubs[0],de_other=world.membership[4].clubs[1],en_other=world.membership[9].clubs[1];
    const std::vector<ClubId> requested{de,de_other,en,en_other};
    rejects([&]{AiMatchProjection::create(world,requested,base);},"AI-only guard allowed a human participant");
    for(uint16_t day:{109,110,230,231}) {
        auto later=source;later.season=1;later.date=365+day;later.calendar.emplace();later.calendar->next_day=day;
        for(const auto& league:later.world.catalogue.leagues)for(int round=1;round<=league.rounds();++round)
            later.calendar->schedule.leagues.push_back({uint16_t((league.id.value==4||league.id.value==9)&&round<=2?round*10:240+round*3),league.id,uint8_t(round)});
        later.validate();const auto projected=AiMatchProjection::create_for_humans(later,requested,base,{3,6,false,true});
        check(projected.state.season==1&&projected.state.season_start==365&&projected.state.season_day()==day,"later-season Source date borrowed the reference epoch");
    }
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
        auto day=source;
        // The internal ten-club English league starts later than this fixture date.
        // Put the second human in the actually scheduled Premier group.
        day.world.managers[1]={3,"English",day.world.membership[5].clubs[0],{5},3};
        bmh::WatcomRandom confirmation(91);initialize_manager_contracts(day.world,3,base,{3,false,false,false,6},confirmation);
        day.results=SeasonResults::empty(day.world.catalogue);day.league_rounds.fill(0);
        std::vector<uint16_t> de(365),en(365);for(int r=0;r<34;++r)de[size_t(20+r*8)]=15;for(int r=0;r<38;++r)en[size_t(20+r*8)]=15;
        day.calendar=CalendarCursor{SeasonCalendar::from_sources(day.world.catalogue,de,en),20,0};day.game_seed=17;day.watcom_seed=91;
        const auto ticket=*prepare_day(day);std::vector<app::AiDayLeagueConditions> inputs;
        AiLeagueConditions c;c.month=8;c.temperature=19;c.date=day.date;c.spectators=15000;c.referee_properties={24};c.presentation_buttons.assign(0xb4,1);
        for(const auto& league:ticket.leagues)inputs.push_back({league.appointment,std::vector<AiLeagueConditions>(league.matches.size(),c)});
        const auto before=day;app::AiDayOwner owner{71,0};HumanRoundHooks hooks;hooks.live_event=[](auto&,auto&,const auto&){};
        rejects([&]{app::play_ai_league_day(day,owner,{71,0},ticket,base,inputs);},"AI day absorbed a human game");check(day==before&&owner.revision==0,"AI-only human rejection mutated day");
        int clocks=0;hooks.before_owned=[&](auto&,auto& engine,const auto& staged){++clocks;check(engine.groups.size()==2&&staged.world==before.world&&staged.results==before.results,"AI-only future results/stats leaked into human minute one");};
        const auto played=app::play_league_day(day,owner,{71,0},ticket,base,inputs,{3,6,false,true},hooks);
        check(clocks==1,"mixed day opened multiple human league clocks");
        auto combined=before;app::ManagerActionOwner combined_owner{73,0};const auto source_rules=bmh::test::world12::country_rules(false);
        const auto combined_result=app::play_competition_day(combined,combined_owner,{73,0},ticket,base,{source_rules,source_rules},inputs,{}, {},{3,6,false,true},hooks);
        check(combined==day&&combined_result.game_draws==played.game_draws&&combined_result.watcom_draws==played.watcom_draws&&combined_owner.revision==1&&clocks==2,"combined day did not share the league owner clock/results/GW/order");
        check(day.date==before.date+1&&day.calendar->next_day==21&&owner.revision==1&&day.league_rounds[4]==1&&played.leagues.size()==ticket.leagues.size(),"mixed human/AI day did not publish one complete transition");
        check(read_save(make_save(day))==day,"committed mixed day lost calendar/results/manager data");const auto after=day;
        rejects([&]{app::play_league_day(day,owner,{71,0},ticket,base,inputs,{3,6,false,true},hooks);},"mixed day replay accepted");check(day==after&&owner.revision==1,"mixed replay changed completed day");
        auto failed=before;app::AiDayOwner failed_owner{72,0};inputs.back().matches.back().month=0;
        rejects([&]{app::play_league_day(failed,failed_owner,{72,0},ticket,base,inputs,{3,6,false,true},hooks);},"bad final group committed earlier human match");check(failed==before&&failed_owner.revision==0&&clocks==2,"invalid late group opened a presenter or leaked mixed day records/RNG/date");
        app::ManagerActionOwner combined_failed_owner{74,0};
        rejects([&]{app::play_competition_day(failed,combined_failed_owner,{74,0},ticket,base,{source_rules,source_rules},inputs,{}, {},{3,6,false,true},hooks);},"combined owner accepted invalid late conditions");
        check(failed==before&&combined_failed_owner.revision==0&&clocks==2,"combined preflight opened a presenter or leaked candidate state");
        std::cout<<"Complete mixed human/AI league day, replay/snapshot and late-group atomic rollback passed\n";
    }
    std::cout<<total_matches<<" actual human/AI league fixtures, goals="<<total_goals<<", presented human events="<<events<<"; repeat/snapshot/calendar-owner checks passed\n";
    std::cout<<"Mixed human Source ratings, sparse seats, country-specific forecasts, accounts/stats/history/global references and late rollback passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
