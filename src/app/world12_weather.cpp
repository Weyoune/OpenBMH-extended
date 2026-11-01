// SPDX-License-Identifier: GPL-3.0-only
#include "app/world12_weather.hpp"
#include "cli/json.hpp"
#include <algorithm>
#include <map>
namespace bmh::app::world12 {
using namespace bmh::world12;
RefereeCatalogue decode_referee_catalogue(const Bytes& bytes) {
    if(bytes.size()>65536)throw FormatError("world12: referee catalogue is too large");
    const auto root=cli::parse_json(std::string(bytes.begin(),bytes.end()));
    const auto get=[&](const char* name)->const cli::Json& {
        if(!root.object())throw FormatError("world12: invalid referee catalogue object");
        const auto found=root.object()->find(name);
        if(found==root.object()->end())throw FormatError("world12: incomplete referee catalogue");return found->second;
    };
    const auto& format=get("format");const auto& version=get("version");
    if(!format.string()||*format.string()!="openbmh-referees"||!version.integer()||*version.integer()!=1)
        throw FormatError("world12: unsupported referee catalogue");
    const auto& names=get("names");const auto& properties=get("properties");
    if(!names.array()||names.array()->size()!=50||!properties.array()||properties.array()->size()!=50)
        throw FormatError("world12: referee catalogue must contain fifty entries");
    RefereeCatalogue result;
    for(size_t i=0;i<50;++i) {
        const auto& name=names.array()->at(i);const auto& property=properties.array()->at(i);
        if(!name.string()||name.string()->empty()||name.string()->size()>255||name.string()->find('\0')!=std::string::npos||
           !property.integer()||*property.integer()<0||*property.integer()>255)
            throw FormatError("world12: invalid referee name/properties");
        result.names.push_back(*name.string());result.properties.push_back(uint8_t(*property.integer()));
    }
    return result;
}
WeatherPreparation prepare_fixture_weather(const World& world,int month,
    const std::vector<FixtureWeatherRequest>& requests,const std::vector<uint8_t>& catalogue,
    GameRandom& game,WatcomRandom& random) {
    world.validate();
    if(month<1||month>12||catalogue.size()!=50)throw FormatError("world12: invalid match weather/referee resources");
    std::map<Country,size_t> counts;
    // OpenBMH deviates from the original: referee availability is scoped to
    // each country's programme so the shared world can exceed 49 matches.
    for(const auto& request:requests) {
        const auto home=world.clubs.find(request.fixture.home),away=world.clubs.find(request.fixture.away);
        if(home==world.clubs.end()||away==world.clubs.end()||home==away)
            throw FormatError("world12: invalid weather fixture identities");
        const bool referee=home->second.kind==ClubKind::club&&away->second.kind==ClubKind::club&&!(request.flags&0x80);
        if(referee&&++counts[home->second.country]>49)
            throw FormatError("world12: country programme exceeds its referee pool");
    }
    auto staged_game=game;auto staged_random=random;
    const auto previous_game=game.calls(),previous_random=random.calls();
    std::map<Country,std::vector<uint8_t>> pools;WeatherPreparation result;
    for(const auto& request:requests) {
        const auto& home=world.clubs.at(request.fixture.home);const auto& away=world.clubs.at(request.fixture.away);
        auto [pool,inserted]=pools.try_emplace(home.country,catalogue);
        if(inserted)for(auto& property:pool->second)property&=0x7f;
        const bool referee=home.kind==ClubKind::club&&away.kind==ClubKind::club&&!(request.flags&0x80);
        const auto weather=draw_match_weather(int(home.country),month,referee,pool->second,staged_game,staged_random);
        result.matches.push_back({request,weather,home.country});
    }
    result.game_draws=staged_game.calls()-previous_game;result.watcom_draws=staged_random.calls()-previous_random;
    game=staged_game;random=staged_random;return result;
}
}
