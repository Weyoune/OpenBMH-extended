// SPDX-License-Identifier: GPL-3.0-only
#include "world12_fixture.hpp"
#include "fixture_tables.hpp"
#include "game/world12/european.hpp"
#include "game/world12/european_campaign.hpp"
#include "game/world12/codec.hpp"
#include "game/world12/save.hpp"
#include <algorithm>
#include <iostream>
using namespace bmh;using namespace bmh::world12;
namespace {
void check(bool ok,const char* reason){if(!ok)throw std::runtime_error(reason);}
template<class F>void rejects(F f){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}check(rejected,"invalid European transition accepted");}
World fixture(){auto w=bmh::test::world12::complete_world();w.catalogue.scope.international=false;w.players_initialized=true;w.markets=CareerMarkets{};w.free_players=CareerPlayerPools{};w.settings=CareerSettings{};InitialCompetitionFields fields;auto it=w.clubs.begin();const size_t counts[]={64,24,32};for(size_t k=0;k<3;++k){for(size_t i=0;i<counts[k];++i)fields.fields[k].push_back((it++)->first);fields.holders[k]=fields.fields[k][0];}w.initial_competitions=fields;w.validate();return w;}
void programme(World world,const LeImage& source){
    world.european_campaigns=initial_european_campaigns(world,source);world.validate();WatcomRandom random(91);const auto initial=world;size_t totals[3]={};size_t dates[3]={};
    check(current_european_members((*world.european_campaigns)[1]).size()==24,"CL qualifying lost its eight seeds");
    for(size_t k=0;k<3;++k)while(!(*world.european_campaigns)[k].complete){const auto& c=(*world.european_campaigns)[k];const auto groups=prepare_european_groups(c);EuropeanResults result;result.competition=c.competition;result.revision=c.revision;
        for(const auto& group:groups){result.groups.push_back({group.source_entry,group.played,std::vector<uint16_t>(group.matches.size(),group.leg==2?uint16_t(1):uint16_t(2<<5))});totals[k]+=group.matches.size();}
        const auto before=world;const auto seed=random.seed();const auto calls=random.calls();auto broken=result;broken.groups.back().scores.pop_back();rejects([&]{commit_european_results(world,broken,source,random);});check(world==before&&random.seed()==seed&&random.calls()==calls,"late result rejection leaked a stage/draw/RNG");
        auto changed=source;const uint8_t flags=0;changed.patch(cup_entry_address(groups[0].source_entry)+0x14,&flags,1);rejects([&]{commit_european_results(world,result,changed,random);});check(world==before&&random.seed()==seed,"changed source rules leaked campaign state");
        if(groups[0].flags&8){auto tie=result;for(auto& g:tie.groups)std::fill(g.scores.begin(),g.scores.end(),groups[0].leg==2?uint16_t(2<<5):uint16_t(0));rejects([&]{commit_european_results(world,tie,source,random);});check(world==before&&random.seed()==seed,"unresolved tie leaked state");}
        commit_european_results(world,result,source,random);++dates[k];rejects([&]{commit_european_results(world,result,source,random);});
        const auto saved=world;world=decode_world(encode_world(world));check(world==saved,"campaign/source links/results lost in World35");CareerSnapshot career;career.world=world;career.results=SeasonResults::empty(world.catalogue);career.watcom_seed=random.seed();check(read_save(make_save(career))==career,"campaign state lost in typed career save");
    }
    check(totals[0]==126&&totals[1]==77&&totals[2]==61,"European full Source programme dropped a match");check(dates[0]==12&&dates[1]==13&&dates[2]==9,"European programme leg/group cursor drift");check(random.calls()==236,"European source draw count changed");
    for(size_t k=0;k<3;++k){const auto& c=(*world.european_campaigns)[k];check(c.winner.valid()&&c.runner_up.valid()&&current_european_members(c)==std::vector<ClubId>{c.winner},"European final has no canonical winner");const auto status=european_club_status(world,c.winner);check(status&&status->winner&&status->active&&!status->eliminated,"winner membership projection is stale");for(auto id:c.entrants)if(id!=c.winner){const auto eliminated=european_club_status(world,id);check(eliminated&&eliminated->entered&&eliminated->eliminated,"eliminated entrant lost its campaign ownership");}}
    (void)initial;
    std::cout<<"Source programmes: UEFA=126 CL=77 CW=61 matches; W=236; every calendar boundary saved; supplied synthetic scores, not played games\n";
}
}
int main(int argc,char** argv)try{set_fixture_tables(bmh::test::authored_fixture_tables());auto world=fixture();auto source=bmh::test::world12::country_rules(false);programme(world,source);if(argc==3){world=decode_world(read_file(argv[2]));world.players_initialized=true;world.markets=CareerMarkets{};world.free_players=CareerPlayerPools{};world.settings=CareerSettings{};GameRandom game(17);WatcomRandom random(91);world.initial_competitions=draw_initial_shared_fields(world,game,random);source=load_le(argv[1]);install_fixture_tables(source);programme(world,source);}return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
