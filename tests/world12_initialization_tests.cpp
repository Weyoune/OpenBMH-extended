// SPDX-License-Identifier: GPL-3.0-only
#include "world12_fixture.hpp"
#include "game/world12/initialization.hpp"
#include "game/world12/codec.hpp"
#include "game/world12/save.hpp"
#include "game/state.hpp"
#include "cli/json.hpp"
#include <iostream>
#include <stdexcept>
namespace {
using namespace bmh::world12;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class Function> void rejects(Function function,const char* message){bool rejected=false;try{function();}catch(const std::exception&){rejected=true;}check(rejected,message);}
World master_world() {
    auto world=bmh::test::world12::complete_world();
    for(uint32_t i=0;i<54;++i){PlayerRecord p;p.id={1000000+i};p.attributes={"Extra",50u|(60u<<7)|(65u<<14),2u<<27,i%4};world.players[p.id]=p;}
    for(uint32_t i=0;i<5;++i){ClubRecord club;club.id={3000000+i};club.name="Extra club";club.crest={club.id.value};
        club.country=i==0||i==3?Country::germany:i==1||i==4?Country::england:Country::portugal;
        club.kind=i>=3?ClubKind::national_team:ClubKind::club;club.rating=uint8_t(i*51);
        world.crests[club.crest]={"W/TEST.VGA",0};world.clubs[club.id]=club;}
    return world;
}
bmh::GameState market_reference() {
    bmh::GameState result;result.stocks.resize(12*0x5c);
    result.memory.add_block(bmh::layout::FundBlock,6*0x1c);
    // Retain the non-base fund fields supplied by the reference.
    for(uint32_t i=0;i<6*0x1c;++i)result.memory.set8(bmh::layout::FundBlock+i,uint8_t(i*7));
    return result;
}
InitialStrengthRanges ranges() {InitialStrengthRanges ranges;for(size_t i=0;i<ranges.size();++i)ranges[i]={uint8_t(20+i),uint8_t(60+i)};return ranges;}
World* interrupted=nullptr;bmh::GameRandom* interrupted_game=nullptr;
void change_world(int){bmh::draw_hook=nullptr;++interrupted->players.begin()->second.cards;}
void change_random(int){bmh::draw_hook=nullptr;interrupted_game->range(0,1);}
}
int main()try {
    const auto master=master_world();
    const auto reference=market_reference();
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
        auto initialized=world,again=world;auto mg=game,rg=game;auto mw=random,rw=random;
        const auto market_outcome=initialize_career_markets_and_moods(initialized,reference,mg,mw);
        initialize_career_markets_and_moods(again,reference,rg,rw);
        check(initialized==again&&mg.seed()==rg.seed()&&mw.seed()==rw.seed(),"market/mood initialization changed across repeat");
        check(initialized.markets&&market_outcome.game_draws==mg.calls()-game.calls()&&market_outcome.watcom_draws==mw.calls()-random.calls(),"market initialization draw accounting differs");
        check(initialized.players==world.players&&initialized.membership==world.membership,"market initialization changed player IDs or memberships");
        auto bad_reference=reference;bad_reference.stocks.pop_back();
        auto uninitialized=world;auto unchanged_game=game;auto unchanged_random=random;
        rejects([&]{initialize_career_markets_and_moods(uninitialized,bad_reference,unchanged_game,unchanged_random);},"truncated market reference accepted");
        check(uninitialized==world&&unchanged_game.calls()==game.calls()&&unchanged_random.calls()==random.calls(),"bad market reference leaked initialization");
        for(const auto& [id,club]:initialized.clubs)check(club.morale>=20&&club.morale<=30&&(club.match_flags&0xf000)==0x5000,"initial club mood/flags differ from Source bounds");
        for(size_t i=0;i<initialized.markets->funds.size();++i)if(i%0x1c>=4)check(initialized.markets->funds[i]==reference.memory.u8(bmh::layout::FundBlock+uint32_t(i)),"initial market lost reference fund fields");
        check(decode_world(encode_world(initialized))==initialized,"market history/funds lost on world roundtrip");
        snapshot.world=initialized;snapshot.game_seed=mg.seed();snapshot.watcom_seed=mw.seed();
        check(read_save(make_save(snapshot))==snapshot,"market history/funds lost on snapshot roundtrip");
        auto encoded=encode_world(initialized);auto json=bmh::cli::parse_json(std::string(encoded.begin(),encoded.end()));
        auto malformed=json;auto short_stocks=*malformed.object()->at("markets").object()->at("stocks").array();short_stocks.pop_back();
        malformed.object()->at("markets").object()->at("stocks")=std::move(short_stocks);
        const auto invalid=bmh::cli::dump_json(malformed);
        rejects([&]{decode_world(bmh::Bytes(invalid.begin(),invalid.end()));},"truncated saved stock history accepted");
        json.object()->at("version")=9;json.object()->erase("markets");const auto old=bmh::cli::dump_json(json);
        check(!decode_world(bmh::Bytes(old.begin(),old.end())).markets,"older world invented market history");
        rejects([&]{initialize_career_markets_and_moods(initialized,reference,mg,mw);},"market initialization ran twice");
        check(initialized==again&&mg.calls()==rg.calls()&&mw.calls()==rw.calls(),"duplicate market initialization changed state");
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
    initialize_career_players(world,plan,game,random);
    const auto generated=world;const auto previous_game=game;const auto previous_random=random;
    interrupted=&world;bmh::draw_hook=change_world;
    rejects([&]{initialize_career_markets_and_moods(world,reference,game,random);},"market phase overwrote external world change");bmh::draw_hook=nullptr;
    auto generated_change=generated;++generated_change.players.begin()->second.cards;
    check(world==generated_change&&game.calls()==previous_game.calls()&&random.calls()==previous_random.calls(),"market candidate leaked on interrupted owner");
    world=generated;interrupted_game=&game;bmh::draw_hook=change_random;
    rejects([&]{initialize_career_markets_and_moods(world,reference,game,random);},"market phase overwrote external RNG change");bmh::draw_hook=nullptr;
    check(world==generated&&game.calls()==previous_game.calls()+1&&random.calls()==previous_random.calls(),"market phase lost external random draw");
    std::cout<<"Seven fresh-career player and market/mood scopes, Original ordering, stable IDs, deterministic generation, persistence and late ownership rollback passed\n";
    return 0;
}catch(const std::exception& error){bmh::draw_hook=nullptr;std::cerr<<error.what()<<'\n';return 1;}
