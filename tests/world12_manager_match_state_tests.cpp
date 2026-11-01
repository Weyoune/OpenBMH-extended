// SPDX-License-Identifier: GPL-3.0-only
#include "world12_fixture.hpp"
#include "game/world12/manager_match_state.hpp"
#include "game/world12/codec.hpp"
#include "game/world12/save.hpp"
#include "cli/json.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
namespace {
using namespace bmh::world12;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class Function> void rejects(Function function,const char* message){bool rejected=false;try{function();}catch(const std::exception&){rejected=true;}check(rejected,message);}
bmh::GameState workspace() {
    bmh::GameState state;state.manager_records.assign(4*0xb0c,0x5a);state.manager_ratings.assign(4*60,0xa5);
    state.manager_results.assign(4*size_t(bmh::runtime_dimensions.record_teams)*10,255);return state;
}
ManagerMatchState sample(const std::vector<ClubId>& clubs,const std::vector<PlayerId>& players) {
    ManagerMatchState result;
    for(size_t i=0;i<21;++i){result.preferences[i]=uint8_t(i+1);result.best_streaks[i]=uint8_t(21-i);}
    for(size_t i=0;i<72;++i)result.timing_stats[i]=uint8_t(i*3);
    for(size_t i=0;i<6;++i){result.staff_ratings[i]=uint16_t(500+i);result.recent_scores[i]=uint8_t(i*0x11);}
    for(size_t i=0;i<60;++i)result.rating_history[i]=uint8_t(i+50);
    result.tax_policy=0xe5;result.sponsor_win_bonus=7;result.crowd_pressure=1;result.goal_notices=37;result.point_deduction=0x65;
    result.recent_fixtures[0]=clubs[0];result.recent_fixtures[1]=clubs[1];result.recent_fixtures[2]=clubs[2];result.recent_fixtures[3]=clubs[0];
    std::array<uint8_t,10> scores;scores.fill(255);scores[0]=0x21;scores[5]=0x10;result.head_to_head[clubs[1]]=scores;result.head_to_head[{0xf0000000u}]=scores;
    result.designated_takers=true;result.takers={players[1],PlayerId{},players[0]};return result;
}
}
int main()try {
    const std::vector<ClubId> clubs{{100000},{0xf0000001u},{300000}};
    const std::vector<PlayerId> players{{200000},{0xe0000001u}};
    const auto input=sample(clubs,players);auto native=workspace();const auto before=native;
    install_manager_match_state(native,3,input,clubs,players);
    const auto read=manager_match_state_from_legacy(native,3,clubs,players,&input);
    check(read==input,"manager match bridge lost scalar or global history/taker data");
    const auto known=[](size_t at){return (at>=0xc5&&at<0xda)||(at>=0x5e&&at<0x73)||(at>=0x1ae&&at<0x1f6)||
        (at>=0x212&&at<0x218)||(at>=0x292&&at<0x2aa)||(at>=0x8aa&&at<0x8b6)||(at>=0xafc&&at<0xb00)||(at>=0xb08&&at<0xb0c)||
        at==0x172||at==0x173||at==0x289||at==0x28b||at==0x220;};
    for(size_t i=0;i<native.manager_records.size();++i)if(i<3*0xb0c||!known(i-3*0xb0c))check(native.manager_records[i]==before.manager_records[i],"bridge overwrote another manager or unrelated record bytes");
    check(std::equal(before.manager_ratings.begin(),before.manager_ratings.begin()+180,native.manager_ratings.begin()),"bridge changed another manager's ratings");
    const auto installed=native;auto invalid=input;invalid.takers[0]={999999};
    rejects([&]{install_manager_match_state(native,3,invalid,clubs,players);},"unknown designated taker accepted");
    check(native.manager_records==installed.manager_records&&native.manager_results==installed.manager_results&&native.manager_ratings==installed.manager_ratings,"failed reference validation partially wrote manager data");
    invalid=input;invalid.recent_fixtures[4]=clubs[0];
    rejects([&]{install_manager_match_state(native,3,invalid,clubs,players);},"partial fixture accepted");
    auto corrupt=installed;corrupt.manager_records[3*0xb0c+0x292]=0xfe;corrupt.manager_records[3*0xb0c+0x293]=0;
    rejects([&]{manager_match_state_from_legacy(corrupt,3,clubs,players);},"unmapped recent club persisted");
    auto world=bmh::test::world12::complete_world();const std::vector<ClubId> actual_clubs{world.membership[0].clubs[0],world.membership[0].clubs[1],world.membership[5].clubs[0]};
    const std::vector<PlayerId> actual_players{world.clubs.at(actual_clubs[0]).roster[0].player,world.clubs.at(actual_clubs[0]).roster[1].player};
    world.managers={{3,"Human",actual_clubs[0],{0},3}};world.managers[0].match_state=sample(actual_clubs,actual_players);
    world.managers[0].match_state->head_to_head.erase({0xf0000000u});
    world.validate();check(decode_world(encode_world(world))==world,"manager match model codec lost data");
    CareerSnapshot snapshot;snapshot.world=world;snapshot.results=SeasonResults::empty(world.catalogue);check(read_save(make_save(snapshot))==snapshot,"manager match data lost on snapshot reload");
    auto raw=encode_world(world);auto old=bmh::cli::parse_json(std::string(raw.begin(),raw.end()));old.object()->at("version")=8;
    for(auto& manager:std::get<bmh::cli::Json::Array>(old.object()->at("managers").value))manager.object()->erase("match_state");
    const auto json=bmh::cli::dump_json(old);check(!decode_world(bmh::Bytes(json.begin(),json.end())).managers[0].match_state,"old manager was assigned invented match fields");
    auto missing=world;missing.managers[0].match_state->recent_fixtures[1]={9999999};rejects([&]{missing.validate();},"dangling manager history ID accepted");
    std::cout<<"Manager match fields, wide histories/takers, unmapped-history preservation, atomic reference checks and codec/snapshot compatibility passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
