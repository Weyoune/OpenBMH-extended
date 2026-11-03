// SPDX-License-Identifier: GPL-3.0-only
#include "annual_qualification_fixture.hpp"
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
GameState reference() {
    GameState g;g.club_ids.resize(445);g.team_flags.resize(445);g.team_morale.resize(445);g.formations.resize(0x580);
    g.manager_records.resize(4*0xb0c);g.manager_ratings.resize(4*60);g.tactics.resize(4*0x840);g.manager_options.resize(20);
    g.memory.add_block(layout::Loans,4*0x78);g.mailbox.assign(kMailboxEntries*kMailboxEntry,0);
    const uint8_t depth[]={0,5,5,5,5,10,10,10,10,17,17};
    for(size_t m=0;m<4;++m){g.manager_options[m*5]=1;for(size_t mode=0;mode<2;++mode)for(size_t formation=0;formation<8;++formation)for(size_t r=0;r<11;++r){const auto at=m*0x160+mode*0xb0+formation*22+r;g.formations[at]=depth[r];g.formations[at+11]=uint8_t(r%9);}}
    return g;
}
TextDb texts(){TextDb t;for(size_t i=0;i<TEXT_COUNT;++i){t.strings.push_back("Annual "+std::to_string(i));t.offsets.push_back(t.blob.size());const auto& s=t.strings.back();t.blob.insert(t.blob.end(),s.begin(),s.end());t.blob.push_back(0);}return t;}
CareerSnapshot closing(const GameState& rules,GameScope scope,bool academy) {
    auto s=bmh::test::world12::completed_qualification_closing(scope);auto& w=s.world;uint32_t next=0xd1000000;
    for(auto& [id,club]:w.clubs)if(club.league.valid())for(size_t row=0;row<20;++row){
        if(club.roster[row].empty()){PlayerRecord player;player.id={next++};player.attributes.name="Reserve";w.players[player.id]=player;club.roster[row]={player.id,uint16_t(row<17?2:0)};}
        auto& player=w.players.at(club.roster[row].player).attributes;player.w364=uint32_t(row==0||row==11?0:row<5?1:row<9?2:3);player.w35c=60u|(75u<<7)|(85u<<14);
    }
    const auto initial_fields=w.initial_competitions;const auto completed_campaigns=w.european_campaigns;w.initial_competitions.reset();w.european_campaigns.reset();
    auto& manager=w.managers[0];manager.finance.reset();manager.tactics.reset();WatcomRandom random(51);
    initialize_manager_contracts(w,manager.seat,rules,{3,false,false,false,2},random);
    w.initial_competitions=initial_fields;w.european_campaigns=completed_campaigns;
    auto& ready=w.managers[0];ready.finance->balance+=5000000;
    ready.career->season_services->promotion_tail_flags=0;
    ready.career->environment.youth[2]={90,5,0};if(academy)ready.career->environment.monthly[1]=8000;
    w.economy->managers[ready.seat].monthly.emplace();w.economy->managers[ready.seat].depreciation=0;
    w.amateur_slots.emplace();
    for(auto country:{Country::germany,Country::england})if(w.catalogue.selectable(country))for(uint32_t row=0;row<8;++row){
        ClubRecord club;club.id={next++};club.country=country;club.name="Authored amateur "+std::to_string(row);club.crest={club.id.value};
        w.crests[club.crest]={"W/TEST.VGA",0};w.clubs[club.id]=club;(*w.amateur_slots)[country==Country::germany?0:1].push_back(club.id);
    }
    w.transfer_market.emplace();s.transfers.emplace();s.validate();return s;
}
}
int main()try {
    set_fixture_tables(bmh::test::authored_fixture_tables());const auto rules=reference();const auto text=texts();
    for(const auto scope:{GameScope{true,false,false},GameScope{false,true,false},GameScope{true,true,false}})for(bool academy:{false,true}) {
        const auto source=closing(rules,scope,academy);const auto qualification=stage_annual_qualification(source);const auto before=qualification;
        AnnualAccountsHooks hooks;size_t presented=0,chosen=0;
        hooks.present=[&](const AnnualTaxEvent& e){++presented;check(e.manager==3&&e.club==source.world.managers[0].club&&e.source.manager_slot==3&&e.source.team==-1,"annual tax presenter lost canonical sparse owner");return true;};
        hooks.choose=[&](const AnnualTaxEvent&)->std::optional<bool>{++chosen;return false;};
        const auto result=stage_annual_accounts(qualification,rules,text,hooks);
        check(qualification==before&&result.snapshot.date==source.date&&result.snapshot.season==source.season&&result.snapshot.calendar==source.calendar&&result.snapshot.results==source.results&&result.snapshot.league_rounds==source.league_rounds,"unpublished accounts changed owner/date/calendar/standings");
        check(presented==1&&chosen==1&&result.tax_events.size()==1&&result.tax_decisions.size()==1&&result.game_draws>=1&&result.watcom_draws>=1,"actual tax and annual draws were skipped");
        check(result.academy.size()==size_t(academy),"annual academy used a fake recruit or skipped actual Source search");
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
