// SPDX-License-Identifier: GPL-3.0-only
#include "world12_national_fixture.hpp"
#include "world12_national_programme_fixture.hpp"
#include "fixture_tables.hpp"
#include "game/world12/national_match.hpp"
#include "game/world12/national.hpp"
#include "game/world12/national_coach.hpp"
#include "game/world12/national_final_facts.hpp"
#include "game/world12/save.hpp"
#include "game/national_identity.hpp"
#include "game/cup.hpp"
#include "game/national_settlement.hpp"
#include <iostream>
#include <algorithm>
using namespace bmh;using namespace bmh::world12;
namespace {
void check(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
template<class F>void rejects(F f){bool caught=false;try{f();}catch(const std::exception&){caught=true;}check(caught,"invalid national match owner accepted");}
AiLeagueConditions condition(uint32_t date){AiLeagueConditions c;c.date=date;c.month=7;c.temperature=20;c.spectators=20000;c.referee_properties={24};c.presentation_buttons.assign(0xb4,1);return c;}
CareerSnapshot fixture(const GameState& base,bool coached){
    CareerSnapshot s;s.world=bmh::test::world12::national_world();auto& w=s.world;
    for(uint32_t c=0;c<62;++c){if(c==0||c==2)continue;ClubRecord club;club.id={3000000+c};club.country=Country(c);club.kind=ClubKind::national_team;club.name="Authored nation "+std::to_string(c);club.rating=uint8_t((c%8)*32+7);club.morale=25;club.crest={club.id.value};w.crests[club.crest]={"W/TEST.VGA",0};w.clubs[club.id]=club;}
    for(auto& manager:w.managers){manager.career.emplace();manager.career->annual_records.emplace();manager.career->annual_records->national_eligibility=coached?128:0;}
    w.economy.emplace();w.economy->week=3;w.economy->managers[0].deposit=112233;w.economy->managers[3].deposit=445566;w.economy->recipient_mask=9;EconomyMessage message;message.recipients=9;message.read_by=8;message.date=4;message.text[0]='R';w.economy->messages.push_back(message);
    s.results=SeasonResults::empty(w.catalogue);s.date=4;s.desktop.emplace();s.desktop->matchday_number=0;s.desktop->manager_order={3,0};for(const auto& manager:w.managers)s.desktop->managers.push_back({manager.seat});
    initialize_national_coaches(w,base);activate_national_coaches(w,s.desktop->manager_order);WatcomRandom nomination(91);for(auto country:{Country::germany,Country::england})nominate_national_squad(w,country,s.date,base,nomination);s.game_seed=17;s.watcom_seed=nomination.seed();s.validate();const auto actors=national_desktop_actors(s);check(actors.size()==(coached?2u:0u),"national desktop uses parked manager row count");if(coached)check(actors[0].seat==3&&actors[0].country==Country::england&&actors[1].seat==0,"national desktop sorted sparse canonical actors by seat");return s;
}
NationalCoachAccount native_prize(const NationalCoachRecord& record,const NationalFinalFacts& facts,int difficulty){
    GameState native;native.manager_records.resize(0xb0c);native.manager_teams={kNoTeam};native.level=difficulty;install_national_coach_account(native,0,record.account);
    const uint32_t fields[]={layout::Cup_NationalTournamentEnd_RuntimeField,layout::Cup_NationalTournamentEnd_RuntimeField2,layout::Cup_NationalTournamentEnd_RuntimeField3,layout::Cup_NationalTournamentEnd_RuntimeField4};
    for(size_t i=0;i<4;++i){native.memory.add_block(fields[i],2);native.memory.set16(fields[i],uint16_t(facts.places[i]==record.team?kNoTeam:kNoTeam+1+int(i)));}
    national_tournament_end(native);return national_coach_account_from_legacy(native,0);
}
void run(bool coached){
    const auto base=bmh::test::world12::national_reference();const auto source=bmh::test::world12::national_rules();auto state=fixture(base,coached);GameRandom game(state.game_seed);WatcomRandom random(state.watcom_seed);size_t matches=0,english=0,owned_calls=0;
    const size_t expected_matches[]={12,15,30,52},expected_dates[]={6,5,10,7};
    for(uint32_t season=0;season<8;++season){state.season=season;if(season)for(auto country:{Country::germany,Country::england})nominate_national_squad(state.world,country,state.date,base,random);prepare_national_campaign(state.world,source,season,game,random);state.game_seed=game.seed();state.watcom_seed=random.seed();const auto parked=state.world.managers;const auto deposits=state.world.economy->managers;const auto markets=state.world.markets;size_t played=0,dates=0;
        while(!state.world.national_campaigns->seasons.at(season).complete){const auto groups=prepare_national_groups(state.world.national_campaigns->seasons.at(season));std::vector<std::vector<AiLeagueConditions>> conditions;for(const auto& group:groups)conditions.push_back(std::vector<AiLeagueConditions>(group.matches.size(),condition(state.date)));
            HumanRoundHooks hooks;hooks.decline_unpresented_events=true;size_t ticks=0,observed_owned=0,observed_english=0;hooks.before_owned=[&](auto& projection,auto& engine,const auto& owned){++observed_owned;check(owned==state,"national before_owned did not receive the actual staged snapshot");check(engine.groups.size()==groups.size(),"national groups were split across clocks");for(size_t m=0;m<projection.human_managers();++m){const auto team=projection.global_club(uint32_t(projection.state.manager_teams[m]));check(is_national_team(projection.state,projection.state.manager_teams[m])&&state.world.clubs.at(team).kind==ClubKind::national_team,"live coach was rebound to a parked club");if(state.world.clubs.at(team).country==Country::england)++observed_english;}};
            hooks.after_minute=[&](auto& projection,auto& engine,int){++ticks;for(size_t i=0;i<groups.size();++i){check(engine.groups[i].desc[0]==groups[i].competition+1&&engine.groups[i].matches()==int(groups[i].matches.size()),"national Source descriptor/group count changed");for(size_t m=0;m<groups[i].matches.size();++m){const auto& actual=engine.groups[i];check(projection.global_club(uint32_t(actual.fixtures[m*2]))==groups[i].matches[m].home&&projection.global_club(uint32_t(actual.fixtures[m*2+1]))==groups[i].matches[m].away,"national live fixture lost global identities");}}};
            if(!matches){const auto before=state;const auto gs=game;const auto ws=random;auto invalid=conditions;invalid.back().pop_back();rejects([&]{simulate_and_settle_national_groups(state,base,groups,source,invalid,{3,6,false,true},game,random,hooks);});check(state==before&&game.seed()==gs.seed()&&game.calls()==gs.calls()&&random.seed()==ws.seed()&&random.calls()==ws.calls(),"incomplete national group leaked earlier fixtures/RNG");auto interrupted=hooks;interrupted.after_minute=[](auto&,auto&,int){throw std::runtime_error("cancelled actual clock");};rejects([&]{simulate_and_settle_national_groups(state,base,groups,source,conditions,{3,6,false,true},game,random,interrupted);});check(state==before&&game.seed()==gs.seed()&&random.seed()==ws.seed(),"cancelled national presenter published gameplay");ticks=0;observed_owned=0;observed_english=0;}
            const auto receipt=simulate_and_settle_national_groups(state,base,groups,source,conditions,{3,6,false,true},game,random,hooks);check(ticks>=90&&receipt.groups.size()==groups.size(),"actual national minute clock did not complete");owned_calls+=observed_owned;english+=observed_english;NationalResults scores{season,groups.front().revision,{}};
            for(size_t i=0;i<groups.size();++i){const auto& result=receipt.groups[i];check(result.scores.size()==groups[i].matches.size()&&std::all_of(result.end_minutes.begin(),result.end_minutes.end(),[](auto minute){return minute>=90&&minute<=124;}),"national actual match did not reach full time");scores.groups.push_back({groups[i].source_entry,groups[i].played,result.scores});played+=result.scores.size();matches+=result.scores.size();}
            const auto before_advance=*state.world.national_coaches;commit_national_results(state.world,scores,source,random);const auto& campaign=state.world.national_campaigns->seasons.at(season);
            if(campaign.complete&&campaign.competition==4){const auto facts=national_final_facts(campaign);for(size_t i=0;i<before_advance.size();++i){const auto expected=before_advance[i].active?native_prize(before_advance[i],facts,6):before_advance[i].account;check(state.world.national_coaches->at(i).account==expected,"actual national final allowance/prize differs from original single-coach Core");}}
            else check(*state.world.national_coaches==before_advance,"nonfinal national draw changed coach accounts");state.watcom_seed=random.seed();state.game_seed=game.seed();check(state.world.managers==parked&&state.world.economy->managers==deposits&&state.world.markets==markets,"national match wrote parked club finance/portfolio state");if(season==0&&dates==0)check(std::any_of(state.world.economy->messages.begin(),state.world.economy->messages.end(),[](const auto& m){return m.text[0]=='R'&&m.recipients==9&&m.read_by==8;}),"national mailbox lost sparse canonical recipients or prior notices");++state.date;++dates;const auto saved=state;state=read_save(make_save(state));check(state==saved,"actual national scores/nominees/accounts lost on regular save roundtrip");
        }
        check(played==expected_matches[season%4]&&dates==expected_dates[season%4],"actual national programme geometry changed");const auto facts=national_final_facts(state.world.national_campaigns->seasons.at(season));check(facts.winner.valid(),"actual Source final has no winner");
    }
    check(matches==218&&owned_calls>0,"national shared programmes lost actual matches/callbacks");std::cout<<"Actual Core national programmes: "<<matches<<" matches, coached="<<coached<<", English coached clocks="<<english<<"\n";
}
}
int main()try{auto fixtures=bmh::test::authored_fixture_tables();fixtures[4]={"abcd","acdb","adbc"};set_fixture_tables(fixtures);run(false);run(true);}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
