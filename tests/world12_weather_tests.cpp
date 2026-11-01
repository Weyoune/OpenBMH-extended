// SPDX-License-Identifier: GPL-3.0-only
#include "world12_fixture.hpp"
#include "app/world12_weather.hpp"
#include "cli/json.hpp"
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
namespace {
using namespace bmh;using namespace bmh::world12;namespace weather_app=bmh::app::world12;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class F>void rejects(F f,const char* message){bool failed=false;try{f();}catch(const std::exception&){failed=true;}check(failed,message);}
}
int main()try {
    const auto world=bmh::test::world12::complete_world();const std::vector<uint8_t> catalogue(50,0x24);
    cli::Json::Array names,properties;
    for(size_t i=0;i<50;++i){names.emplace_back("Referee "+std::to_string(i));properties.emplace_back(0x24);}
    cli::Json data(cli::Json::Object{{"format","openbmh-referees"},{"version",1},{"names",names},{"properties",properties}});
    const auto encoded=cli::dump_json(data);const auto decoded=weather_app::decode_referee_catalogue(Bytes(encoded.begin(),encoded.end()));
    check(decoded.names.size()==50&&decoded.names[49]=="Referee 49"&&decoded.properties==catalogue,"source referee catalogue decode changed names/properties");
    auto invalid_catalogue=data;invalid_catalogue.object()->at("version")=2;
    auto bad=cli::dump_json(invalid_catalogue);rejects([&]{weather_app::decode_referee_catalogue(Bytes(bad.begin(),bad.end()));},"unknown referee metadata version accepted");
    invalid_catalogue=data;std::get<cli::Json::Array>(invalid_catalogue.object()->at("names").value).pop_back();
    bad=cli::dump_json(invalid_catalogue);rejects([&]{weather_app::decode_referee_catalogue(Bytes(bad.begin(),bad.end()));},"short referee catalogue accepted");
    invalid_catalogue=data;std::get<cli::Json::Array>(invalid_catalogue.object()->at("properties").value)[49]=256;
    bad=cli::dump_json(invalid_catalogue);rejects([&]{weather_app::decode_referee_catalogue(Bytes(bad.begin(),bad.end()));},"invalid late referee property accepted");
    std::vector<weather_app::FixtureWeatherRequest> requests;
    for(const auto& group:world.membership)for(size_t i=0;i<group.clubs.size();i+=2)
        requests.push_back({{group.clubs[i],group.clubs[i+1]},2});
    check(requests.size()==117,"full dual-world fixture count differs");
    GameRandom game(31),again_game(31);WatcomRandom random(713),again_random(713);
    const auto result=weather_app::prepare_fixture_weather(world,7,requests,catalogue,game,random);
    const auto repeated=weather_app::prepare_fixture_weather(world,7,requests,catalogue,again_game,again_random);
    check(result.matches==repeated.matches&&game.seed()==again_game.seed()&&random.seed()==again_random.seed()&&
          result.watcom_draws==234&&result.game_draws>=117,"weather preparation lost repeatability/source draw count");
    std::map<Country,std::set<uint8_t>> used;
    for(const auto& match:result.matches) {
        check(match.request.fixture.home.valid()&&match.weather.rain<=7&&
              used[match.referee_country].insert(match.weather.referee).second,
              "country referee reused or fixture identity lost");
        check(match.referee_country==world.clubs.at(match.request.fixture.home).country,"referee mask belongs to another country");
    }
    check(used[Country::germany].size()==45&&used[Country::england].size()==45&&catalogue==std::vector<uint8_t>(50,0x24),
          "dual domestic programmes exhausted/mutated a global catalogue");
    auto oversized=requests;for(size_t i=0;i<5;++i)oversized.push_back(requests.front());
    const auto gs=game.seed();const auto ws=random.seed();const auto gc=game.calls(),wc=random.calls();
    rejects([&]{weather_app::prepare_fixture_weather(world,7,oversized,catalogue,game,random);},"overlarge single-country pool was accepted");
    check(game.seed()==gs&&random.seed()==ws&&game.calls()==gc&&random.calls()==wc,"bad weather batch consumed random streams");
    auto invalid=requests;invalid.back().fixture.away={};
    rejects([&]{weather_app::prepare_fixture_weather(world,7,invalid,catalogue,game,random);},"missing late weather participant was accepted");
    check(game.seed()==gs&&random.seed()==ws,"late identity error drew partial country weather");
    std::cout<<"117 simultaneous fixtures: country referee masks, source weather, retained identities, replay and pre-draw rejection passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
