// SPDX-License-Identifier: GPL-3.0-only
#include "world12_fixture.hpp"
#include "game/world12/manager_creation.hpp"
#include "game/world12/match_projection.hpp"
#include "game/world12/codec.hpp"
#include <iostream>
using namespace bmh;using namespace bmh::world12;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class F>void rejects(F action,const char* message){bool rejected=false;try{action();}catch(const std::exception&){rejected=true;}check(rejected,message);}
GameState reference(){GameState g;g.manager_records.resize(4*0xb0c);g.memory.add_block(layout::Flags,12);g.memory.add_block(layout::EnvironmentYouthTeams,4*0x24);g.memory.add_block(layout::Spending,4*0x24);for(size_t seat=0;seat<4;++seat){for(size_t n=0;n<10;++n)g.manager_records[seat*0xb0c+0x8b6+n]=255;g.memory.set8(layout::Flags+uint32_t(seat)*3,uint8_t(0x40+seat));g.memory.set8(layout::Flags+uint32_t(seat)*3+1,uint8_t(0x50+seat));g.memory.set8(layout::Flags+uint32_t(seat)*3+2,uint8_t(0x60+seat));}return g;}
}
int main()try{
    auto native=reference();auto known=manager_career_state_from_legacy(native,3);check(known.statistics_flags==0x43&&known.stadium_curve_flags==0x53&&known.player_statistics_flags==0x63,"fresh Source capture lost independent flags0/1/2");
    auto unknown=known;unknown.statistics_flags.reset();unknown.player_statistics_flags.reset();auto target=reference();install_manager_career_state(target,3,unknown);check(target.memory.u8(layout::Flags+9)==0x43&&target.memory.u8(layout::Flags+10)==0x53&&target.memory.u8(layout::Flags+11)==0x63,"unknown statistics preferences overwrote Source neighbors");
    check(!manager_career_state_from_legacy(native,3,{},false,&unknown).statistics_flags&&!manager_career_state_from_legacy(native,3,{},false,&unknown).player_statistics_flags,"generic capture invented old unknown preference values");
    native.memory.set8(layout::Flags+11,0xa5);capture_manager_statistics_flags(native,3,unknown,4);check(!unknown.statistics_flags&&unknown.player_statistics_flags==0xa5&&unknown.stadium_curve_flags==0x53,"owned player-page write promoted another unknown byte or altered curve owner");
    known.statistics_flags=0x7f;known.player_statistics_flags=0x81;install_manager_career_state(target,3,known);check(target.memory.u8(layout::Flags+9)==0x7f&&target.memory.u8(layout::Flags+10)==0x53&&target.memory.u8(layout::Flags+11)==0x81&&target.memory.u8(layout::Flags+8)==0x62,"statistics installation mixed sparse seats or neighboring bytes");
    auto world=bmh::test::world12::complete_world();world.players_initialized=true;world.free_players.emplace();world.markets.emplace();world.settings=CareerSettings{3,2,5,1,false};world.managers={{3,"Flags",world.membership[0].clubs[0],{0},3}};world.managers[0].career=known;const auto encoded=encode_world(world);check(decode_world(encoded)==world,"statistics flags schema41 roundtrip changed actual independent owners");
    auto json=cli::parse_json(std::string(encoded.begin(),encoded.end()));json.object()->at("version")=40;auto bytes=cli::dump_json(json);rejects([&]{decode_world(Bytes(bytes.begin(),bytes.end()));},"older schema accepted known41statistics values");
    auto& entry=std::get<cli::Json::Array>(json.object()->at("managers").value)[0];entry.object()->at("career").object()->erase("statistics_flags");bytes=cli::dump_json(json);auto old=decode_world(Bytes(bytes.begin(),bytes.end()));check(!old.managers[0].career->statistics_flags&&!old.managers[0].career->player_statistics_flags,"old snapshot fabricated statistics flags");
    std::cout<<"SourceFlags0/1/2: independent known/unknowns, sparse slots, authoritative masked capture, schema41/older guards passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
