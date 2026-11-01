// SPDX-License-Identifier: GPL-3.0-only
#include "world12_fixture.hpp"
#include "fixture_tables.hpp"
#include "game/world12/manager_tactics.hpp"
#include "game/world12/manager_match_state.hpp"
#include "game/world12/match_projection.hpp"
#include "game/world12/codec.hpp"
#include "game/world12/save.hpp"
#include "cli/json.hpp"
#include "game/matchday_setup.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
namespace {
using namespace bmh;using namespace bmh::world12;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class F>void rejects(F action,const char* message){bool failed=false;try{action();}catch(const std::exception&){failed=true;}check(failed,message);}
GameState rules() {
    GameState g;g.club_ids.resize(445);g.team_flags.resize(445);g.team_morale.resize(445);
    g.formations.resize(0x580);g.manager_records.resize(4*0xb0c);g.manager_ratings.resize(240);
    g.tactics.resize(4*0x840);g.manager_options.resize(20);
    for(size_t m=0;m<4;++m)g.manager_options[m*5]=1;
    const uint8_t depth[]={0,5,5,5,5,10,10,10,10,17,17},lane[]={4,1,3,5,7,1,3,5,7,3,5};
    for(size_t m=0;m<4;++m)for(size_t mode=0;mode<2;++mode)for(size_t f=0;f<4;++f)for(size_t r=0;r<11;++r){const auto at=m*0x160+mode*0xb0+f*22+r;g.formations[at]=depth[r];g.formations[at+11]=lane[r];}
    return g;
}
}
int main()try {
    set_fixture_tables(bmh::test::authored_fixture_tables());const auto base=rules();
    CareerSnapshot state;state.world=bmh::test::world12::complete_world();state.world.players_initialized=true;
    const auto club=state.world.membership[0].clubs[0],opponent=state.world.membership[5].clubs[0];
    state.world.managers={{3,"Manager",club,{0},3}};WatcomRandom random(71);
    initialize_manager_contracts(state.world,3,base,{3,false,false,false,6},random);
    state.results=SeasonResults::empty(state.world.catalogue);
    auto& match=*state.world.managers[0].match_state;
    check(match.attendance.has_value(),"fresh manager did not capture the known empty gate history");
    auto& a=*match.attendance;a.league_home_count=19;
    for(size_t i=0;i<19;++i){a.spectators[i]=10000+uint32_t(i)*100;a.catering[i]=8000+uint32_t(i);a.ticket_prices[i]=uint8_t(10+i%8);}
    a.match_counts={19,7,8};a.spectator_totals={300000,200000,100000};a.cup_income={4000000000u,4294967295u};
    a.record_spectators={20000,30000,40000};a.record_spectator_dates={15,30,45};
    a.record_income={700000,800000,900000};a.record_income_dates={16,31,46};
    a.spectator_opponents[1]=opponent;a.income_opponents[2]=opponent;
    auto projected=AiMatchProjection::create_for_humans(state,{club},base,{3,6,false,true});
    const auto raw_name=projected.state.club_ids.at(projected.local_club_reference(opponent));
    check(raw_name&&raw_name!=opponent.value,"global attendance identity was packed as a native name pointer");
    const auto at=size_t(0x8c2);
    check(uint16_t(projected.state.manager_records[at]|uint16_t(projected.state.manager_records[at+1])<<8)==raw_name,
          "attendance opponent name translation was lost");
    const auto before=state.world;projected.commit_match_state(state.world);
    check(state.world==before,"unchanged bounded attendance projection mutated history/references");
    check(read_save(make_save(state))==state,"attendance counters/global record opponents did not survive snapshot");
    MatchGroup group;group.desc.assign(0xd0,0);group.desc[1]=2;MatchdayContext context;
    context.projected_matchdays=38;context.projected_completed_rounds=33;MinuteReport report;
    const auto english=match_mode_bits(projected.state,group,context,report);
    context.projected_matchdays=34;const auto german=match_mode_bits(projected.state,group,context,report);
    check((english&31)==5&&(german&31)==17,"English interest used the German 34-round season length");
    auto unknown=state;unknown.world.managers[0].match_state->attendance.reset();
    auto old_projection=AiMatchProjection::create_for_humans(unknown,{club},base,{3,6,false,true});
    old_projection.commit_match_state(unknown.world);
    check(!unknown.world.managers[0].match_state->attendance,"ordinary projection invented older missing attendance history");
    auto invalid=state;invalid.world.managers[0].match_state->attendance->league_home_count=20;
    rejects([&]{make_save(invalid);},"invalid attendance cursor accepted");
    invalid=state;invalid.world.managers[0].match_state->attendance->income_opponents[0]={4294967294u};
    rejects([&]{make_save(invalid);},"unknown attendance record opponent accepted");
    const auto bytes=encode_world(state.world);auto json=cli::parse_json(std::string(bytes.begin(),bytes.end()));
    json.object()->at("version")=24;auto text=cli::dump_json(json);
    rejects([&]{decode_world(Bytes(text.begin(),text.end()));},"older schema silently discarded known attendance history");
    auto& manager=std::get<cli::Json::Array>(json.object()->at("managers").value)[0];manager.object()->at("match_state").object()->erase("attendance");
    text=cli::dump_json(json);check(!decode_world(Bytes(text.begin(),text.end())).managers[0].match_state->attendance,
          "schema24 migration fabricated lost gate records");
    std::cout<<"Attendance history scalar ranges, global opponent/name references, source roundtrip and explicit old-schema unknown state passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
