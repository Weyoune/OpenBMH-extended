// SPDX-License-Identifier: GPL-3.0-only
#include "annual_accounts_fixture.hpp"
#include "fixture_tables.hpp"
#include "game/world12/annual_fields.hpp"
#include "game/world12/european.hpp"
#include "game/market_rules.hpp"
#include <iostream>
namespace {
using namespace bmh;using namespace bmh::world12;
void check(bool yes,const char* text){if(!yes)throw std::runtime_error(text);}
template<class F>void rejects(F f,const char* text){bool refused=false;try{f();}catch(const std::exception&){refused=true;}check(refused,text);}
CareerSnapshot* disturbed=nullptr;void interrupt(int){draw_hook=nullptr;++disturbed->game_seed;}
}
int main()try {
    set_fixture_tables(bmh::test::authored_fixture_tables());const auto reference=bmh::test::world12::annual_reference();const auto texts=bmh::test::world12::annual_texts();
    const auto de=bmh::test::world12::country_rules(false),en=bmh::test::world12::country_rules(true);
    for(auto scope:{GameScope{true,false,false},GameScope{false,true,false},GameScope{true,true,false}}) {
        const auto closing=bmh::test::world12::completed_account_closing(reference,scope,false);const auto qualification=stage_annual_qualification(closing);
        AnnualAccountsHooks hooks;hooks.present=[](const auto&){return true;};hooks.choose=[](const auto&)->std::optional<bool>{return false;};
        const auto accounts=stage_annual_accounts(qualification,reference,texts,hooks);auto source=accounts.snapshot;std::vector<AnnualPromotionOutcome> receipts;
        for(auto country:{Country::germany,Country::england})if(source.world.catalogue.selectable(country)) {
            const AnnualDecisionContext context{{country,(*source.world.amateur_slots)[country==Country::germany?0:1]},country==Country::germany?RegionalPromotion::champions_only:RegionalPromotion::not_applicable,{}};
            const auto decision=evaluate_annual_promotion_decisions(source,context);const auto stage=stage_annual_membership(source,decision,texts);receipts.push_back(decision);source.world=stage.world;source.game_seed=stage.game_seed;source.watcom_seed=stage.watcom_seed;
        }
        const auto before=source;const auto fields=stage_annual_fields(source,qualification,de,en);const auto again=stage_annual_fields(source,qualification,de,en);
        check(source==before&&fields.world==again.world&&fields.cups==again.cups&&fields.game_seed==again.game_seed&&fields.watcom_seed==again.watcom_seed,"annual fields changed source or rerolled on repeat");
        const auto& installed=*fields.world.initial_competitions;
        check(installed.qualified==qualification.next_european.qualified&&installed.fields==qualification.next_european.fields&&installed.holders==qualification.next_european.holders&&fields.world.european_campaigns.has_value(),"annual reset reused initial/year-zero European entrants");
        for(const auto& campaign:*fields.world.european_campaigns)check(!campaign.complete&&campaign.stages.size()>0&&!campaign.winner.valid(),"new actual European programme retained old results/awards");
        WatcomRandom oracle(source.watcom_seed);auto expected=source.world.membership;
        for(auto group:{TableGroup::germany,TableGroup::england,TableGroup::europe})for(auto league:source.world.catalogue.simulation_leagues(group)){auto& clubs=expected[league.value].clubs;shuffle_initial_positions(0,int(clubs.size()),oracle,[&](int a,int b){std::swap(clubs[size_t(a)],clubs[size_t(b)]);});}
        check(fields.world.membership==expected&&fields.watcom_seed==oracle.seed()&&fields.watcom_draws==oracle.calls(),"annual fixture order/Watcom stream differs from Source position-swap loop");
        const auto ranks=annual_player_ranks(fields.world,qualification,receipts);check(ranks.size()>=fields.world.catalogue.league_clubs(),"new fixture order lost actual completed position history");
        for(const auto& cup:fields.cups){check(!cup.complete()&&cup.rounds.size()==1&&cup.rounds[0].first_leg.empty(),"annual domestic programme retained old scores");if(cup.cup==DomesticCup::german_supercup||cup.cup==DomesticCup::english_supercup){const auto country=cup_country(cup.cup);const auto titles=std::find_if(qualification.domestic_titles.begin(),qualification.domestic_titles.end(),[&](const auto& t){return t.country==country;});check(cup.next_fixtures()==std::vector<bmh::world12::Fixture>{{titles->champion,titles->cup.winner}},"Super-Cup ignored actual completed year titles");}}
        auto snapshot=source;snapshot.world=fields.world;snapshot.cups=fields.cups;snapshot.game_seed=fields.game_seed;snapshot.watcom_seed=fields.watcom_seed;check(read_save(make_save(snapshot))==snapshot,"annual actual fields/fixture order failed checkpoint roundtrip");
        auto changed=source;disturbed=&changed;draw_hook=interrupt;rejects([&]{stage_annual_fields(changed,qualification,de,en);},"annual draw overwrote an external source change");draw_hook=nullptr;auto external=before;++external.game_seed;check(changed==external,"annual field failure leaked staged draws or rewound external input");
    }
    std::cout<<"Annual actual next fields: completed titles/qualifiers, Source membership shuffle/RNG, programme reset, history, snapshot and late-owner rollback passed (unpublished)\n";return 0;
}catch(const std::exception& error){draw_hook=nullptr;std::cerr<<error.what()<<'\n';return 1;}
