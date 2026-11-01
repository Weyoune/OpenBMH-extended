// SPDX-License-Identifier: GPL-3.0-only
#include "game/matchday_setup.hpp"
#include "game/match.hpp"
#include <iostream>
#include <set>
#include <stdexcept>
namespace {
using namespace bmh;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class F>void rejects(F action,const char* message){bool failed=false;try{action();}catch(const FormatError&){failed=true;}check(failed,message);}
}
int main()try {
    std::vector<uint8_t> refs(50,0x24);GameRandom game(31);WatcomRandom random(713);
    std::set<uint8_t> chosen;
    for(size_t i=0;i<49;++i) {
        const auto weather=draw_match_weather(0,1,true,refs,game,random);
        check(weather.referee>=1&&weather.referee<50&&chosen.insert(weather.referee).second,
              "referee reused inside an available native pool");
        check((refs[weather.referee]&0x7f)==0x24&&(refs[weather.referee]&0x80),"selection changed referee skill bits");
    }
    const auto blocked_refs=refs;const auto gs=game.seed();const auto ws=random.seed();
    const auto gc=game.calls(),wc=random.calls();
    rejects([&]{draw_match_weather(0,1,true,refs,game,random);},"exhausted referee pool did not reject instead of looping");
    check(refs==blocked_refs&&game.seed()==gs&&random.seed()==ws&&game.calls()==gc&&random.calls()==wc,
          "exhaustion rejection consumed weather/randomness");
    auto short_refs=std::vector<uint8_t>(2);
    rejects([&]{draw_match_weather(0,1,true,short_refs,game,random);},"short referee table accepted");
    std::vector<uint8_t> unused;GameRandom g1(7),g2(7);WatcomRandom w1(91),w2(91);
    const auto de=draw_match_weather(0,7,false,unused,g1,w1);
    const auto en=draw_match_weather(2,7,false,unused,g2,w2);
    check(uint8_t(de.temperature+region_temperature(2)-region_temperature(0))==en.temperature&&
          de.rain==en.rain&&g1.calls()==0&&g2.calls()==0&&w1.calls()==2&&w1.seed()==w2.seed(),
          "explicit regional climate changed weather draw count or geography");
    bool snow=false,negative=false;
    for(uint32_t seed=0;seed<500;++seed) {
        GameRandom g(seed);WatcomRandom w(seed);
        const auto weather=draw_match_weather(0,12,false,unused,g,w);
        if(weather.rain>5){check(weather.temperature<2,"snow test stopped using the Original unsigned byte");snow=true;}
        if(weather.temperature>=128){check(weather.rain<=5,"negative temperature was treated as Original zero/one snow");negative=true;}
    }
    check(snow&&negative,"weather boundary fixture never exercised snow and signed temperature");
    std::cout<<"Native weather geography, snow byte boundaries, referee uniqueness/skill bits and pre-draw exhaustion rejection passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
