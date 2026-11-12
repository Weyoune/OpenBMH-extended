// SPDX-License-Identifier: GPL-3.0-only
#include "annual_accounts_fixture.hpp"
#include "fixture_tables.hpp"
#include "game/world12/annual_accounts.hpp"
#include "game/world12/manager_tactics.hpp"
#include "game/world12/codec.hpp"
#include "game/world12/season_transition.hpp"
#include "game/messages.hpp"
#include <iostream>
#include <set>
namespace {
using namespace bmh;using namespace bmh::world12;
void check(bool yes,const char* text){if(!yes)throw std::runtime_error(text);}
template<class F>void rejects(F f,const char* text){bool refused=false;try{f();}catch(const std::exception&){refused=true;}check(refused,text);}

}
int main()try {
    set_fixture_tables(bmh::test::authored_fixture_tables());const auto rules=bmh::test::world12::annual_reference();const auto text=bmh::test::world12::annual_texts();
    for(const auto scope:{GameScope{true,false,false},GameScope{false,true,false},GameScope{true,true,false}})for(bool academy:{false,true}) {
        auto source=bmh::test::world12::completed_account_closing(rules,scope,academy);
        if(academy){   // two rounds of 1 in 4: take the first game seed with a recruit
            bool found=false;
            for(uint32_t probe=0;probe<400&&!found;++probe){
                auto trial=source;trial.game_seed+=probe;
                AnnualAccountsHooks quiet;quiet.present=[](const AnnualTaxEvent&){return true;};quiet.choose=[](const AnnualTaxEvent&)->std::optional<bool>{return false;};
                found=!stage_annual_accounts(stage_annual_qualification(trial),rules,text,quiet).academy.empty();
                if(found)source=trial;
            }
            check(found,"no game seed lets the academy recruit");
        }
        const auto qualification=stage_annual_qualification(source);const auto before=qualification;
        AnnualAccountsHooks hooks;size_t presented=0,chosen=0;
        hooks.present=[&](const AnnualTaxEvent& e){++presented;check(e.manager==3&&e.club==source.world.managers[0].club&&e.source.manager_slot==3&&e.source.team==-1,"annual tax presenter lost canonical sparse owner");return true;};
        hooks.choose=[&](const AnnualTaxEvent&)->std::optional<bool>{++chosen;return false;};
        const auto result=stage_annual_accounts(qualification,rules,text,hooks);
        check(qualification==before&&result.snapshot.date==source.date&&result.snapshot.season==source.season&&result.snapshot.calendar==source.calendar&&result.snapshot.results==source.results&&result.snapshot.league_rounds==source.league_rounds,"unpublished accounts changed owner/date/calendar/standings");
        check(presented==1&&chosen==1&&result.tax_events.size()==1&&result.tax_decisions.size()==1&&result.game_draws>=1&&result.watcom_draws>=1,"actual tax and annual draws were skipped");
        check(academy?(result.academy.size()>=1&&result.academy.size()<=2):result.academy.empty(),"annual academy recruited without an active academy or more than its two rounds allow");
        if(academy){const auto& recruit=result.academy.front();check(recruit.player.valid()&&recruit.donor!=recruit.recipient&&recruit.manager==3&&recruit.recipient==source.world.managers[0].club,"academy lost real donor/player identity");
            const auto& donor=result.snapshot.world.clubs.at(recruit.donor).roster;const auto& recipient=result.snapshot.world.clubs.at(recruit.recipient).roster;
            check(std::none_of(donor.begin(),donor.end(),[&](const auto& r){return r.player==recruit.player;})&&std::count_if(recipient.begin(),recipient.end(),[&](const auto& r){return r.player==recruit.player;})==1&&result.snapshot.world.players.size()==source.world.players.size(),"academy invented, duplicated or failed to move actual donor");}
        if(academy){
            auto promoted=result.snapshot;
            for(auto country:{Country::germany,Country::england})if(promoted.world.catalogue.selectable(country)){
                const AnnualDecisionContext context{{country,(*promoted.world.amateur_slots)[country==Country::germany?0:1]},
                    country==Country::germany?RegionalPromotion::champions_only:RegionalPromotion::not_applicable,{}};
                const auto decisions=evaluate_annual_promotion_decisions(promoted,context);const auto stage=stage_annual_membership(promoted,decisions,text);
                promoted.world=stage.world;promoted.game_seed=stage.game_seed;promoted.watcom_seed=stage.watcom_seed;
            }
            const auto& recruit=result.academy.front();size_t occurrences=0;
            for(const auto& [id,club]:promoted.world.clubs)for(const auto& slot:club.roster)occurrences+=slot.player==recruit.player;
            check(occurrences==1&&promoted.world.players.size()==source.world.players.size(),"academy identity broke during real annual promotion roster inheritance");
            check(read_save(make_save(promoted))==promoted,"post-academy promotion lineage failed snapshot roundtrip");
        }
        validate_annual_accounts(result,rules,text);check(read_save(make_save(result.snapshot))==result.snapshot,"account canonical backing failed save roundtrip");
        auto tampered=result;++tampered.carried_fine;rejects([&]{validate_annual_accounts(tampered,rules,text);},"altered Source fine carry accepted");
        rejects([&]{stage_annual_accounts(qualification,rules,text,{});},"tax branch silently chose without presenter");
        hooks.present=[](const auto&){return true;};hooks.choose=[](const auto&)->std::optional<bool>{return {};};
        rejects([&]{stage_annual_accounts(qualification,rules,text,hooks);},"cancelled tax choice accepted");
        auto unknown=source;unknown.world.managers[0].career->annual_records.reset();const auto old=stage_annual_qualification(unknown);
        rejects([&]{stage_annual_accounts(old,rules,text,hooks);},"unknown imported tax carry invented");
        check(qualification==before,"failed annual account stage changed its source binding");
    }
    std::cout<<"Annual accounts/tax/academy: actual country donor lineage, canonical choices, replay, unknown backing and rollback passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
