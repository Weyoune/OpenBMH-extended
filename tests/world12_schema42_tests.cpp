// SPDX-License-Identifier: GPL-3.0-only
#include "world12_fixture.hpp"
#include "game/world12/national_coach.hpp"
#include "game/world12/codec.hpp"
#include "cli/json.hpp"
#include <iostream>
namespace {
using namespace bmh;using namespace bmh::world12;
void check(bool yes,const char* text){if(!yes)throw std::runtime_error(text);}
Bytes encode_json(const cli::Json& value){const auto text=cli::dump_json(value);return {text.begin(),text.end()};}
void rejects42(const cli::Json& value,const char* text){try{(void)decode_world(encode_json(value));}catch(const FormatError& error){check(std::string(error.what()).find("older schema")!=std::string::npos,text);return;}throw std::runtime_error(text);}
}
int main()try {
    auto world=bmh::test::world12::complete_world();world.catalogue.scope.international=true;world.players_initialized=true;world.free_players.emplace();world.markets.emplace();world.settings=CareerSettings{3,2,5,1,false};
    for(auto country:{Country::germany,Country::england}){const auto c=uint32_t(country);ClubRecord nation;nation.id={0xff000000u+c};nation.country=country;nation.kind=ClubKind::national_team;nation.name="Authored nation "+std::to_string(c);nation.crest=world.clubs.begin()->second.crest;world.clubs[nation.id]=nation;}
    GameState reference;reference.manager_records.resize(5*0xb0c);initialize_national_coaches(world,reference);
    world.national_coaches->at(0).account.investment_balance=0xffffffffu;world.national_coaches->at(1).account.depreciation=0;
    check(decode_world(encode_world(world))==world,"schema42 lost known maximum/zero national bank facts");
    const auto bytes=encode_world(world);auto json=cli::parse_json(std::string(bytes.begin(),bytes.end()));json.object()->at("version")=41;rejects42(json,"forged41 silently dropped known national bank facts");
    for(auto& role:*world.national_coaches){role.account.investment_balance.reset();role.account.depreciation.reset();}
    const auto unknown_bytes=encode_world(world);json=cli::parse_json(std::string(unknown_bytes.begin(),unknown_bytes.end()));json.object()->at("version")=41;check(decode_world(encode_json(json))==world,"old41 unknown national accounts were reconstructed or rejected");
    for(const char* key:{"startup","view_preferences","desktop_layout","investment_balance","depreciation"}){auto downgrade=json;auto& roles=std::get<cli::Json::Array>(downgrade.object()->at("national_coaches").value);roles[0].object()->at(key)=cli::Json::Object{};rejects42(downgrade,"forged41 accepted a nonempty schema42 national payload");}
    auto inactive=json;inactive.object()->at("inactive_club_state")=cli::Json::Object{};rejects42(inactive,"forged41 silently discarded inactive club state");
    json.object()->erase("inactive_club_state");check(decode_world(encode_json(json))==world,"old absent inactive club state failed unknown preservation");
    std::cout<<"Schema42: real nullable national bank facts, known zero/max, old unknown preservation, startup/view/layout/inactive-state downgrade guards passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
