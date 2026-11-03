// SPDX-License-Identifier: GPL-3.0-only
#include "regional_playoff_fixture.hpp"
#include "fixture_tables.hpp"
#include "game/world12/codec.hpp"
#include "game/world12/regional_playoff_match.hpp"
#include "game/world12/economy.hpp"
#include "game/world12/manager_finance.hpp"
#include "game/world12/manager_tactics.hpp"
#include "game/lineup.hpp"
#include "game/messages.hpp"
#include "game/matchday.hpp"
#include "game/cup.hpp"
#include "game/driver.hpp"
#include "game/schedule.hpp"
#include <iostream>
namespace {
using namespace bmh;using namespace bmh::world12;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class F>void rejects(F action,const char* message){bool rejected=false;try{action();}catch(const std::exception&){rejected=true;}check(rejected,message);}
std::array<ClubId,4> source_activation(const CareerSnapshot& after){
    GameState native;native.league_rounds.assign(9,0);native.league_rounds[2]=native.league_rounds[3]=33;native.results.assign(9*380,0xcc);native.position_history.assign(138*26,0);
    for(auto league:{LeagueId{2},LeagueId{3}})for(int round=1;round<=34;++round)for(int match=0;match<9;++match){const auto& score=after.results.result(after.world.catalogue,league,round,match);native.results[league.value*380+(round-1)*10+match]=uint8_t((score.home<<4)|score.away);}
    native.memory.add_block(layout::Teams,8);native.memory.add_block(layout::MatchdayGroupWorkspace,32);native.memory.add_block(layout::ActiveSelectors,8);for(uint32_t i=0;i<8;++i)native.memory.set8(layout::ActiveSelectors+i,0x14);
    WatcomRandom random(91);MinuteReport report;advance_league_rounds(native,4,random,report);check(!report.unsupported&&random.calls()==0,"Original regional activation consumed RNG or failed");
    check(native.memory.u8(layout::ActiveSelectors)==13,"Original did not activate competition13");for(uint32_t i=0;i<32;++i)check(native.memory.u8(layout::MatchdayGroupWorkspace+i)==0xcc,"Original activation did not clear all scratch scores");
    std::array<ClubId,4> result;for(uint32_t i=0;i<4;++i){const auto team=native.memory.u16(layout::Teams+i*2);check(team>=36&&team<72,"Original selected outside its regional physical slots");result[i]=after.world.membership[team<54?2:3].clubs[team<54?team-36:team-54];}return result;
}
void replay(const LeImage& rules){
    auto before=bmh::test::world12::before_regional_final(rules);const auto original=before;check(!activate_regional_playoff(before,before)&&before==original,"early activation changed the campaign");
    auto staged=before;bmh::test::world12::authored_final_day_results(staged);const auto source=source_activation(staged);check(activate_regional_playoff(staged,before)&&staged.world.regional_playoff->entrants==source,"activation differs from actual Original inline boundary/physical order");
    ++staged.calendar->next_day;++staged.calendar->revision;++staged.date;staged.validate();check(read_save(make_save(staged))==staged,"activated campaign did not survive save/reopen");
    const auto active=staged;rejects([&]{activate_regional_playoff(staged,before);},"replayed activation accepted");check(staged==active,"replayed activation changed actual owner");
    auto missing=before;missing.world.regional_playoff.reset();auto missing_after=missing;bmh::test::world12::authored_final_day_results(missing_after);rejects([&]{activate_regional_playoff(missing_after,missing);},"unknown older campaign was fabricated at closing");
    rejects([&]{annual_regional_qualifiers(staged);},"unfinished playoff supplied annual winners");
    GameState native;native.memory.add_block(kCupTable,kCupEntries*kCupEntrySize);for(int i=0;i<kCupEntries;++i)native.memory.set16(cup_entry_address(i)+0x18,65535);
    const auto entry=cup_entry_address(35);native.memory.add_block(layout::Teams,8);native.memory.add_block(layout::MatchdayGroupWorkspace,32);native.memory.set_pointer(entry,layout::Teams);native.memory.set_pointer(entry+8,layout::CareerSession_RuntimeError_RuntimeField);native.memory.set_pointer(entry+12,layout::MatchdayGroupWorkspace);
    for(uint32_t i=0;i<4;++i)native.memory.set16(layout::Teams+i*2,uint16_t(i));for(uint32_t i=0;i<16;++i)native.memory.set16(layout::MatchdayGroupWorkspace+i*2,0xcccc);
    for(uint32_t b=0x14;b<=0x20;++b)native.memory.set8(entry+b,rules.u8(entry+b));native.memory.set8(entry+0x1d,0);
    WatcomRandom random(91),source_random(91);
    for(uint8_t round=0;round<6;++round){
        const auto day=staged.world.regional_playoff->dates[round];
        // Bring intervening authored league appointments to their canonical cursor.
        for(const auto& a:staged.calendar->schedule.leagues)if(a.day<day&&a.round>staged.league_rounds[a.league.value]){staged.league_rounds[a.league.value]=a.round;for(int m=0;m<staged.world.catalogue.league(a.league).clubs/2;++m)staged.results.result(staged.world.catalogue,a.league,a.round,m)={true,0,0};}
        staged.date=day;staged.calendar->next_day=day;staged.validate();const auto group=*prepare_regional_playoff_group(staged);
        const auto fixtures=generate_fixtures(4,round+1,3,0);for(size_t m=0;m<2;++m)check(group.matches[m]==bmh::world12::Fixture{source[fixtures[m].home],source[fixtures[m].away]},"playoff fixture differs from Source double group");
        RegionalPlayoffResults results{group,{uint16_t(((round+1)%4)<<5|(round%3)),uint16_t((round%2)<<5|((round+2)%4))}};
        const auto snapshot=staged;auto stale=results;++stale.group.revision;rejects([&]{commit_regional_playoff_results(staged,stale,rules,random);},"stale result accepted");check(staged==snapshot&&random.calls()==0,"stale result leaked state/RNG");
        for(size_t m=0;m<2;++m)native.memory.set16(layout::MatchdayGroupWorkspace+(round*2+uint32_t(m))*2,results.scores[m]);MinuteReport report;advance_cup(native,13,source_random,report);check(!report.unsupported,"Original playoff advance failed");
        commit_regional_playoff_results(staged,results,rules,random);check(staged.world.regional_playoff->played==round+1&&random.seed()==source_random.seed()&&random.calls()==source_random.calls(),"playoff advance differs from Source cursor/RNG");
        rejects([&]{commit_regional_playoff_results(staged,results,rules,random);},"replayed result accepted");++staged.calendar->next_day;++staged.calendar->revision;++staged.date;staged.validate();staged=read_save(make_save(staged));
    }
    const auto qualifiers=annual_regional_qualifiers(staged);check(qualifiers==std::array<ClubId,2>{source[native.memory.u16(entry+0x24)],source[native.memory.u16(entry+0x22)]}&&random.calls()==0,"annual qualifier order differs from Source+24,+22 or consumed RNG");
    auto tampered=staged;std::swap(tampered.world.regional_playoff->qualifiers[0],tampered.world.regional_playoff->qualifiers[1]);rejects([&]{tampered.validate();},"tampered Source qualifier order accepted");
    const auto encoded=encode_world(staged.world);auto json=cli::parse_json(std::string(encoded.begin(),encoded.end()));
    json.object()->at("version")=39;auto raw=cli::dump_json(json);rejects([&]{decode_world(Bytes(raw.begin(),raw.end()));},"older schema accepted known playoff owner");
    json.object()->at("version")=40;json.object()->erase("regional_playoff");raw=cli::dump_json(json);check(!decode_world(Bytes(raw.begin(),raw.end())).regional_playoff,"older proto40 invented a missing playoff owner");
    std::cout<<"Source inline activation, physical ranks1/2, six dates/12scores, group fixtures/ranking/qualifier word order, exact zero RNG, replay and save/reopen passed\n";
}
GameState match_reference(){
    GameState state;state.club_ids.resize(445);state.team_flags.resize(445);state.team_morale.resize(445);state.formations.resize(0x580);state.manager_records.resize(4*0xb0c);state.manager_ratings.resize(4*60);state.tactics.resize(4*0x840);state.manager_options.resize(20);
    const uint8_t depth[]={0,5,5,5,5,10,10,10,10,17,17},lane[]={4,1,3,5,7,1,3,5,7,3,5};
    for(size_t manager=0;manager<4;++manager){state.manager_options[manager*5]=1;for(size_t mode=0;mode<2;++mode)for(size_t formation=0;formation<8;++formation)for(size_t row=0;row<11;++row){const auto at=manager*0x160+mode*0xb0+formation*22+row;state.formations[at]=depth[row];state.formations[at+11]=lane[row];}}
    return state;
}
void real_match_rounds(const LeImage& rules){
    const auto reference=match_reference();
    for(uint8_t humans=1;humans<=4;++humans){
        auto before=bmh::test::world12::before_regional_final(rules);auto state=before;bmh::test::world12::authored_final_day_results(state);activate_regional_playoff(state,before);++state.date;++state.calendar->next_day;
        auto& world=state.world;world.markets.emplace();world.free_players.emplace();world.settings=CareerSettings{3,6,5,humans,false};world.economy.emplace();world.economy->presentation_options.emplace();world.economy->presentation_options->fill(1);
        for(size_t seat=0;seat<4;++seat){auto& economy=world.economy->managers[seat];economy.monthly.emplace();economy.monthly->history_count=1;economy.monthly->counters[0]=uint32_t(1000+seat);economy.deposit=uint32_t(7000+seat);economy.depreciation=80;}
        const auto field=world.regional_playoff->entrants;uint32_t player_id=3000000;
        for(auto club:field)for(size_t row=0;row<20;++row){auto& slot=world.clubs.at(club).roster[row];if(slot.empty()){PlayerRecord player;player.id={player_id++};player.attributes.name="Reserve";world.players[player.id]=player;slot={player.id,uint16_t(row<17?2:0)};}auto& attributes=world.players.at(slot.player).attributes;attributes.w35c=60u|(75u<<7)|(85u<<14);attributes.w364=uint32_t(row==0?0:row<5?1:row<9?2:3);}
        WatcomRandom confirmation(91);for(uint8_t n=0;n<humans;++n){const uint8_t seat=n==humans-1?3:n;world.managers.push_back({seat,"Human",field[n],world.clubs.at(field[n]).league,seat});initialize_manager_contracts(world,seat,reference,{3,false,false,false,6},confirmation);world.managers.back().career.emplace();world.managers.back().career->season_records.emplace();}
        state.game_seed=17;state.watcom_seed=91;state.validate();
        for(uint8_t round=0;round<6;++round){
            const auto day=state.world.regional_playoff->dates[round];for(const auto& a:state.calendar->schedule.leagues)if(a.day<day&&a.round>state.league_rounds[a.league.value]){state.league_rounds[a.league.value]=a.round;for(int m=0;m<state.world.catalogue.league(a.league).clubs/2;++m)state.results.result(state.world.catalogue,a.league,a.round,m)={true,0,0};}state.date=day;state.calendar->next_day=day;state.validate();
            const auto initial=state;const auto group=*prepare_regional_playoff_group(state);AiLeagueConditions c;c.date=day;c.month=5;c.temperature=18;c.spectators=15000;c.referee_properties={24};c.presentation_buttons.assign(180,1);std::vector<AiLeagueConditions> conditions(2,c);GameRandom game(state.game_seed);WatcomRandom random(state.watcom_seed);
            std::optional<CareerSnapshot> expected;std::vector<MatchGroup> native_groups;uint64_t game_draws=0,watcom_draws=0;HumanRoundHooks hooks;hooks.decline_unpresented_events=true;int opened=0,last=0;
            hooks.before_matchday=[&](auto& projection,auto& engine){check(engine.groups.size()==1&&engine.groups[0].desc[0]==14&&engine.groups[0].desc[1]==195&&projection.human_managers()==humans,"regional group lost Source14/options/cards or humans");auto oracle=projection;native_groups=engine.groups;auto g=engine.game;auto w=engine.rand;MatchEngine native(oracle.state,native_groups,g,w,engine.referees);native.month=engine.month;native.buttons=engine.buttons;native.leave_screens=true;MinuteReport report;play_matchday(native,oracle.state,native_groups,native.buttons,g,w,report);check(!report.unsupported,"Source regional clock failed");process_group(oracle.state,native_groups[0],g,w,report);check(!report.unsupported,"Source regional settlement failed");expected=initial;oracle.commit_match_state(expected->world);capture_career_economy(expected->world,oracle);oracle.commit_desktop_preferences(*expected);expected->game_seed=g.seed();expected->watcom_seed=w.seed();game_draws=g.calls();watcom_draws=w.calls();};
            hooks.before_owned=[&](auto&,auto&,const auto& owned){check(owned==initial,"regional live hook lost staged owner");++opened;};hooks.after_minute=[&](auto&,auto&,int minute){check(opened==1&&minute>last,"regional live matches used separate clocks");last=minute;};
            const auto receipt=simulate_and_settle_regional_playoff_group(state,reference,group,rules,conditions,{3,6,false,true},game,random,hooks);
            check(expected&&state==*expected&&receipt.scores==native_groups[0].cup_results&&receipt.game_draws==game_draws&&receipt.watcom_draws==watcom_draws&&last>=90,"regional match differs from complete direct Source state/finance/monthly/GW");
            const auto scored=state;HumanRoundHooks abort;abort.decline_unpresented_events=true;abort.after_minute=[](auto&,auto&,int minute){if(minute==10)throw std::runtime_error("cancel regional live");};auto cancelled=initial;GameRandom cancel_g(initial.game_seed);WatcomRandom cancel_w(initial.watcom_seed);rejects([&]{simulate_and_settle_regional_playoff_group(cancelled,reference,group,rules,conditions,{3,6,false,true},cancel_g,cancel_w,abort);},"cancelled regional match committed");check(cancelled==initial&&cancel_g.calls()==0&&cancel_w.calls()==0,"cancelled regional match leaked state/RNG");
            commit_regional_playoff_results(state,{group,receipt.scores},rules,random);++state.calendar->next_day;++state.calendar->revision;++state.date;state.watcom_seed=random.seed();state.validate();state=read_save(make_save(state));(void)scored;
        }
        check(annual_regional_qualifiers(state)[0].valid(),"completed actual match programme lacks annual qualifiers");std::cout<<int(humans)<<" humans:12real Core playoff matches, complete Source state/score/finance/monthly/GW oracle, cancel/save/reopen passed\n";
    }
}

}
int main()try{set_fixture_tables(bmh::test::authored_fixture_tables());const auto source=bmh::test::world12::regional_rules();replay(source);real_match_rounds(source);return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
