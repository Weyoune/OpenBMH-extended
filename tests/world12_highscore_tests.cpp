// SPDX-License-Identifier: GPL-3.0-only
#include "annual_accounts_fixture.hpp"
#include "fixture_tables.hpp"
#include "game/world12/highscores.hpp"
#include "game/world12/manager_tactics.hpp"
#include "game/world12/economy.hpp"
#include "game/world12/codec.hpp"
#include <iostream>
namespace {
using namespace bmh;using namespace bmh::world12;
void check(bool yes,const char* text){if(!yes)throw std::runtime_error(text);}
template<class F>void rejects(F f,const char* text){bool refused=false;try{f();}catch(const std::exception&){refused=true;}check(refused,text);}
template<class Bytes>void put(Bytes& bytes,size_t at,uint32_t value,int width=4){for(int n=0;n<width;++n)bytes.at(at+size_t(n))=uint8_t(value>>(8*n));}
}
int main()try {
    set_fixture_tables(bmh::test::authored_fixture_tables());const auto rules=bmh::test::world12::annual_reference();auto source=bmh::test::world12::completed_account_closing(rules,{true,true,false},false);
    auto& world=source.world;const auto fields=world.initial_competitions;const auto campaigns=world.european_campaigns;const auto market=world.transfer_market;world.transfer_market.reset();world.initial_competitions.reset();world.european_campaigns.reset();world.settings->planned_managers=2;
    Manager manager{0,"Second manager",world.membership[5].clubs[2],{5},0};manager.career.emplace();manager.career->season_records.emplace();manager.career->annual_records.emplace();manager.career->cup_titles.emplace();manager.career->season_services.emplace();world.managers.push_back(manager);
    WatcomRandom random(61);initialize_manager_contracts(world,0,rules,{3,false,false,false,2},random);std::sort(world.managers.begin(),world.managers.end(),[](const auto& a,const auto& b){return a.seat<b.seat;});world.initial_competitions=fields;world.european_campaigns=campaigns;world.transfer_market=market;source.desktop->managers.push_back({0});
    world.economy->managers[0].monthly.emplace();world.economy->managers[0].depreciation=0;
    world.managers[0].finance->balance=1000000;world.managers[1].finance->balance=2000000;world.economy->managers[0].deposit=250000;world.economy->managers[3].deposit=500000;
    put(world.markets->stocks,0x4e,100,2);put(world.markets->stocks,0x50,2,2);put(world.markets->stocks,0x56,3,2);
    put(world.markets->funds,0,100000);put(world.markets->funds,0x14,1,2);put(world.markets->funds,0x1a,2,2);
    world.highscore_signature=151;source.validate();const auto before=source;const auto projected=project_owned_highscores(source,rules);
    check(projected.ranked&&projected.signature==151&&projected.managers.size()==2&&source==before,"owned highscore lost signature, sparse managers or changed career");
    check(projected.managers[0].manager==0&&projected.managers[1].manager==3&&projected.managers[0].score.assets==27&&projected.managers[1].score.assets==54,"typed highscore used a spare/global manager's cash or portfolios");
    auto local=AiMatchProjection::create_for_humans(source,{world.managers[0].club,world.managers[1].club},rules,{3,2,false,true});install_career_economy(local,world);
    local.state.manager_records.resize(3*0xb0c);put(local.state.manager_records,2*0xb0c+0x314,7000000);local.state.memory.set8(layout::ActiveManager,2);
    for(int m=0;m<2;++m){check(highscore_score(local.state,m,HighscoreContext::OwnedManagerAssets).assets==(m?54:27),"shared explicit owned asset context changed manager binding");check(highscore_score(local.state,m,HighscoreContext::SeasonBoundary).assets==100&&highscore_score(local.state,m,HighscoreContext::ActiveManager).assets==100,"Legacy highscore source-global contexts were changed");}
    check(decode_world(encode_world(world))==world,"schema41 lost known highscore signature");
    const auto encoded=encode_world(world);auto old=cli::parse_json(std::string(encoded.begin(),encoded.end()));old.object()->at("version")=40;auto text=cli::dump_json(old);rejects([&]{decode_world(Bytes(text.begin(),text.end()));},"schema40 silently accepted a known41 signature");
    old.object()->erase("highscore_signature");old.object()->erase("inactive_national_draw");text=cli::dump_json(old);check(!decode_world(Bytes(text.begin(),text.end())).highscore_signature,"old40 invented ranked/unranked ownership");
    auto unknown=source;unknown.world.highscore_signature.reset();rejects([&]{project_owned_highscores(unknown,rules);},"unknown old highscore signature became silently unranked");
    auto unranked=source;unranked.world.highscore_signature=0;const auto zero=project_owned_highscores(unranked,rules);check(!zero.ranked&&zero.managers.empty()&&zero.signature==0,"known unranked source invented ranked candidates");
    auto missing=source;missing.world.managers[1].career->annual_records.reset();rejects([&]{project_owned_highscores(missing,rules);},"ranked highscore invented unknown title history");
    std::cout<<"Owned highscore policy: actual sparse-seat cash/deposits/shares/funds, unchanged Legacy contexts, known41signature/oldunknown and pure projection passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
