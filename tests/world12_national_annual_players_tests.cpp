// SPDX-License-Identifier: GPL-3.0-only
#include "inactive_club_state_fixture.hpp"
#include "annual_accounts_fixture.hpp"
#include "game/world12/national_annual_players.hpp"
#include "game/world12/codec.hpp"
#include <iostream>
#include <set>
namespace {
using namespace bmh;using namespace bmh::world12;
void check(bool yes,const char* text){if(!yes)throw std::runtime_error(text);}
template<class F>void rejects(F f,const char* text){bool refused=false;try{f();}catch(const std::exception&){refused=true;}check(refused,text);}
CareerSnapshot* interrupted=nullptr;void interrupt(int){draw_hook=nullptr;++interrupted->watcom_seed;}
}
int main()try {
    const auto master=bmh::test::world12::inactive_master_fixture();const auto reference=bmh::test::world12::inactive_reference_fixture();const auto texts=bmh::test::world12::annual_texts();
    for(auto primary:{Country::germany,Country::england})for(bool retire:{false,true}){
        GameRandom game(17);WatcomRandom random(91);CareerSnapshot source;source.world=bmh::test::world12::inactive_national_fixture(master,reference,primary,game,random);source.world.economy.emplace();source.world.economy->week=3;source.world.economy->presentation_options.emplace();source.results=SeasonResults::empty(source.world.catalogue);source.date=365;source.game_seed=game.seed();source.watcom_seed=random.seed();const auto original_club=source.world.national_origins->front().club.id;const auto player=source.world.national_origins->front().club.roster[0].player;
        if(retire)source.world.players.at(player).attributes.w35c|=31u<<27;source.validate();const auto before=source;
        const auto stage=stage_national_annual_players(source,1,reference,texts);const auto again=stage_national_annual_players(source,1,reference,texts);
        check(source==before&&stage.world==again.world&&stage.game_seed==again.game_seed&&stage.watcom_seed==again.watcom_seed&&stage.game_draws==again.game_draws&&stage.watcom_draws==again.watcom_draws,"actual national annual population renewal was non-deterministic or changed its source");
        check(stage.visited==source.world.players.size(),("actual origin/pool visit count differs: "+std::to_string(stage.visited)+" vs "+std::to_string(source.world.players.size())).c_str());check(!retire||stage.released>=1,"forced actual first-slot injury did not retire through the Source kernel");
        check(stage.world.catalogue==source.world.catalogue&&stage.world.membership==source.world.membership&&stage.world.clubs==source.world.clubs&&stage.world.managers==source.world.managers&&stage.world.national_coaches==source.world.national_coaches&&stage.world.national_squads==source.world.national_squads&&!stage.world.transfer_market&&stage.world.economy==source.world.economy,"national renewal invented a club actor/finance/fixture/nominee or reset the real coach account");
        size_t young=0;for(const auto& [id,person]:before.world.players)if((!retire||id!=player)&&person.attributes.age()<=31&&person.attributes.w35c>>27!=31){++young;check(stage.world.players.at(id).attributes.age()==person.attributes.age()+1,"retained young Source player did not age exactly once");}check(young>0,"actual Source fixture contains no young non-retiring player");
        for(size_t i=0;i<source.world.national_origins->size();++i){const auto& old=source.world.national_origins->at(i);const auto& now=stage.world.national_origins->at(i);check(old.club.id==now.club.id&&old.club.league==now.club.league&&old.rank==now.rank&&old.source_initialized==now.source_initialized,"actual national origin rank/identity/initialization was relabelled");}
        std::set<PlayerId> owned;for(const auto& origin:*stage.world.national_origins)for(const auto& row:origin.club.roster)if(!row.empty())check(owned.insert(row.player).second,"national retirement duplicated a club-owned identity");for(auto id:stage.world.free_players->domestic)if(id.valid())check(owned.insert(id).second,"national retirement duplicated a domestic pool identity");for(auto id:stage.world.free_players->foreign)if(id.valid())check(owned.insert(id).second,"national retirement duplicated a foreign pool identity");check(owned.size()==stage.visited,"national retirement dropped a real player identity");
        if(retire){const auto& person=stage.world.players.at(player).attributes;check(person.age()>=20&&person.age()<=31,"actual national retirement did not regenerate the same identity through the shared Source kernel");const auto origin=std::find_if(stage.world.national_origins->begin(),stage.world.national_origins->end(),[&](const auto& c){return c.club.id==original_club;});check(std::none_of(origin->club.roster.begin(),origin->club.roster.end(),[&](const auto& r){return r.player==player;}),"released national origin kept its retired real player");}
        auto checkpoint=source;checkpoint.world=stage.world;checkpoint.game_seed=stage.game_seed;checkpoint.watcom_seed=stage.watcom_seed;check(read_save(make_save(checkpoint))==checkpoint,"national annual source pools/lineages failed save continuation");
        if(stage.game_draws||stage.watcom_draws){auto changed=source;interrupted=&changed;draw_hook=interrupt;rejects([&]{stage_national_annual_players(changed,1,reference,texts);},"national renewal overwrote changed external owner");draw_hook=nullptr;auto external=source;++external.watcom_seed;check(changed==external,"national renewal leaked candidate or rewound external input");}else check(stage.game_seed==source.game_seed&&stage.watcom_seed==source.watcom_seed,"draw-free actual ageing changed Source random seeds");
    }
    std::cout<<"National-only actual full-origin/free-pool annual ageing, shared Source retirement, exact-once IDs, retained coaches/nominees, codec and late-owner rollback passed (not a played season)\n";return 0;
}catch(const std::exception& error){draw_hook=nullptr;std::cerr<<error.what()<<'\n';return 1;}
