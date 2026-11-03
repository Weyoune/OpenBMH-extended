// SPDX-License-Identifier: GPL-3.0-only
#include "annual_indoor_fixture.hpp"
#include "fixture_tables.hpp"
#include "game/world12/annual_calendar.hpp"
#include "game/world12/codec.hpp"
#include "game/calendar.hpp"
#include "game/profile_runtime.hpp"
#include "game/week.hpp"
#include <iostream>
namespace {
using namespace bmh;using namespace bmh::world12;
void check(bool yes,const char* text){if(!yes)throw std::runtime_error(text);}
template<class F>void rejects(F f,const char* text){bool refused=false;try{f();}catch(const std::exception&){refused=true;}check(refused,text);}
LeImage calendar_rules(bool english) {
    auto image=LeImage::from_bytes(layout::CalendarDateSetup,Bytes(layout::CompetitionCalendar+366*2-layout::CalendarDateSetup));
    const uint8_t setup[]={0xc6,0x44,0x24,0x04,2,0xc6,0x44,0x24,0x0c,0x07,0xc7,0x04,0x24,0xea,0x07,0,0};image.patch(layout::CalendarDateSetup,setup,sizeof setup);
    const auto word=[&](uint16_t day,uint16_t value){const uint8_t bytes[]={uint8_t(value),uint8_t(value>>8)};image.patch(layout::CompetitionCalendar+day*2,bytes,2);};
    word(0,0x4001);for(uint16_t round=0;round<(english?38:34);++round)word(uint16_t(7+round*8),15);
    word(320,0x20);if(!english)for(uint16_t i=0;i<6;++i)word(uint16_t(321+i*4),0x4004);return image;
}
}
int main()try {
    install_runtime_profile(DataProfile::international_11());auto tables=bmh::test::authored_fixture_tables();tables[4]={"abcd","acdb","adbc"};set_fixture_tables(tables);
    const auto de=calendar_rules(false),en=calendar_rules(true);const auto calendar=make_annual_calendar(Catalogue::dual_classic(),de,en,0);
    check(calendar.leagues.size()>0&&calendar.others_on(0).empty(),"new annual calendar retained runtime opening/replay appointment");
    for(uint32_t league=0;league<kLeagueSlots;++league)check(std::count_if(calendar.leagues.begin(),calendar.leagues.end(),[&](const auto& a){return a.league==LeagueId{league};})==Catalogue::dual_classic().leagues[league].rounds(),"new annual calendar lost actual country league rounds");
    rejects([&]{make_annual_calendar(Catalogue::dual_classic(),de,en,1);},"new calendar accepted a non-Wednesday epoch");
    const auto rules=bmh::test::world12::annual_indoor_rules();const auto reference=bmh::test::world12::annual_indoor_reference(rules);auto old=bmh::test::world12::annual_indoor_fixture(reference,rules,false);
    old.league_rounds[0]=old.league_rounds[1]=17;for(auto league:{LeagueId{0},LeagueId{1}})for(int r=1;r<=17;++r)for(int m=0;m<9;++m)old.results.result(old.world.catalogue,league,r,m)={true,2,0};old.date=137;
    check(prepare_indoor_draw(old,reference,rules).changed,"authored real Source Hall draw did not activate");
    while(!old.world.indoor->masters_winner.valid()){const auto groups=prepare_indoor_groups(*old.world.indoor);IndoorResults scores;scores.revision=groups[0].revision;scores.tournament=groups[0].tournament;for(const auto& group:groups)scores.groups.push_back({group.source_entry,group.played,std::vector<uint16_t>(group.matches.size(),uint16_t(2<<5))});commit_indoor_results(old,scores,reference,rules);}
    const auto actual=*old.world.indoor;auto next=old.world;next.indoor.reset();for(auto& members:next.membership)if(members.clubs.size()>1)std::rotate(members.clubs.begin(),members.clubs.begin()+1,members.clubs.end());
    const auto reset=make_annual_indoor_season(next,old,rules);check(reset.previous_holders[2]==actual.masters_winner&&reset.tournaments.empty()&&reset.selection_slots.empty()&&!reset.masters_winner.valid(),"new Hall owner reused a template winner or old played campaign");
    for(size_t i=0;i<4;++i){const auto source_slot=size_t(std::find(actual.selection_slots.begin(),actual.selection_slots.end(),actual.masters_scratch[i])-actual.selection_slots.begin());if(source_slot<164)check(reset.initial_scratch_slots[i]==source_slot,"new Hall scratch changed retained Source physical index");}
    next.indoor=reset;next.validate();check(decode_world(encode_world(next))==next,"new actual Hall carrier failed schema roundtrip");
    auto unknown=old;unknown.world.indoor.reset();rejects([&]{auto world=next;world.indoor.reset();make_annual_indoor_season(world,unknown,rules);},"new Hall owner invented missing old Masters/scratch provenance");
    std::cout<<"Annual source calendar and real prior Hall Masters/scratch carry reset passed (authored Source score programme, unpublished)\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
