// SPDX-License-Identifier: GPL-3.0-only
#include "world12_fixture.hpp"
#include "game/world12/progression.hpp"
#include "game/matchday_setup.hpp"
#include "game/schedule.hpp"
#include <algorithm>
#include <iostream>
namespace {
using namespace bmh;using namespace bmh::world12;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class F>void rejects(F f,const char* message){bool bad=false;try{f();}catch(const std::exception&){bad=true;}check(bad,message);}
CareerSnapshot fixture() {
    CareerSnapshot s;s.world=bmh::test::world12::complete_world();auto& w=s.world;w.players_initialized=true;w.markets.emplace();w.free_players.emplace();w.settings=CareerSettings{3,3,5,1,false};w.economy.emplace();w.economy->presentation_options.emplace();
    std::vector<uint16_t> de(365),en(365);for(size_t r=0;r<34;++r)de[7+r*8]=15;for(size_t r=0;r<38;++r)en[7+r*8]=15;
    de[1]=en[1]=0x4002;de[2]=0x4002;en[3]=0x4002;
    s.calendar=CalendarCursor{SeasonCalendar::from_sources(w.catalogue,de,en),1,2};s.results=SeasonResults::empty(w.catalogue);s.date=1;s.game_seed=17;s.watcom_seed=29;s.validate();return s;
}
}
int main()try {
    auto s=fixture();const auto schedule=s.calendar->schedule;
    GameState original;original.memory.add_block(layout::CalendarState_FormatError_RuntimeField,1);original.memory.add_block(layout::CalendarState_FormatError_RuntimeField2,1);
    for(uint8_t raw:{0,1,2,128,129,255}) {
        (*s.world.economy->presentation_options)[16*9+4]=raw;
        original.memory.set8(layout::CalendarState_FormatError_RuntimeField,raw);
        kEnglishCompetitions=false;const bool indoor=day_competitions(original,0x4002)!=0;
        kEnglishCompetitions=true;const bool league_cup=day_competitions(original,0x4002)!=0;kEnglishCompetitions=false;
        const auto admitted=admitted_other_appointments(s,1);
        check(std::count_if(admitted.begin(),admitted.end(),[](const auto& a){return a.domain==CalendarDomain::germany;})==int(indoor),"German admission differs from Original calendar kernel");
        check(std::count_if(admitted.begin(),admitted.end(),[](const auto& a){return a.domain==CalendarDomain::england;})==int(league_cup),"German indoor switch removed English League Cup");
        check(s.calendar->schedule==schedule,"runtime policy rewrote immutable source appointments");
    }
    auto& raw=(*s.world.economy->presentation_options)[16*9+4];raw=0;
    rejects([&]{prepare_day(s);},"unknown older indoor campaign was invented");
    raw=1;const auto old=*prepare_day(s);raw=0;const auto before=s;
    rejects([&]{commit_day(s,old,{},{});},"ticket from another indoor policy was committed");check(s==before,"stale policy ticket changed state");
    raw=1;const auto english_before=s;const auto english=*prepare_day(s);check(english.others.size()==1&&english.others[0].domain==CalendarDomain::england,"English appointment was discarded");
    rejects([&]{commit_day(s,english,{},{});},"unplayed English cup was skipped with disabled German indoors");check(s==english_before,"unplayed English cup changed date/RNG");
    s.date=2;s.calendar->next_day=2;const auto quiet=*prepare_day(s);check(quiet.others.empty(),"disabled German-only date was not quiet");
    const auto seed=s.game_seed;const auto watcom=s.watcom_seed;commit_day(s,quiet,{},{});check(s.date==3&&s.game_seed==seed&&s.watcom_seed==watcom&&s.calendar->schedule==schedule,"quiet indoor date changed RNG or source schedule");
    check(read_save(make_save(s))==s,"indoor policy/date did not survive FBM");
    s.world.economy->presentation_options.reset();check(prepare_day(s)->others.size()==1,"unknown German policy blocked an English-only cup date");
    s.date=1;s.calendar->next_day=1;rejects([&]{prepare_day(s);},"missing saved indoor policy borrowed current defaults");
    std::cout<<"Original indoor admission oracle, English League Cup separation, immutable calendar, stale tickets, exact quiet-day commit and FBM passed\n";
}catch(const std::exception& e){kEnglishCompetitions=false;std::cerr<<e.what()<<'\n';return 1;}
