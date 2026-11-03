// SPDX-License-Identifier: GPL-3.0-only
#include "annual_accounts_fixture.hpp"
#include "fixture_tables.hpp"
#include "game/world12/annual_opening.hpp"
#include "game/world12/economy.hpp"
#include "game/world12/weekly_players.hpp"
#include "game/turn.hpp"
#include "game/week.hpp"
#include <iostream>
#include <set>
namespace {
using namespace bmh;using namespace bmh::world12;
void check(bool yes,const char* text){if(!yes)throw std::runtime_error(text);}
template<class F>void rejects(F f,const char* text){bool refused=false;try{f();}catch(const std::exception&){refused=true;}check(refused,text);}
CareerSnapshot* external=nullptr;void disturb(int){draw_hook=nullptr;++external->game_seed;}
}
int main()try {
    set_fixture_tables(bmh::test::authored_fixture_tables());const auto rules=bmh::test::world12::annual_reference();const auto texts=bmh::test::world12::annual_texts();
    auto authored=bmh::test::world12::completed_account_closing(rules,{false,true,false},false);
    authored.calendar.reset();authored.progress.reset();authored.cups.clear();authored.world.european_campaigns.reset();authored.results=SeasonResults::empty(authored.world.catalogue);authored.league_rounds.fill(0);authored.world.settings->duration=4;
    uint32_t date=1;std::set<int> weekdays;
    for(uint32_t season=1;season<=14&&weekdays.size()<7;++season) {
        GameState clock;for(;;++date){clock.date=date;if(is_season_end(clock))break;}
        auto source=authored;source.date=date++;source.season=season;const auto before=source;weekdays.insert(weekday_code(source.date));
        const auto result=stage_annual_opening_services(source,rules,texts,{});const auto repeated=stage_annual_opening_services(source,rules,texts,{});
        check(source==before&&result.snapshot==repeated.snapshot&&result.game_draws==repeated.game_draws&&result.watcom_draws==repeated.watcom_draws,"annual first-Wednesday services changed owner or rerolled");
        check(weekday_code(result.snapshot.date)==2&&result.days<=6&&result.snapshot.date==source.date+result.days&&!result.snapshot.calendar&&!result.snapshot.progress,"annual opening invented a calendar day or published an old cursor");
        check(result.snapshot.world.markets->stocks==source.world.markets->stocks&&result.snapshot.world.markets->funds==source.world.markets->funds,"annual direct-interest days incorrectly advanced stock/fund prices");
        auto interest_only=source;size_t trainings=0;uint64_t g=0,w=0;
        while(weekday_code(interest_only.date)!=2){const auto prior=interest_only.date++;const auto paid=daily_finance(interest_only,rules,DailyEconomyKind::after_event);g+=paid.game_draws;w+=paid.watcom_draws;if(weekday_code(prior)==6){const auto trained=train_manager_squads(interest_only,rules);g+=trained.game_draws;w+=trained.watcom_draws;++trainings;}}
        check(result.training_passes==trainings&&result.game_draws==g&&result.watcom_draws>=w&&result.new_listings>0,"annual training/listing order skipped a Source phase");
        check(read_save(make_save(result.snapshot))==result.snapshot,"annual quiescent opening failed exact checkpoint roundtrip");
        auto wrong=source;++wrong.date;rejects([&]{stage_annual_opening_services(wrong,rules,texts,{});},"annual first-Wednesday service replay accepted after1July");
    }
    check(weekdays.size()==7,"authored annual service fixtures missed a weekday boundary");
    auto changed=authored;changed.date=365;changed.season=1;const auto before=changed;external=&changed;draw_hook=disturb;rejects([&]{stage_annual_opening_services(changed,rules,texts,{});},"annual opening overwrote late external owner");draw_hook=nullptr;auto expected=before;++expected.game_seed;check(changed==expected,"annual opening failure leaked services or rewound external state");
    std::cout<<"Annual first-Wednesday Source interest/training/new-listing services: seven weekday starts, no stock updates, snapshot/replay/owner guards passed (unpublished)\n";return 0;
}catch(const std::exception& error){draw_hook=nullptr;std::cerr<<error.what()<<'\n';return 1;}
