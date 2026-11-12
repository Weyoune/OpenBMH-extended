// SPDX-License-Identifier: GPL-3.0-only
#include "annual_accounts_fixture.hpp"
#include "fixture_tables.hpp"
#include "game/world12/annual_players.hpp"
#include "game/world12/codec.hpp"
#include <iostream>
#include <set>
namespace {
using namespace bmh;using namespace bmh::world12;
void check(bool yes,const char* text){if(!yes)throw std::runtime_error(text);}
template<class F>void rejects(F f,const char* text){bool refused=false;try{f();}catch(const std::exception&){refused=true;}check(refused,text);}
struct Fixture {CareerSnapshot before;UnpublishedAnnualQualification qualification;std::vector<AnnualPromotionOutcome> exchanges;PlayerId injured;ClubId old_club;};
Fixture prepared(const GameState& rules,const TextDb& texts,GameScope scope,bool release,bool loan=false,bool full_pools=false) {
    auto closing=bmh::test::world12::completed_account_closing(rules,scope,false);Fixture fixture;
    const auto league=scope.germany?LeagueId{4}:LeagueId{8};fixture.old_club=closing.world.membership[league.value].clubs[5];fixture.injured=closing.world.clubs.at(fixture.old_club).roster[0].player;
    if(release){auto& out=closing.world.players.at(fixture.injured).attributes;out.w35c|=31u<<27;out.w364|=31u<<16;}  // duration and injury type 0x1f
    if(release||loan){
        closing.world.loans.push_back({fixture.injured,closing.world.managers[0].club,fixture.old_club,3,uint16_t(5)});
        LoanContract contract;contract.player=fixture.injured;contract.manager=3;contract.source_slot=2;contract.board[6]=19;contract.board[15]=77;closing.transfers->loans.push_back(contract);
        closing.world.player_statistics[{fixture.old_club,fixture.injured}]={{65536,17,3},65556,12,9};
    }
    if(full_pools)for(size_t n=0;n<100;++n){PlayerRecord player;player.id={uint32_t(0xc1000000u+n)};player.attributes={"Pool "+std::to_string(n),60u|(75u<<7)|(85u<<14),n<80?0u:5u<<27,1};closing.world.players.emplace(player.id,player);if(n<80)closing.world.free_players->domestic[n]=player.id;else closing.world.free_players->foreign[n-80]=player.id;}
    fixture.qualification=stage_annual_qualification(closing);AnnualAccountsHooks hooks;hooks.present=[](const auto&){return true;};hooks.choose=[](const auto&)->std::optional<bool>{return false;};
    auto accounts=stage_annual_accounts(fixture.qualification,rules,texts,hooks);fixture.before=accounts.snapshot;
    // This authored calendar contains no regional-playoff appointments. The
    // production coordinator requires the actual regional play-off campaign instead.
    for(auto country:{Country::germany,Country::england})if(fixture.before.world.catalogue.selectable(country)) {
        const AnnualDecisionContext context{{country,(*fixture.before.world.amateur_slots)[country==Country::germany?0:1]},country==Country::germany?RegionalPromotion::champions_only:RegionalPromotion::not_applicable,{}};
        const auto outcome=evaluate_annual_promotion_decisions(fixture.before,context);const auto moved=stage_annual_membership(fixture.before,outcome,texts);fixture.exchanges.push_back(outcome);
        fixture.before.world=moved.world;fixture.before.game_seed=moved.game_seed;fixture.before.watcom_seed=moved.watcom_seed;
    }
    // An authored fixed permutation verifies retained rank lineage. Actual
    // annual fixture shuffling belongs to the outer Source RNG owner.
    for(auto& members:fixture.before.world.membership)if(members.clubs.size()>1)std::rotate(members.clubs.begin(),members.clubs.begin()+1,members.clubs.end());
    fixture.before.validate();return fixture;
}
CareerSnapshot* external=nullptr;void disturb(int){draw_hook=nullptr;++external->watcom_seed;}
}
int main()try {
    set_fixture_tables(bmh::test::authored_fixture_tables());const auto rules=bmh::test::world12::annual_reference();const auto texts=bmh::test::world12::annual_texts();
    for(auto scope:{GameScope{true,false,false},GameScope{false,true,false},GameScope{true,true,false}})for(bool release:{false,true}) {
        const auto input=prepared(rules,texts,scope,release);const auto before=input.before;
        const auto result=stage_annual_player_renewal(input.before,input.qualification,input.exchanges,rules,texts);
        const auto repeated=stage_annual_player_renewal(input.before,input.qualification,input.exchanges,rules,texts);
        check(input.before==before&&result.world==repeated.world&&result.desktop==repeated.desktop&&result.transfers==repeated.transfers&&result.game_seed==repeated.game_seed&&result.watcom_seed==repeated.watcom_seed&&result.game_draws==repeated.game_draws&&result.watcom_draws==repeated.watcom_draws,"annual renewal was non-deterministic or changed its owner");
        check(result.released==size_t(release)&&result.visited==before.world.players.size()&&!result.inactive_origin_refresh_required,"annual active slot count/release/provenance receipt is wrong");
        check(result.world.player_statistics==before.world.player_statistics,"annual retirement destroyed real historical player/club counters");
        std::set<PlayerId> owned;
        for(const auto& [id,club]:result.world.clubs){if(club.league.valid())check(club.fairplay_points==0,"annual discipline counter was not reset");for(const auto& slot:club.roster)if(!slot.empty())check(owned.insert(slot.player).second,"renewal duplicated a real roster player");}
        for(auto id:result.world.free_players->domestic)if(id.valid())check(owned.insert(id).second,"renewal duplicated domestic pool player");for(auto id:result.world.free_players->foreign)if(id.valid())check(owned.insert(id).second,"renewal duplicated foreign pool player");
        for(const auto& [id,player]:before.world.players)if(id!=input.injured||!release)check(result.world.players.at(id).attributes.age()==player.attributes.age()+1,"real player did not age exactly once");
        if(release){check(result.world.loans.empty(),"retired real loan ledger entry remained active");check(owned.contains(input.injured)&&result.world.players.at(input.injured).attributes.age()>=20&&result.world.players.at(input.injured).attributes.age()<=31,"actual released identity was invented/lost or aged twice after regeneration");}
        const auto& manager=result.world.managers[0];check(manager.finance->bonus_paid==0&&manager.career->annual_records->player_awards==0&&manager.career->strengths_display_word==0x1ff&&manager.career->pairings_display_byte==1&&manager.career->table_display_word==0x11&&manager.career->other_games_view==0x80&&manager.career->stadium_curve_flags==0x40,"annual Source board/presentation counters were not reset");
        check(result.desktop&&result.desktop->managers[0].display_word==0x15,"annual desktop preferences were not reset");
        for(const auto& row:manager.tactics->backing->scalar_rows)check(row[6]==0&&row[15]>=20&&row[15]<=50,"annual familiarity reset was skipped or rerolled after player ageing");
        auto snapshot=before;snapshot.world=result.world;snapshot.desktop=result.desktop;snapshot.transfers=result.transfers;snapshot.game_seed=result.game_seed;snapshot.watcom_seed=result.watcom_seed;
        check(read_save(make_save(snapshot))==snapshot,"unpublished renewal backing did not roundtrip");
        auto altered=before;std::swap(altered.world.membership[10].clubs,altered.world.membership[11].clubs);rejects([&]{annual_player_ranks(altered.world,input.qualification,input.exchanges);},"foreign membership permutation accepted as annual shuffle");
    }
    {
        const auto input=prepared(rules,texts,{true,false,true},false);
        check(input.before.world.national_origins&& !input.before.world.national_origins->empty(),"authored inactive provenance fixture is absent");
        const auto result=stage_annual_player_renewal(input.before,input.qualification,input.exchanges,rules,texts);
        check(!result.inactive_origin_refresh_required&&result.world.catalogue==input.before.world.catalogue&&result.world.membership==input.before.world.membership&&result.world.clubs.size()==input.before.world.clubs.size(),
            "inactive annual population became active fixtures/clubs or stayed frozen");
        check(result.world.national_origins->size()==input.before.world.national_origins->size(),"inactive annual population lost actual origin records");
        for(size_t n=0;n<input.before.world.national_origins->size();++n){const auto& old=(*input.before.world.national_origins)[n];const auto& current=(*result.world.national_origins)[n];
            check(current.club.id==old.club.id&&current.rank==old.rank&&current.source_initialized==old.source_initialized,"annual origin order/provenance was invented");
            for(const auto& slot:old.club.roster)if(!slot.empty())check(result.world.players.at(slot.player).attributes.age()==input.before.world.players.at(slot.player).attributes.age()+1,
                "actual inactive-origin player did not age exactly once");
        }
        check(result.visited==input.before.world.players.size(),"merged active/inactive physical pass skipped or duplicated actual player identities");
    }
    {
        const auto input=prepared(rules,texts,{true,true,false},true,true,true);
        ClubId source_club;for(const auto& [id,club]:input.before.world.clubs)for(const auto& slot:club.roster)if(slot.player==input.injured)source_club=id;
        const auto result=stage_annual_player_renewal(input.before,input.qualification,input.exchanges,rules,texts);
        ClubId destination;size_t found=0;for(const auto& [id,club]:result.world.clubs)for(const auto& slot:club.roster)if(slot.player==input.injured){destination=id;++found;}
        check(result.released==1&&found==1&&destination.valid()&&destination!=source_club&&result.world.players.size()==input.before.world.players.size(),
            "Source full-pool retirement lost/invented a player or scored its original club as buyer");
        check(result.visited==input.before.world.players.size(),"overflow placement aged a regenerated player twice");
    }
    {
        const auto input=prepared(rules,texts,{true,true,false},false,true);
        const auto result=stage_annual_player_renewal(input.before,input.qualification,input.exchanges,rules,texts);
        check(result.released==0&&result.transfers&&result.transfers->loans.size()==1&&result.world.loans==input.before.world.loans,
            "Source annual reset dropped an actual live loan");
        const auto& board=result.transfers->loans[0];check(board.board[6]==0&&board.board[15]>=20&&board.board[15]<=50&&board.source_slot==2,
            "actual loan-owner board familiarity/season reset was not published");
    }
    {
        auto input=prepared(rules,texts,{true,true,false},false);const auto before=input.before;external=&input.before;draw_hook=disturb;
        rejects([&]{stage_annual_player_renewal(input.before,input.qualification,input.exchanges,rules,texts);},"changed annual renewal owner was overwritten");draw_hook=nullptr;auto expected=before;++expected.watcom_seed;check(input.before==expected,"cancelled annual renewal leaked state or rewound external mutation");
        auto world=before.world;auto& career=*world.managers[0].career;career.first_recorded_season=7;for(size_t i=0;i<150;++i)(*career.season_records)[i]=uint8_t(i);const auto original=*career.season_records;
        begin_annual_manager_history(world,50);check(world.managers[0].career->first_recorded_season==8&&(*world.managers[0].career->season_records)[147]==0xfe,"Source annual history shift/start origin is wrong");for(size_t i=0;i<147;++i)check((*world.managers[0].career->season_records)[i]==original[i+3],"Source fifty-season history shift changed byte order");
        world=before.world;world.managers[0].career->first_recorded_season.reset();const auto unknown=world;rejects([&]{begin_annual_manager_history(world,50);},"old unknown history origin invented for a real shift");check(world==unknown,"history guard leaked partial shifts");begin_annual_manager_history(world,1);
    }
    std::cout<<"Annual active player renewal: real ageing, injury retirement/regeneration, loan/statistics lineage, unique pools, Source resets/history, retained ranks, save and owner rollback passed (unpublished)\n";return 0;
}catch(const std::exception& error){draw_hook=nullptr;std::cerr<<error.what()<<'\n';return 1;}
