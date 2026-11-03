// SPDX-License-Identifier: GPL-3.0-only
#include "annual_accounts_fixture.hpp"
#include "fixture_tables.hpp"
#include "game/world12/annual_moods.hpp"
#include "game/world12/annual_accounts.hpp"
#include "game/season.hpp"
#include <iostream>
#include <set>
namespace {
using namespace bmh;using namespace bmh::world12;
void check(bool yes,const char* text){if(!yes)throw std::runtime_error(text);}
template<class F>void rejects(F f,const char* text){bool refused=false;try{f();}catch(const std::exception&){refused=true;}check(refused,text);}
CareerSnapshot* changed=nullptr;
void disturb(int){draw_hook=nullptr;++changed->game_seed;}
}
int main()try {
    set_fixture_tables(bmh::test::authored_fixture_tables());const auto rules=bmh::test::world12::annual_reference();const auto text=bmh::test::world12::annual_texts();
    for(const auto scope:{GameScope{true,false,false},GameScope{false,true,false},GameScope{true,true,false}}) {
        auto source=bmh::test::world12::completed_account_closing(rules,scope,true);const auto before=source;
        const auto facts=completed_annual_titles(source);const auto stage=stage_annual_moods(source,rules);
        check(source==before&&stage.snapshot.date==source.date&&stage.snapshot.season==source.season&&stage.snapshot.calendar==source.calendar&&stage.snapshot.results==source.results&&stage.snapshot.world.players==source.world.players&&stage.snapshot.world.managers==source.world.managers,"annual moods changed closing owners, rosters or accounts");
        check(stage.physical_order.size()==source.world.clubs.size()&&stage.game_draws==stage.physical_order.size()&&stage.watcom_draws>stage.game_draws,"annual moods skipped real club identities or source draws");
        std::set<ClubId> holders;for(const auto& row:facts.domestic_titles)holders.insert(row.cup.winner);for(const auto& row:facts.european_titles)holders.insert(row.winner);
        for(auto id:holders)check((stage.snapshot.world.clubs.at(id).match_flags&0xf000)==0xc000,"actual completed cup holder lost Source top mood class");
        validate_annual_moods(stage,rules);check(read_save(make_save(stage.snapshot))==stage.snapshot,"annual mood state lost on save roundtrip");
        const auto qualification=stage_annual_qualification(stage.snapshot);
        check(qualification.game_seed_before==stage.snapshot.game_seed&&qualification.watcom_seed_before==stage.snapshot.watcom_seed&&qualification.final_rankings==facts.final_rankings,"qualification bypassed mood seeds or altered completed rankings");
        AnnualAccountsHooks hooks;hooks.present=[](const auto&){return true;};hooks.choose=[](const auto&)->std::optional<bool>{return false;};
        const auto accounts=stage_annual_accounts(qualification,rules,text,hooks);validate_annual_accounts(accounts,rules,text);
        check(accounts.academy.size()==1,"Source pre-qualification mood order skipped later actual academy recruitment");
        auto unknown=source;unknown.world.amateur_slots.reset();rejects([&]{stage_annual_moods(unknown,rules);},"annual moods sorted unknown old amateur history into fake physical order");
        auto altered=stage;std::swap(altered.physical_order[0],altered.physical_order[1]);rejects([&]{validate_annual_moods(altered,rules);},"altered annual physical order accepted");
        changed=&source;draw_hook=disturb;rejects([&]{stage_annual_moods(source,rules);},"late Source mood owner change overwritten");draw_hook=nullptr;
        auto expected=before;++expected.game_seed;check(source==expected,"failed mood stage leaked state or rewound external owner");
    }
    std::cout<<"Annual Source moods: real physical order, completed holders, qualification/account RNG continuation, unknown history and stale rollback passed\n";return 0;
}catch(const std::exception& error){draw_hook=nullptr;std::cerr<<error.what()<<'\n';return 1;}
