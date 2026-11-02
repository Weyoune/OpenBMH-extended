// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/world.hpp"
#include "game/world12/fixtures.hpp"
#include "game/matchday_setup.hpp"
#include "app/world12_cups.hpp"
namespace bmh::app::world12 {
struct RefereeCatalogue {
    std::vector<std::string> names;
    std::vector<uint8_t> properties;
};
RefereeCatalogue decode_referee_catalogue(const Bytes&);
struct FixtureWeatherRequest {
    bmh::world12::Fixture fixture;
    uint8_t flags=2;
    bool operator==(const FixtureWeatherRequest&)const=default;
};
struct FixtureWeather {
    FixtureWeatherRequest request;
    MatchWeatherOutcome weather;
    bmh::world12::Country referee_country=bmh::world12::Country::germany;
    bool operator==(const FixtureWeather&)const=default;
};
struct WeatherPreparation {
    std::vector<FixtureWeather> matches;
    uint64_t game_draws=0,watcom_draws=0;
};
// Each country's simultaneous programme has its own availability mask over the
// supplied catalogue. One global 49-referee mask cannot cover the larger world.
// The event owner persists this result together with the post-preparation RNG.
WeatherPreparation prepare_fixture_weather(const bmh::world12::World&,int month,
    const std::vector<FixtureWeatherRequest>&,const std::vector<uint8_t>& catalogue,
    GameRandom&,WatcomRandom&);
WeatherPreparation prepare_event_weather(bmh::world12::CareerSnapshot&,ManagerActionOwner&,
    ManagerActionOwner expected,const std::vector<uint8_t>& catalogue,const CupSources&,const GameState* indoor_reference=nullptr);
}
