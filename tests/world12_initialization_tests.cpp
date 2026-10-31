// SPDX-License-Identifier: GPL-3.0-only
#include "world12_fixture.hpp"
#include "game/world12/initialization.hpp"
#include "game/world12/codec.hpp"
#include "game/world12/save.hpp"
#include <iostream>
#include <stdexcept>
namespace {
using namespace bmh::world12;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class Function> void rejects(Function function,const char* message){bool rejected=false;try{function();}catch(const std::exception&){rejected=true;}check(rejected,message);}
World master_world() {
    auto world=bmh::test::world12::complete_world();
    for(uint32_t i=0;i<54;++i){PlayerRecord p;p.id={1000000+i};p.attributes={"Extra",50u|(60u<<7)|(65u<<14),2u<<27,i%4};world.players[p.id]=p;}
    return world;
}
InitialStrengthRanges ranges() {InitialStrengthRanges ranges;for(size_t i=0;i<ranges.size();++i)ranges[i]={uint8_t(20+i),uint8_t(60+i)};return ranges;}
World* interrupted=nullptr;bmh::GameRandom* interrupted_game=nullptr;
void change_world(int){bmh::draw_hook=nullptr;++interrupted->players.begin()->second.cards;}
void change_random(int){bmh::draw_hook=nullptr;interrupted_game->range(0,1);}
}
int main()try {
    const auto master=master_world();
    for(int mask=1;mask<8;++mask) {
        const GameScope scope{bool(mask&1),bool(mask&2),bool(mask&4)};
        auto world=select_world_scope(master,scope),repeat=world;
        const auto plan=make_player_initialization_plan(master,scope,ranges());
        bmh::GameRandom game(17),repeat_game(17);bmh::WatcomRandom random(91),repeat_random(91);
        const auto outcome=initialize_career_players(world,plan,game,random);
        initialize_career_players(repeat,plan,repeat_game,repeat_random);
        check(world.players_initialized && world==repeat && game.seed()==repeat_game.seed() && random.seed()==repeat_random.seed(),"initial player generation changed across repeat");
        check(outcome.players==world.players.size() && outcome.game_draws==game.calls() && outcome.watcom_draws==random.calls(),"initial generation omitted players or lost draw counts");
        for(const auto& [id,p]:world.players) {
            const auto& before=master.players.at(id);
            check(p.id==before.id && p.attributes.name==before.attributes.name && p.bonus==before.bonus && p.cards==0,"generation changed stable metadata/cards");
        }
        check(decode_world(encode_world(world))==world,"initialized player flag/state lost on world roundtrip");
        CareerSnapshot snapshot;snapshot.world=world;snapshot.results=SeasonResults::empty(world.catalogue);
        check(read_save(make_save(snapshot))==snapshot,"initialized player state lost on snapshot roundtrip");
        const auto before=world;const auto old_game=game;const auto old_random=random;
        rejects([&]{initialize_career_players(world,plan,game,random);},"career player generation ran twice");
        check(world==before && game.seed()==old_game.seed() && random.seed()==old_random.seed(),"repeat initialization changed world/RNG");
        rejects([&]{select_world_scope(world,scope);},"initialized players re-scoped as untouched data");
        if(mask==1) {
            std::vector<LeagueId> order;for(const auto& group:plan.groups)if(order.empty()||order.back()!=group.league)order.push_back(group.league);
            check(order==std::vector<LeagueId>{{0},{1},{2},{3},{4},{10},{5},{11},{12}},"German fallback generation changed Original international order");
        }
    }
    const GameScope scope{true,false,true};const auto plan=make_player_initialization_plan(master,scope,ranges());
    auto world=select_world_scope(master,scope);const auto before=world;bmh::GameRandom game(17);bmh::WatcomRandom random(91);
    auto bad=plan;bad.groups[0].players[0]=bad.extra_players[0];
    rejects([&]{initialize_career_players(world,bad,game,random);},"forged initial squad order accepted");
    check(world==before && game.calls()==0 && random.calls()==0,"invalid plan partially generated players");
    interrupted=&world;bmh::draw_hook=change_world;
    rejects([&]{initialize_career_players(world,plan,game,random);},"external world change overwritten during generation");bmh::draw_hook=nullptr;
    auto changed=before;++changed.players.begin()->second.cards;check(world==changed && game.calls()==0 && random.calls()==0,"candidate leaked or external player change was lost");
    world=before;interrupted_game=&game;auto external_game=game;external_game.range(0,1);bmh::draw_hook=change_random;
    rejects([&]{initialize_career_players(world,plan,game,random);},"external generation RNG owner change overwritten");bmh::draw_hook=nullptr;
    check(world==before && game.seed()==external_game.seed() && game.calls()==external_game.calls() && random.calls()==0,"external generation RNG changes lost");
    std::cout<<"Seven fresh-career player scopes, Original ordering, stable IDs, deterministic generation, persistence and late ownership rollback passed\n";
    return 0;
}catch(const std::exception& error){bmh::draw_hook=nullptr;std::cerr<<error.what()<<'\n';return 1;}
