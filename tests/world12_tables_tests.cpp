// SPDX-License-Identifier: GPL-3.0-only
#include "world12_fixture.hpp"
#include "fixture_tables.hpp"
#include "ui/world12_table.hpp"
#include "game/world12/codec.hpp"
#include "game/table.hpp"
#include "formats/executable_layout.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
using namespace bmh;using namespace bmh::world12;using namespace bmh::ui;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class F>void rejects(F action,const char* message){bool rejected=false;try{action();}catch(const std::exception&){rejected=true;}check(rejected,message);}
TextDb labels(){TextDb out;out.strings.resize(TEXT_COUNT,"Synthetic");out.strings[82]="Only01-34";for(const auto& s:out.strings){out.offsets.push_back(out.blob.size());out.blob.insert(out.blob.end(),s.begin(),s.end());out.blob.push_back(0);}return out;}
CareerSnapshot fixture(GameScope scope){CareerSnapshot out;out.world=select_world_scope(bmh::test::world12::complete_world(),scope);const auto league=out.world.catalogue.starting_league(scope.germany?Country::germany:Country::england,0);out.world.managers={{2,"Manager",out.world.membership[league.value].clubs[0],league,0}};out.results=SeasonResults::empty(out.world.catalogue);
    for(const auto& l:out.world.catalogue.leagues)if(l.active){out.league_rounds[l.id.value]=3;for(int round=1;round<=3;++round)for(int match=0;match<l.clubs/2;++match)out.results.result(out.world.catalogue,l.id,round,match)={true,uint8_t((round+match)%4),uint8_t((round*2+match)%3)};}out.validate();return out;}
