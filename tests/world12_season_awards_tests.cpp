// SPDX-License-Identifier: GPL-3.0-only
#include "world12_fixture.hpp"
#include "fixture_tables.hpp"
#include "game/world12/season_awards.hpp"
#include "app/world12_annual_awards.hpp"
#include "game/world12/codec.hpp"
#include "game/world12/manager_creation.hpp"
#include "game/world12/manager_finance.hpp"
#include "game/season_awards.hpp"
#include "game/season.hpp"
#include "game/schedule.hpp"
#include "game/calendar.hpp"
#include "game/save.hpp"
#include "game/lineup.hpp"
#include <iostream>
#include <numeric>
#include <stdexcept>
namespace {
using namespace bmh;using namespace bmh::world12;
void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
template<class F>void rejects(F f,const char* why){bool refused=false;try{f();}catch(const std::exception&){refused=true;}check(refused,why);}
TextDb texts(){TextDb t;for(size_t i=0;i<TEXT_COUNT;++i){t.strings.push_back("Award "+std::to_string(i));t.offsets.push_back(t.blob.size());const auto& s=t.strings.back();t.blob.insert(t.blob.end(),s.begin(),s.end());t.blob.push_back(0);}return t;}
CareerSnapshot closing() {
    CareerSnapshot s;s.world=select_world_scope(bmh::test::world12::complete_world(),{true,true,false});
    s.world.annual_awards_completed=0;s.world.players_initialized=true;s.world.markets.emplace();s.world.free_players.emplace();s.world.settings=CareerSettings{3,2,5,4,false};s.world.economy.emplace();
    for(const auto& league:s.world.catalogue.leagues) {
        const auto& members=s.world.membership[league.id.value].clubs;
        for(auto club:members)s.world.clubs.at(club).fairplay_points=20;
        s.world.clubs.at(members[2]).fairplay_points=1;
        auto& goal=s.world.players.at(s.world.clubs.at(members[1]).roster[0].player).attributes;
        auto& scorer=s.world.players.at(s.world.clubs.at(members[0]).roster[0].player).attributes;
        goal.w364=(goal.w364&~(63u<<2))|(20u<<2);
        scorer.w364=(scorer.w364&~(63u<<2))|(15u<<2);scorer.w368=(scorer.w368&~(63u<<18))|(6u<<18);
    }
    const std::pair<uint32_t,size_t> selected[]={{0,1},{1,0},{5,2},{8,0}};
    s.desktop.emplace();for(uint8_t seat=0;seat<4;++seat) {
        const auto [league,index]=selected[seat];Manager manager{seat,"Manager "+std::to_string(seat),s.world.membership[league].clubs[index],{league},seat};
        manager.finance=ManagerFinance{1234567,600000,42,17,0};s.world.managers.push_back(manager);s.desktop->managers.push_back({seat});
    }
    std::vector<uint16_t> de(365),en(365);for(int r=0;r<34;++r)de[size_t(7+r*8)]=15;for(int r=0;r<38;++r)en[size_t(7+r*8)]=15;
    const auto calendar=SeasonCalendar::from_sources(s.world.catalogue,de,en);s.calendar=CalendarCursor{calendar,365,365};
    s.date=365;s.game_seed=101;s.watcom_seed=707;s.results=SeasonResults::empty(s.world.catalogue);
    for(const auto& league:s.world.catalogue.leagues){s.league_rounds[league.id.value]=uint8_t(league.rounds());for(auto& score:s.results.leagues[league.id.value])score={true,1,1};}
    const auto epoch=calendar_epoch();s.progress=CareerProgress{ProgressPhase::season_end,DayEntry::plain_day,364,0,uint16_t(epoch.year),uint8_t(epoch.weekday),0};
    s.validate();return s;
}
void kernel_oracles() {
    for(size_t count:{size_t(0),size_t(1),size_t(29),size_t(80)})for(uint8_t flags:{uint8_t(2),uint8_t(4),uint8_t(6)}) {
        std::vector<SeasonScoringEntry> rows;for(size_t i=0;i<count;++i)rows.push_back({uint8_t((i*7)%64),uint8_t((i*11)%64)});
        std::vector<size_t> expected;
        for(size_t i=0;i<rows.size();++i) {
            const auto score=uint8_t((flags&2?rows[i].goals:0)+(flags&4?rows[i].assists:0));if(!score)continue;
            size_t at=0;for(;at<expected.size();++at){const auto& other=rows[expected[at]];const auto prior=uint8_t((flags&2?other.goals:0)+(flags&4?other.assists:0));if(prior<score)break;}
            if(at<10){expected.insert(expected.begin()+ptrdiff_t(at),i);if(expected.size()>10)expected.pop_back();}
        }
        check(rank_season_scorers(rows,flags)==expected,"Source top-ten insertion, zero suppression or tied traversal changed");
    }
    for(int count:{10,18,20})for(int pattern=0;pattern<4;++pattern) {
        std::vector<uint16_t> cards;std::vector<int16_t> teams;std::vector<uint32_t> points;
        for(int i=0;i<count;++i){const auto c=uint16_t(pattern==0?0:pattern==1?(count-i)*256+i:pattern==2?0x8000+i:i%3);cards.push_back(c);teams.push_back(int16_t(29+i));points.push_back(uint32_t((int16_t(c)>>8)*4+(c&255)));}
        quicksort_by_key(teams.data(),points.data(),count,true);
        check(season_fairplay_winner(cards)==size_t(teams.front()-29),"Source fair-play signed card/tie ordering changed");
    }
    rejects([]{rank_season_scorers(std::vector<SeasonScoringEntry>{{64,0}},2);},"invalid packed goal counter accepted");
    rejects([]{season_fairplay_winner({});},"empty fair-play league accepted");
}
void native_money(const CareerSnapshot& snapshot,const AnnualAwardsStage& staged,Country country) {
    uint8_t old_sizes[kLeagueCount];char old_suffix[kLeagueCount];std::copy_n(installed_league_system().sizes,kLeagueCount,old_sizes);std::copy_n(installed_league_system().suffix,kLeagueCount,old_suffix);
    uint8_t sizes[kLeagueCount];for(int i=0;i<5;++i)sizes[i]=snapshot.world.catalogue.league(snapshot.world.catalogue.starting_league(country,uint8_t(i))).clubs;for(int i=5;i<9;++i)sizes[i]=12;
    set_league_sizes(sizes);if(country==Country::england)std::fill_n(installed_league_system().suffix,kLeagueCount,char(0));
    GameState native;native.level=snapshot.world.settings->difficulty;native.manager_records.assign(5*0xb0c,0);native.squad_slots.assign(3072,0xfff);
    native.memory.add_block(layout::DeskRecords_Teams_RuntimeField,276);native.memory.add_block(layout::Season_List_RuntimeField,80);
    std::vector<uint8_t> seats;int team=0;
    for(int league=0;league<5;++league)for(auto id:snapshot.world.membership[snapshot.world.catalogue.starting_league(country,uint8_t(league)).value].clubs) {
        const auto& club=snapshot.world.clubs.at(id);native.memory.set16(layout::DeskRecords_Teams_RuntimeField+uint32_t(team*2),club.fairplay_points);
        for(size_t slot=0;slot<24;++slot)if(!club.roster[slot].empty()){native.squad_slots[size_t(team*24)+slot]=uint16_t(native.players.size());native.players.push_back(snapshot.world.players.at(club.roster[slot].player).attributes);}
        for(const auto& manager:snapshot.world.managers)if(manager.club==id){const auto local=native.manager_teams.size();native.manager_teams.push_back(team);seats.push_back(manager.seat);install_manager_finance(native,int(local),*manager.finance);}
        ++team;
    }
    season_awards(native);
    for(size_t manager=0;manager<seats.size();++manager){const auto actual=manager_finance_from_legacy(native,int(manager));const auto& wanted=*staged.world.managers[seats[manager]].finance;check(actual==wanted,"typed annual payouts differ from complete Original award booking");}
    std::copy_n(old_sizes,kLeagueCount,installed_league_system().sizes);std::copy_n(old_suffix,kLeagueCount,installed_league_system().suffix);
}
void annual_backing() {
    GameState source;source.manager_records.assign(4*0xb0c,0);source.memory.add_block(layout::EnvironmentYouthTeams,4*0x24);source.memory.add_block(layout::Spending,4*0x24);
    const auto at=size_t(3)*0xb0c;for(size_t i=0;i<10;++i)source.manager_records[at+0x8b6+i]=255;
    const auto put=[&](size_t offset,uint32_t value){for(size_t b=0;b<4;++b)source.manager_records[at+offset+b]=uint8_t(value>>(8*b));};
    put(0x328,123400);put(0x32c,uint32_t(int32_t(-456700)));source.manager_records[at+0x8a4]=0x34;source.manager_records[at+0x8a5]=0x12;source.manager_records[at+0x21c]=1;source.manager_records[at+0x21e]=7;
    auto captured=manager_career_state_from_legacy(source,3,{});check(captured.annual_records==ManagerAnnualRecords{123400,-456700,0x1234,1,7},"fresh Source annual account/title backing lost signedness or seat");
    auto older=captured;older.annual_records.reset();check(!manager_career_state_from_legacy(source,3,{},false,&older).annual_records,"ordinary projection invented old unknown annual backing");
    source.manager_records[at+0x327]=0x71;source.manager_records[at+0x334]=0x72;captured.annual_records->tax_paid=987600;captured.annual_records->league_titles=0xbeef;
    install_manager_career_state(source,3,captured,{});check(manager_career_state_from_legacy(source,3,{}).annual_records==captured.annual_records&&source.manager_records[at+0x327]==0x71&&source.manager_records[at+0x334]==0x72,"annual backing install changed neighbours or lost values");
    auto current=closing();current.world.managers[0].career=captured;const auto bytes=encode_world(current.world);check(decode_world(bytes)==current.world,"schema38 lost annual account/award completion facts");
    auto input=cli::parse_json(std::string(bytes.begin(),bytes.end()));input.object()->at("version")=cli::Json(37);auto old=cli::dump_json(input);rejects([&]{decode_world(Bytes(old.begin(),old.end()));},"schema37 admitted known38 annual facts");
    input.object()->erase("annual_awards_completed");auto& managers=std::get<cli::Json::Array>(input.object()->at("managers").value);managers[0].object()->at("career").object()->erase("annual_records");managers[0].object()->at("career").object()->erase("first_recorded_season");old=cli::dump_json(input);const auto decoded=decode_world(Bytes(old.begin(),old.end()));check(!decoded.annual_awards_completed&&!decoded.managers[0].career->annual_records,"schema37 invented annual records/completion");
}
uint64_t draws=0;void observed(int){++draws;}
}
int main()try {
    set_fixture_tables(bmh::test::authored_fixture_tables());kernel_oracles();annual_backing();auto snapshot=closing();const auto before=snapshot;const auto source=texts();
    GameState reference;draws=0;draw_hook=observed;const auto staged=stage_annual_awards(snapshot,reference,source);draw_hook=nullptr;
    check(snapshot==before&&draws==0&&staged.completed_seasons==1,"annual staging changed caller, date or RNG");
    check(staged.awards.size()==30,"dual country award categories omitted an internal league or included foreign-only tables");
    for(const auto& award:staged.awards){const auto& members=snapshot.world.membership[award.league.value].clubs;check(award.club==members[award.kind==AnnualAwardKind::goals?1:award.kind==AnnualAwardKind::scorer?0:2],"annual winner replaced actual goal/assist/discipline evidence");if(award.player.valid())check(award.player.value>4095&&award.club.value>4095,"annual identity was packed into legacy widths");}
    check(staged.world.players==snapshot.world.players&&staged.world.player_statistics==snapshot.world.player_statistics&&staged.world.membership==snapshot.world.membership,"awards changed roster/statistics archive/membership");
    native_money(snapshot,staged,Country::germany);native_money(snapshot,staged,Country::england);
    auto published=snapshot;app::world12::ManagerActionOwner owner{91,7};
    const auto paid=app::world12::publish_annual_awards(published,owner,{91,7},reference,source);
    check(owner.revision==8&&paid==staged.awards&&published.world==staged.world&&published.date==snapshot.date&&published.game_seed==snapshot.game_seed&&published.watcom_seed==snapshot.watcom_seed,"atomic annual award publication lost receipt/revision or changed clock/RNG");
    check(published.world.annual_awards_completed==1&&read_save(make_save(published))==published,"paid closing awards lost exactly-once completion on reload");
    const auto once=published;rejects([&]{app::world12::publish_annual_awards(published,owner,owner,reference,source);},"annual prize paid twice after reload");
    check(published==once&&owner.revision==8,"replayed annual awards changed account/mail/revision");
    auto foreign=snapshot;app::world12::ManagerActionOwner wrong{91,7};rejects([&]{app::world12::publish_annual_awards(foreign,wrong,{92,7},reference,source);},"foreign annual owner published awards");check(foreign==snapshot&&wrong.revision==7,"foreign annual owner changed closing state");
    auto late=snapshot;late.world.managers[3].finance.reset();const auto late_before=late;app::world12::ManagerActionOwner late_owner{91,7};
    rejects([&]{app::world12::publish_annual_awards(late,late_owner,late_owner,reference,source);},"late unknown award account was guessed");
    check(late==late_before&&late_owner.revision==7,"late annual award failure leaked earlier prize/mail/completion");
    auto unknown=snapshot;unknown.world.annual_awards_completed.reset();rejects([&]{stage_annual_awards(unknown,reference,source);},"unknown older closing completion invented");
    auto zero=snapshot;zero.world.settings->difficulty=0;const auto free=stage_annual_awards(zero,reference,source);for(size_t i=0;i<zero.world.managers.size();++i)check(free.world.managers[i].finance==zero.world.managers[i].finance,"Source difficulty0 award silently created money");
    auto pending=snapshot;pending.progress->phase=ProgressPhase::inspect_calendar;rejects([&]{stage_annual_awards(pending,reference,source);},"awards paid before closing boundary");
    pending=snapshot;pending.calendar.reset();pending.progress.reset();--pending.league_rounds[9];pending.results.leagues[9].back()={};rejects([&]{stage_annual_awards(pending,reference,source);},"incomplete league season was awarded");
    rejects([&]{stage_annual_awards(snapshot,reference,TextDb{});},"missing Source award text was invented");check(snapshot==before,"rejected annual payout changed input");
    std::cout<<"Source top-ten/fair-play kernels, complete German/English prize booking, wideIDs, difficulty0 and immutable closing-award staging passed\n";return 0;
}catch(const std::exception& error){draw_hook=nullptr;std::cerr<<error.what()<<'\n';return 1;}
