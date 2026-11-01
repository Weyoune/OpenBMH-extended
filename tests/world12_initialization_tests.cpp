// SPDX-License-Identifier: GPL-3.0-only
#include "world12_fixture.hpp"
#include "game/world12/initialization.hpp"
#include "game/world12/codec.hpp"
#include "game/world12/save.hpp"
#include "game/state.hpp"
#include "cli/json.hpp"
#include "app/world12_manager.hpp"
#include <iostream>
#include <stdexcept>
#include <algorithm>
namespace {
using namespace bmh::world12;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class Function> void rejects(Function function,const char* message){bool rejected=false;try{function();}catch(const std::exception&){rejected=true;}check(rejected,message);}
World master_world() {
    auto world=bmh::test::world12::complete_world();
    for(uint32_t i=0;i<54;++i){PlayerRecord p;p.id={1000000+i};p.attributes={"Extra",50u|(60u<<7)|(65u<<14),(i<50?5u:0u)<<27,i%4};world.players[p.id]=p;}
    for(uint32_t i=0;i<5;++i){ClubRecord club;club.id={3000000+i};club.name="Extra club";club.crest={club.id.value};
        club.country=i==0||i==3?Country::germany:i==1||i==4?Country::england:Country::portugal;
        club.kind=i>=3?ClubKind::national_team:ClubKind::club;club.rating=uint8_t(i*51);
        world.crests[club.crest]={"W/TEST.VGA",0};world.clubs[club.id]=club;}
    return world;
}
bmh::GameState market_reference() {
    bmh::GameState result;result.stocks.resize(12*0x5c);
    result.formations.resize(0x160);
    const uint8_t depths[]={0,5,5,5,5,10,10,10,10,17,17};
    for(size_t f=0;f<4;++f)for(size_t r=0;r<11;++r)result.formations[f*22+r]=depths[r];
    result.memory.add_block(bmh::layout::FundBlock,6*0x1c);
    // Retain the non-base fund fields supplied by the reference.
    for(uint32_t i=0;i<6*0x1c;++i)result.memory.set8(bmh::layout::FundBlock+i,uint8_t(i*7));
    return result;
}
InitialStrengthRanges ranges() {InitialStrengthRanges ranges;for(size_t i=0;i<ranges.size();++i)ranges[i]={uint8_t(20+i),uint8_t(60+i)};return ranges;}
World* interrupted=nullptr;bmh::GameRandom* interrupted_game=nullptr;
bmh::app::world12::ManagerActionOwner* interrupted_owner=nullptr;
void change_world(int){bmh::draw_hook=nullptr;++interrupted->players.begin()->second.cards;}
void change_random(int){bmh::draw_hook=nullptr;interrupted_game->range(0,1);}
void change_owner(int){bmh::draw_hook=nullptr;++interrupted_owner->revision;}
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
        if(scope.club_play()) {
            const auto market_world=initialized;
            const auto pool_draws=initialize_career_player_pools(initialized,plan,reference,bool(mask&1),mg,mw);
            initialize_career_player_pools(again,plan,reference,bool(mask&1),rg,rw);
            check(initialized==again&&mg.seed()==rg.seed()&&mw.seed()==rw.seed(),"free-player assignment changed across repeat");
            check(initialized.free_players&&pool_draws.players==initialized.players.size(),"free-player phase omitted initial players");
            size_t free=0,rostered=0;
            for(auto id:initialized.free_players->domestic)free+=id.valid();for(auto id:initialized.free_players->foreign)free+=id.valid();
            for(const auto& [id,club]:initialized.clubs)for(const auto& row:club.roster)rostered+=!row.empty();
            check(free+rostered==initialized.players.size()&&free<54,"overflow was lost or hidden in an unlimited pool");
            check(decode_world(encode_world(initialized))==initialized,"free-player identities lost on world roundtrip");
            snapshot.world=initialized;snapshot.game_seed=mg.seed();snapshot.watcom_seed=mw.seed();
            check(read_save(make_save(snapshot))==snapshot,"free-player state lost on snapshot roundtrip");
            auto duplicate=initialized;duplicate.free_players->foreign[1]=duplicate.free_players->foreign[0];
            rejects([&]{duplicate.validate();},"duplicated free player accepted");
            auto gap=initialized;gap.free_players->foreign[0]={};rejects([&]{gap.validate();},"noncompact free pool accepted");
            const auto ready=initialized;const auto pg=mg;const auto pw=mw;
            rejects([&]{initialize_career_player_pools(initialized,plan,reference,false,mg,mw);},"initial free players released twice");
            check(initialized==ready&&mg.calls()==pg.calls()&&mw.calls()==pw.calls(),"duplicate release mutated world or seeds");
            auto changed_plan=plan;std::swap(changed_plan.groups[0],changed_plan.groups[1]);auto rejected=market_world;
            rejects([&]{initialize_career_player_pools(rejected,changed_plan,reference,false,mg,mw);},"reordered age/release plan accepted");
            auto whole=select_world_scope(master,scope);bmh::GameRandom wg(17);bmh::WatcomRandom ww(91);
            bmh::app::world12::ManagerActionOwner owner{881,7};
            bmh::app::world12::initialize_club_player_state(whole,owner,{881,7},plan,reference,bool(mask&1),wg,ww);
            check(whole==initialized&&wg.seed()==mg.seed()&&ww.seed()==mw.seed()&&owner.revision==8,"combined player creation did not publish one equivalent revision");
            auto fresh=select_world_scope(master,scope);auto broken=reference;broken.formations.clear();auto fg=bmh::GameRandom(17);auto fw=bmh::WatcomRandom(91);owner={881,7};
            rejects([&]{bmh::app::world12::initialize_club_player_state(fresh,owner,{881,7},plan,broken,false,fg,fw);},"failed final creation phase accepted");
            check(fresh==select_world_scope(master,scope)&&fg.calls()==0&&fw.calls()==0&&owner.revision==7,"combined creation leaked an earlier phase on final failure");
            rejects([&]{bmh::app::world12::initialize_club_player_state(fresh,owner,{881,6},plan,reference,false,fg,fw);},"stale pre-manager revision accepted");
            interrupted_owner=&owner;bmh::draw_hook=change_owner;
            rejects([&]{bmh::app::world12::initialize_club_player_state(fresh,owner,{881,7},plan,reference,false,fg,fw);},"changed pre-manager owner overwritten");bmh::draw_hook=nullptr;
            check(fresh==select_world_scope(master,scope)&&fg.calls()==0&&fw.calls()==0&&owner.revision==8,"combined creation lost external owner revision or leaked state");
        }
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
    initialize_career_markets_and_moods(world,reference,game,random);
    const auto market=world;const auto market_game=game;const auto market_random=random;
    interrupted=&world;bmh::draw_hook=change_world;
    rejects([&]{initialize_career_player_pools(world,plan,reference,false,game,random);},"release phase overwrote external world change");bmh::draw_hook=nullptr;
    auto market_change=market;++market_change.players.begin()->second.cards;
    check(world==market_change&&game.calls()==market_game.calls()&&random.calls()==market_random.calls(),"release candidate leaked on interrupted owner");
    world=market;interrupted_game=&game;bmh::draw_hook=change_random;
    rejects([&]{initialize_career_player_pools(world,plan,reference,false,game,random);},"release phase overwrote external RNG change");bmh::draw_hook=nullptr;
    check(world==market&&game.calls()==market_game.calls()+1&&random.calls()==market_random.calls(),"release phase lost external random draw");
    for(bool england_only:{false,true}) {
        auto large=master_world();const auto target=large.membership[5].clubs[0];uint32_t player_id=5000000;
        for(auto& [id,club]:large.clubs)if(club.league.valid())for(size_t row=11;row<size_t(id==target?20:22);++row) {
            PlayerRecord p;p.id={player_id++};p.attributes={"Large reserve",55u|(60u<<7)|(70u<<14),uint32_t(club.country)<<27,uint32_t(row%4)};
            large.players[p.id]=p;club.roster[row]={p.id,uint16_t(row<17?2:0)};
        }
        for(uint32_t i=0;i<54;++i)large.players.at({1000000+i}).attributes.w368=(i<21?(england_only?0u:5u):(england_only?2u:0u))<<27;
        const GameScope selected{!england_only,true,true};const auto initial=make_player_initialization_plan(large,selected,ranges());
        auto full=select_world_scope(large,selected);bmh::GameRandom fg(17);bmh::WatcomRandom fw(91);
        bmh::app::world12::ManagerActionOwner owner{991,0};
        bmh::app::world12::initialize_club_player_state(full,owner,{991,0},initial,reference,false,fg,fw);
        check(std::count_if(full.clubs.at(target).roster.begin(),full.clubs.at(target).roster.end(),[](const auto& row){return !row.empty();})==21,"single eligible overflow club was not selected (including second-pass retry)");
        check(full.free_players->foreign.back().valid()&&full.free_players->domestic[32].valid()&&!full.free_players->domestic[33].valid(),"single overflow changed bounded pool capacities");
        if(!england_only)check(full.players.size()>4095,"wide-player transfer fixture fell below legacy packed limit");
        // Fill the only eligible club before rebuilding the immutable plan.
        auto blocked_master=large;
        for(size_t row=20;row<22;++row){PlayerRecord p;p.id={player_id++};p.attributes={"No room",55u|(60u<<7)|(70u<<14),0,1};blocked_master.players[p.id]=p;blocked_master.clubs.at(target).roster[row]={p.id,0};}
        const auto blocked_plan=make_player_initialization_plan(blocked_master,selected,ranges());
        auto blocked=select_world_scope(blocked_master,selected);const auto blocked_before=blocked;
        bmh::GameRandom bg(17);bmh::WatcomRandom bw(91);owner={992,0};
        rejects([&]{bmh::app::world12::initialize_club_player_state(blocked,owner,{992,0},blocked_plan,reference,false,bg,bw);},"full clubs accepted a pool-overflow player");
        check(blocked==blocked_before&&bg.calls()==0&&bw.calls()==0&&owner.revision==0,"overflow failure leaked any earlier career phase");
    }
    std::cout<<"Seven player/market scopes, six bounded release scopes, wide-ID transfers, second-pass overflow, persistence and atomic creation rollback passed\n";
    return 0;
}catch(const std::exception& error){bmh::draw_hook=nullptr;std::cerr<<error.what()<<'\n';return 1;}