void test_models(){const auto texts=labels();int compared=0;
    for(const auto scope:{GameScope{true,true,true},GameScope{true,false,true},GameScope{false,true,false}}){const auto state=fixture(scope);const auto before=state;const auto owner=World12TableIdentity{2,state.world.managers[0].club,17,42,1};auto initial=initial_world12_table_selection(state,2,0x11,1);
        check(initial.league==state.world.clubs.at(owner.club).league&&initial.aggregation==7&&initial.first_round==0&&initial.end_round==3,"table default lost current manager/rounds");
        for(int group=0;group<3;++group)for(const auto id:state.world.catalogue.table_leagues(TableGroup(group))){auto s=initial;s.group=TableGroup(group);s.league=id;const auto& league=state.world.catalogue.league(id);
            for(int control:{0,1,2,4,5,6,7,8}){auto selected=s;apply_world12_table_control(selected,control,state.world.catalogue);const auto page=world12_table_state(state,owner,selected,{},texts);
                check(page.clubs.size()==league.clubs&&page.table.rows.size()==league.clubs&&page.table.league_id==int(id.value),"typed table lost its actual league size/ID");
                check(page.groups[0]==scope.germany&&page.groups[1]==scope.england&&page.groups[2],"table groups contradict country scope");
                check(std::find(page.leagues.begin(),page.leagues.end(),id)!=page.leagues.end(),"table league not in its visible group");
                if(league.clubs==10){check(page.table.rows.size()==10&&league.rounds()==18,"English fifth tier stretched to18/20 rows");continue;}
                const auto saved_size=kLeagueSizes[0];kLeagueSizes[0]=league.clubs;
                struct Restore{uint8_t value;~Restore(){kLeagueSizes[0]=value;}} restore{saved_size};
                GameState legacy;legacy.results.assign(9*0x17c,0xcc);legacy.league_rounds.assign(9,0);legacy.league_rounds[0]=3;
                for(int round=1;round<=3;++round)for(int match=0;match<league.clubs/2;++match){const auto score=state.results.result(state.world.catalogue,id,round,match);legacy.results[size_t(round-1)*10+match]=uint8_t(score.home<<4|score.away);}
                const auto expected=compute_league_table(legacy,0,selected.first_round,selected.end_round,selected.aggregation,selected.normal_mode+14-int((selected.word>>3)&14));
                const auto& members=state.world.membership[id.value].clubs;
                for(size_t row=0;row<page.clubs.size();++row){const auto global=page.clubs[row];const auto at=std::find(members.begin(),members.end(),global)-members.begin();const auto& actual=page.table.rows[row];
                    check(global.value>65535&&at<ptrdiff_t(members.size())&&actual.rank==expected.rank(int(at))+1&&actual.previous_rank==expected.previous_rank(int(at))+1,"table rank order/global ID differs from Original");
                    check(actual.games==expected.games(int(at))&&actual.wins==expected.wins[size_t(at)]&&actual.losses==expected.defeats[size_t(at)]&&actual.points==expected.points[size_t(at)]&&actual.goals_for==expected.goals_for[size_t(at)]&&actual.goals_against==expected.goals_against[size_t(at)]&&actual.form_bits==expected.form[size_t(at)],"table counts/form/points differ from Original");
                }++compared;
            }
        }
        if(!scope.england){auto s=initial;s.group=TableGroup::europe;s.league={5};const auto page=world12_table_state(state,owner,s,{},texts);check(std::find(page.leagues.begin(),page.leagues.end(),LeagueId{5})!=page.leagues.end()&&page.table.rows.size()==20,"disabled English system lost its Premier League fallback");s.group=TableGroup::england;rejects([&]{world12_table_state(state,owner,s,{},texts);},"disabled English ladder remained selectable");}
        if(scope.england){const auto europe=state.world.catalogue.table_leagues(TableGroup::europe);check(std::find(europe.begin(),europe.end(),LeagueId{5})==europe.end(),"fully simulated England duplicated into Europe");}
        auto s=initial;rejects([&]{apply_world12_table_control(s,3,state.world.catalogue);},"forecast consumed an unowned action");rejects([&]{apply_world12_table_control(s,11,state.world.catalogue);},"unsupported family switch was accepted");auto wrong=owner;wrong.club=state.world.membership[initial.league.value].clubs[1];rejects([&]{world12_table_state(state,wrong,initial,{},texts);},"another club stole table ownership");check(state==before,"read-only table changed world/date/results/RNG");
    }std::cout<<compared<<" typed table query/control projections match Original calculations\n";
}
void codec(){auto state=fixture({});state.world.players_initialized=true;state.world.markets.emplace();state.world.free_players.emplace();state.world.settings=CareerSettings{3,3,5,1,false};state.world.managers[0].career.emplace();state.world.managers[0].career->table_display_word=0xf271;const auto bytes=encode_world(state.world);check(decode_world(bytes)==state.world,"schema33 lost table word bits");auto raw=bmh::cli::parse_json(std::string(bytes.begin(),bytes.end()));raw.object()->at("version")=bmh::cli::Json(32);auto old=bmh::cli::dump_json(raw);rejects([&]{decode_world(Bytes(old.begin(),old.end()));},"schema32 accepted a table word it did not serialize");auto& managers=std::get<bmh::cli::Json::Array>(raw.object()->at("managers").value);managers[0].object()->at("career").object()->erase("table_display_word");old=bmh::cli::dump_json(raw);check(!decode_world(Bytes(old.begin(),old.end())).managers[0].career->table_display_word,"schema32 invented table preferences");}
void family_codec(){auto state=fixture({});state.world.players_initialized=true;state.world.markets.emplace();state.world.free_players.emplace();state.world.settings=CareerSettings{3,3,5,1,false};auto& career=state.world.managers[0].career.emplace();career.strengths_display_word=0xf2ab;career.pairings_display_byte=0xe8;const auto bytes=encode_world(state.world);check(decode_world(bytes)==state.world,"schema34 lost Source family preference bits");
    auto raw=bmh::cli::parse_json(std::string(bytes.begin(),bytes.end()));raw.object()->at("version")=bmh::cli::Json(33);auto old=bmh::cli::dump_json(raw);rejects([&]{decode_world(Bytes(old.begin(),old.end()));},"older schema restored unowned family preferences");auto& managers=std::get<bmh::cli::Json::Array>(raw.object()->at("managers").value);auto* object=managers[0].object()->at("career").object();object->erase("strengths_display_word");object->erase("pairings_display_byte");old=bmh::cli::dump_json(raw);const auto decoded=decode_world(Bytes(old.begin(),old.end()));check(!decoded.managers[0].career->strengths_display_word&&!decoded.managers[0].career->pairings_display_byte,"schema33 invented Source family preferences");}

}
int main()try{set_fixture_tables(bmh::test::authored_fixture_tables());test_models();codec();family_codec();std::cout<<"Country table scopes, wide IDs, Source controls and schema33 passed\n";}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
