// SPDX-License-Identifier: GPL-3.0-only
#include "annual_accounts_fixture.hpp"
#include "fixture_tables.hpp"
#include "game/world12/annual_archive.hpp"
#include "game/world12/codec.hpp"
#include "game/world12/manager_creation.hpp"
#include <iostream>
namespace {
using namespace bmh;using namespace bmh::world12;
void check(bool yes,const char* text){if(!yes)throw std::runtime_error(text);}
template<class F>void rejects(F f,const char* text){bool refused=false;try{f();}catch(const std::exception&){refused=true;}check(refused,text);}
void source_origin(const GameState& reference) {
    auto source=reference;source.memory.add_block(layout::EnvironmentYouthTeams,4*0x24);source.memory.add_block(layout::Spending,4*0x24);
    for(size_t i=0;i<10;++i)source.manager_records[3*0xb0c+0x8b6+i]=255;
    const auto at=size_t(3)*0xb0c;source.manager_records[at+0x2a9]=0x71;source.manager_records[at+0x2aa]=0x34;source.manager_records[at+0x2ab]=0x12;source.manager_records[at+0x2ac]=0x72;
    auto captured=manager_career_state_from_legacy(source,3);check(captured.first_recorded_season==0x1234,"Source history origin2aa was not captured");
    auto unknown=captured;unknown.first_recorded_season.reset();check(!manager_career_state_from_legacy(source,3,{},false,&unknown).first_recorded_season,"ordinary projection invented unknown old history origin");
    captured.first_recorded_season=0xbeef;install_manager_career_state(source,3,captured);
    check(source.manager_records[at+0x2aa]==0xef&&source.manager_records[at+0x2ab]==0xbe&&source.manager_records[at+0x2a9]==0x71&&source.manager_records[at+0x2ac]==0x72,"history origin install changed neighbours or endianness");
}
}
int main()try {
    set_fixture_tables(bmh::test::authored_fixture_tables());const auto rules=bmh::test::world12::annual_reference();source_origin(rules);
    auto source=bmh::test::world12::completed_account_closing(rules,{false,true,false},false);const auto before=source;
    const auto facts=completed_annual_titles(source);const auto unknown=stage_annual_title_archive(source);
    check(source==before&&unknown.earlier_history_unknown&&unknown.first_retained_season==source.season&&unknown.seasons.size()==1,"unpublished archive lost old unknown history or changed closing state");
    auto fresh=source;fresh.world.annual_title_archive.emplace();const auto archive=stage_annual_title_archive(fresh);
    check(!archive.earlier_history_unknown&&archive.first_retained_season==0,"known fresh archive lost provenance");
    size_t leagues=0,domestic=0,european=0;
    for(const auto& row:archive.seasons[0].titles) {
        if(row.kind==AnnualTitleKind::league){++leagues;check(row.winner==facts.final_rankings[row.league.value][0]&&row.runner_up==facts.final_rankings[row.league.value][1],"archive league identities differ from actual final standings");}
        if(row.kind==AnnualTitleKind::domestic)++domestic;
        if(row.kind==AnnualTitleKind::european){++european;check(row.winner==facts.european_titles[row.competition-1].winner&&row.runner_up==facts.european_titles[row.competition-1].runner_up,"archive European identities differ from actual final scores");}
    }
    check(leagues==8&&domestic==3&&european==3,"archive omitted internal/foreign league or actual cup results");
    auto world=source.world;world.annual_title_archive=archive;world.managers[0].career->first_recorded_season=0x1234;
    check(decode_world(encode_world(world))==world,"schema40 archive/history origin failed exact codec roundtrip");
    auto premature=source;premature.world=world;rejects([&]{premature.validate();},"closing phase published its current title row before season advancement");
    auto retained=premature;retained.season=1;check(read_save(make_save(retained))==retained,"published archive prefix failed snapshot roundtrip");
    const auto winner=archive.seasons[0].titles.front().winner;world.clubs.at(winner).name="Renamed after title";
    std::swap(world.membership[5].clubs[0],world.membership[5].clubs[1]);world.validate();
    check(world.annual_title_archive->seasons[0].titles.front().winner==winner,"archived winner followed current name/rank instead of global identity");
    auto broken=world;broken.annual_title_archive->seasons.push_back(archive.seasons[0]);rejects([&]{broken.validate();},"duplicate archived season accepted");
    broken=world;broken.annual_title_archive->seasons[0].titles[0].runner_up=broken.annual_title_archive->seasons[0].titles[0].winner;rejects([&]{broken.validate();},"title winner duplicated as runner-up");
    broken=world;broken.annual_title_archive->seasons[0].titles.push_back(broken.annual_title_archive->seasons[0].titles[0]);rejects([&]{broken.validate();},"duplicate title competition accepted");
    auto older=source;TitleHistoryArchive labels;labels[0][0]=std::string("Ambiguous name\0",15)+char(0xff);labels[0][4]="Unresolved label";
    retain_unresolved_source_titles(older,labels);check(older.world.annual_title_archive->earlier_history_unknown&&older.world.annual_title_archive->seasons.empty()&&older.world.annual_title_archive->unresolved_source_labels==labels,"Source labels were resolved by name or dropped");
    check(read_save(make_save(older))==older,"unresolved Original label bytes were not preserved");
    const auto older_before=older;labels[0][0]="Conflicting label";rejects([&]{retain_unresolved_source_titles(older,labels);},"conflicting Source labels replaced known bytes");check(older==older_before,"failed label migration leaked partial state");
    const auto migrated=stage_annual_title_archive(older);check(migrated.unresolved_source_labels==older.world.annual_title_archive->unresolved_source_labels&&migrated.earlier_history_unknown,"actual title append discarded unresolved historical labels");
    const auto encoded=encode_world(world);auto json=cli::parse_json(std::string(encoded.begin(),encoded.end()));json.object()->at("version")=39;
    auto data=cli::dump_json(json);rejects([&]{decode_world(Bytes(data.begin(),data.end()));},"schema39 silently accepted known40 archive");
    json.object()->erase("annual_title_archive");data=cli::dump_json(json);rejects([&]{decode_world(Bytes(data.begin(),data.end()));},"schema39 silently accepted known40 history origin");
    auto& managers=std::get<cli::Json::Array>(json.object()->at("managers").value);managers[0].object()->at("career").object()->erase("first_recorded_season");data=cli::dump_json(json);const auto old=decode_world(Bytes(data.begin(),data.end()));check(!old.annual_title_archive&&!old.managers[0].career->first_recorded_season,"older schema invented retained IDs or Source history origin");
    const auto german=bmh::test::world12::completed_account_closing(rules,{true,false,false},false);rejects([&]{stage_annual_title_archive(german);},"missing real indoor Masters replaced with a fabricated title");
    std::cout<<"Annual schema40 archive: actual standings/cup finals, stable IDs, unknown prior history/Source label bytes, published prefix, origin word and strict migration passed (not a season publication)\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
