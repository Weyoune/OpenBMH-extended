// SPDX-License-Identifier: GPL-3.0-only
#include "world12_fixture.hpp"
#include "fixture_tables.hpp"
#include "game/world12/national.hpp"
#include "game/world12/codec.hpp"
#include "game/world12/save.hpp"
#include "app/world12_national.hpp"
#include "game/calendar.hpp"
#include "game/national_selection.hpp"
#include "game/national.hpp"
#include "game/career.hpp"
#include "game/match_engine.hpp"
#include "cli/json.hpp"
#include <algorithm>
#include <iostream>
#include <set>
#include <stdexcept>
namespace {
using namespace bmh;using namespace bmh::world12;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class F>void rejects(F action,const char* message){bool failed=false;try{action();}catch(const std::exception&){failed=true;}check(failed,message);}
GameState rules() {
    GameState g;g.manager_records.resize(5*0xb0c);g.manager_options.resize(20);
    g.tactics.resize(5*0x840);g.formations.resize(0x580);g.club_ids.resize(445);
    g.team_flags.resize(445);g.team_morale.resize(445,25);g.club_heap_78.resize(890);
    g.manager_ratings.resize(240);g.manager_results.resize(size_t(runtime_dimensions.record_teams)*40,255);
    g.memory.add_block(layout::CompetitionSelection,4);g.memory.set32(layout::CompetitionSelection,3);
    const uint8_t depth[]={0,5,5,5,5,10,10,10,10,17,17},lane[]={4,1,3,5,7,1,3,5,7,3,5};
    for(size_t m=0;m<4;++m)for(size_t mode=0;mode<2;++mode)for(size_t f=0;f<4;++f)for(size_t r=0;r<11;++r) {
        const auto at=m*0x160+mode*0xb0+f*22+r;g.formations[at]=depth[r];g.formations[at+11]=lane[r];
    }
    return g;
}
World fixture() {
    World w=bmh::test::world12::complete_world();uint32_t next=1000000;
    for(auto& [id,club]:w.clubs) {
        for(size_t r=11;r<20;++r) {
            PlayerRecord p;p.id={next++};p.attributes={"Reserve "+std::to_string(p.id.value),0,0,0};
            w.players[p.id]=p;club.roster[r]={p.id,uint16_t(r<17?2:0)};
        }
        for(size_t r=0;r<20;++r) {
            auto& p=w.players.at(club.roster[r].player).attributes;
            p.w35c=60u|(uint32_t(60+r%8)<<7)|(uint32_t(75+r%7)<<14);
            p.w368=uint32_t(club.country)<<27;
            p.w364=uint32_t(r<3?0:r<8?1:r<15?2:3)|(10u<<21);
        }
    }
    for(auto country:{Country::germany,Country::england}) {
        ClubRecord nation;nation.id={3000000+uint32_t(country)};nation.name="National "+std::to_string(int(country));
        nation.country=country;nation.kind=ClubKind::national_team;nation.crest={nation.id.value};
        w.crests[nation.crest]={"W/TEST.VGA",0};w.clubs[nation.id]=nation;
    }
    w.players_initialized=true;w.markets.emplace();w.free_players.emplace();w.settings=CareerSettings{3,6,5,2,false};
    const auto base=rules();
    for(auto country:{Country::germany,Country::england}) {
        Manager m;m.seat=country==Country::germany?0:3;m.portrait=m.seat;m.name="Manager "+std::to_string(m.seat);
        m.start_league=w.catalogue.starting_league(country,0);m.club=w.membership[m.start_league.value].clubs[0];
        ManagerTactics t;t.backing.emplace();std::copy_n(base.formations.begin(),0x160,t.backing->formations.begin());
        std::copy_n(base.formations.begin()+0x160,13,t.backing->formation_tail.begin());
        t.backing->enabled_formations={15,15};t.automatic_formation=1;m.tactics=t;w.managers.push_back(m);
    }
    w.validate();return w;
}
void nomination_rules() {
    NationalSquadSelection selection;
    selection.consider(0,10,10);selection.consider(0,20,20);selection.consider(0,30,30);
    selection.consider(0,10,99);check(selection.token(0,0)==10,"equal threshold replaced the earlier keeper");
    selection.consider(0,25,25);check(selection.token(0,0)==20&&selection.token(0,1)==25&&selection.token(0,2)==30,"ascending bounded selection changed");
    auto w=fixture();const auto before=w;
    const auto en=national_nomination(w,Country::england);check(w==before,"read-only national selection mutated clubs");
    std::array<int,4> counts{};std::set<PlayerId> unique;
    for(const auto& row:en){++counts[w.players.at(row.player).attributes.position()];unique.insert(row.player);check(row.player.value>4095,"global nomination was narrowed");}
    check(counts==std::array<int,4>{3,5,7,5}&&unique.size()==20,"national positional quotas/identity uniqueness changed");
    auto& unavailable=w.players.at(en.back().player).attributes;
    unavailable.w35c|=1u<<27;unavailable.w364|=1u<<16;
    auto without=national_nomination(w,Country::england);
    check(std::none_of(without.begin(),without.end(),[&](const auto& row){return row.player==en.back().player;}),"simultaneously injured/suspended player nominated");
    unavailable.w364&=~(31u<<16);without=national_nomination(w,Country::england);
    check(std::any_of(without.begin(),without.end(),[&](const auto& row){return row.player==en.back().player;}),"Original availability OR was changed to AND");
    const auto abroad=w.membership[6].clubs[0];auto& foreign=w.players.at(w.clubs.at(abroad).roster[0].player).attributes;
    foreign.w368=uint32_t(Country::england)<<27;foreign.w35c=99u|(99u<<7)|(99u<<14);
    const auto selected=national_nomination(w,Country::england);
    check(std::any_of(selected.begin(),selected.end(),[&](const auto& row){return row.club==abroad;}),"eligible expatriate outside England was ignored");
    check(national_nomination_strength(foreign,true)>national_nomination_strength(foreign,false),"foreign-club score bonus missing");
    auto invalid=w;for(auto& [id,p]:invalid.players)if(p.attributes.nationality()==2)p.attributes.w364&=~3u;
    rejects([&]{national_nomination(invalid,Country::england);},"missing positional quotas fabricated duplicate players");
}
void persistence_and_ownership() {
    auto w=fixture();const auto original=w;auto native=rules();WatcomRandom random(713);
    const auto de=nominate_national_squad(w,Country::germany,4,native,random);
    check(!de.coach&&de.watcom_draws>0&&w.managers==original.managers&&w.players==original.players,
          "multiplayer nomination changed club manager contracts/players or appointed an implicit coach");
    for(const auto& [id,club]:original.clubs)check(w.clubs.at(id).roster==club.roster,"nomination removed a player from his club");
    const auto en=nominate_national_squad(w,Country::england,4,native,random);
    check(en.team!=de.team&&w.national_squads.size()==2,"German/English national rosters collided");
    for(const auto& squad:w.national_squads) {
        std::set<PlayerId> players;int first=0,reserves=0;
        for(const auto& row:squad.roster)if(!row.empty()){players.insert(row.player);first+=(row.flags&1)!=0;reserves+=(row.flags&2)!=0;}
        check(players.size()==20&&first==11&&reserves==6,"shared Original national auto-lineup did not produce 11+6+3");
    }
    check(decode_world(encode_world(w))==w,"schema24 lost national tactics/rosters/global references");
    CareerSnapshot saved;saved.world=w;saved.date=4;saved.results=SeasonResults::empty(w.catalogue);
    check(read_save(make_save(saved))==saved,"national squad snapshot did not survive ordinary save/load");
    const auto stable=w;const auto seed=random.seed();const auto calls=random.calls();
    rejects([&]{nominate_national_squad(w,Country::england,4,native,random);},"same appointment nominated twice");
    check(w==stable&&random.seed()==seed&&random.calls()==calls,"rejected nomination consumed state/randomness");
    auto malformed=native;malformed.formations.clear();
    rejects([&]{nominate_national_squad(w,Country::england,18,malformed,random);},"missing rule workspace accepted");
    check(w==stable&&random.seed()==seed,"late preparation failure published partial nation state");
    nominate_national_squad(w,Country::germany,18,native,random);
    check(w.national_squads.size()==2&&w.national_squads[0].nominated_on==18,"new nomination duplicated national identity");
    auto single=fixture();single.managers.erase(single.managers.begin());single.settings->planned_managers=1;
    const auto coached=nominate_national_squad(single,Country::england,4,native,random);
    check(coached.coach==3&&single.managers[0].club==original.managers[1].club,"single English coach became German or lost his club");
    auto bad=single;bad.national_squads[0].coach=0;rejects([&]{bad.validate();},"national coach uses nonexistent sparse seat");
    bad=single;bad.national_squads[0].roster[1]=bad.national_squads[0].roster[0];rejects([&]{bad.validate();},"duplicate national player accepted");
    auto encoded=encode_world(single);auto json=cli::parse_json(std::string(encoded.begin(),encoded.end()));json.object()->at("version")=23;
    const auto older=cli::dump_json(json);rejects([&]{decode_world(Bytes(older.begin(),older.end()));},"older schema silently discarded active national squad");
    encoded=encode_world(original);json=cli::parse_json(std::string(encoded.begin(),encoded.end()));
    json.object()->at("version")=23;json.object()->erase("national_squads");
    const auto legacy=cli::dump_json(json);
    check(decode_world(Bytes(legacy.begin(),legacy.end()))==original,"schema23 migration invented national state");
    single.managers.push_back(original.managers[0]);single.settings->planned_managers=2;
    const auto multiple=nominate_national_squad(single,Country::england,18,native,random);
    check(!multiple.coach,"automatic single-manager coaching persisted after a second manager joined");
}
void original_nomination_equivalence() {
    for(auto country:{Country::germany,Country::england})for(uint32_t seed:{7u,713u,90210u})for(bool multi:{false,true}) {
        auto w=fixture();
        if(!multi){w.managers.erase(std::remove_if(w.managers.begin(),w.managers.end(),
            [&](const auto& m){return w.manager_country(m.seat)!=country;}),w.managers.end());w.settings->planned_managers=1;}
        const auto foreign=w.membership[6].clubs[0];
        auto& expatriate=w.players.at(w.clubs.at(foreign).roster[0].player).attributes;
        expatriate.w368=uint32_t(country)<<27;expatriate.w35c=90u|(90u<<7)|(95u<<14);
        auto core=rules();initialize_national_manager_record(core);
        core.manager_teams=multi?std::vector<int>{0,1}:std::vector<int>{0};core.manager_options[0]=1;core.level=6;core.date=4;
        core.manager_records[0x218]=15;core.manager_records[0x219]=15;core.manager_records[0xb0c+0x290]=1;
        core.manager_records[4*0xb0c+0x290]=uint8_t(kNoTeam);
        core.manager_records[4*0xb0c+0x291]=uint8_t(kNoTeam>>8);
        core.squad_slots.assign(size_t(runtime_dimensions.total_slots),0xfff);
        std::vector<PlayerId> mapping;size_t home=0,abroad=90*24;
        // The Original scanner ignores other nationalities. Keep every eligible
        // player, source order and foreign-club threshold; omit only the records
        // that cannot affect selection. The English numeric oracle uses the
        // Original home nationality zero without changing any football skill.
        for(auto group:{TableGroup::germany,TableGroup::england,TableGroup::europe})
            for(auto league:w.catalogue.simulation_leagues(group))for(auto id:w.membership[league.value].clubs) {
                const auto& club=w.clubs.at(id);
                for(const auto& row:club.roster)if(!row.empty()) {
                    const auto& p=w.players.at(row.player);
                    if(p.attributes.nationality()!=int(country))continue;
                    const auto at=club.country==country?home++:abroad++;
                    check(at<size_t(runtime_dimensions.pool_first),"source oracle candidate space exceeded");
                    const auto local=uint16_t(mapping.size());mapping.push_back(row.player);
                    auto person=p.attributes;person.w368&=~(31u<<27);
                    core.players.push_back(person);core.player_cards.push_back(p.cards);core.player_bonus.push_back(0);
                    core.squad_slots[at]=uint16_t(local|((row.flags&1)?0x8000:0)|((row.flags&2)?0x4000:0));
                }
            }
        core.counts.players=uint32_t(mapping.size());core.club_names={'Q',0};
        WatcomRandom native_random(seed),typed_random(seed);MinuteReport report;
        check(national_team_day(core,native_random,report)==!multi&&!report.unsupported,"Original national nomination failed");
        const auto result=nominate_national_squad(w,country,4,rules(),typed_random);
        const auto& squad=w.national_squads.front();const auto range=squad_range(kNoTeam);
        check(result.watcom_draws==native_random.calls()&&typed_random.seed()==native_random.seed(),
              "global nomination differs from Original board/contract RNG");
        for(size_t r=0;r<20;++r) {
            const auto entry=core.squad_slots.at(range.first+r);
            check(squad.roster[r].player==mapping.at(entry&0xfff),"global national lineup differs from Original");
            const auto board=(multi?4*0x840:0)+r*0x58;
            for(size_t offset=0;offset<0x58;++offset) {
                if(offset==0x44||offset==0x45||offset==0x46||offset==0x47||offset==2||offset==3||offset==4||offset==5||offset==0x3f||offset==0x41||offset==0x42)continue;
                check(squad.tactics.backing->scalar_rows[r][offset]==core.tactics[board+offset],
                      "national contract/familiarity bytes differ from Original");
            }
            const auto& row=squad.tactics.rows[r];
            check(std::equal(row.orders.begin(),row.orders.end(),core.tactics.begin()+board+2)&&
                  row.partner==core.tactics[board+0x3f]&&row.depth==core.tactics[board+0x41]&&row.lane==core.tactics[board+0x42],
                  "national orders/coordinates differ from Original");
        }
    }
}
void scheduled_owner() {
    namespace app=bmh::app::world12;
    CareerSnapshot state;state.world=fixture();state.date=4;state.watcom_seed=713;
    state.results=SeasonResults::empty(state.world.catalogue);
    SeasonCalendar calendar;
    for(const auto& league:state.world.catalogue.leagues)
        for(uint8_t round=1;round<=league.rounds();++round)calendar.leagues.push_back({uint16_t(50+round),league.id,round});
    calendar.others.push_back({4,CalendarDomain::shared,0x4006});state.calendar=CalendarCursor{calendar,4,0};
    state.desktop.emplace();
    for(const auto& manager:state.world.managers){ManagerDesktop desk;desk.seat=manager.seat;state.desktop->managers.push_back(desk);}
    state.desktop->manager_order={3,0};state.desktop->matchday_number=123;
    const auto epoch=calendar_epoch();state.progress=CareerProgress{ProgressPhase::scheduled_event,DayEntry::plain_day,3,0,uint16_t(epoch.year),uint8_t(epoch.weekday),3};
    state.validate();const auto original=state;app::ManagerActionOwner owner{71,1};
    rejects([&]{app::prepare_national_appointment(state,owner,{72,1},rules());},"foreign scope prepared national squads");
    auto older=read_save(make_save(state));older.desktop->manager_order.clear();older.desktop->matchday_number.reset();
    const auto old_file=make_save(older);
    check(old_file.stream[7]=='B'&&read_save(old_file)==older,"old SB progress did not retain unknown manager order");
    rejects([&]{app::prepare_national_appointment(older,owner,owner,rules());},"old missing event order was fabricated");
    auto incomplete=state;
    for(auto& [id,player]:incomplete.world.players)if(player.attributes.nationality()==2)player.attributes.w364&=~3u;
    const auto invalid_before=incomplete;auto invalid_owner=owner;
    rejects([&]{app::prepare_national_appointment(incomplete,invalid_owner,invalid_owner,rules());},"late second-country error was accepted");
    check(incomplete==invalid_before&&invalid_owner.revision==owner.revision,"late nomination error published German squad/RNG before English failure");
    const auto result=app::prepare_national_appointment(state,owner,owner,rules());
    check(result.nominations.size()==2&&!result.coach_desk_required&&result.game_draws==0&&result.watcom_draws>0&&
          owner.revision==2&&state.date==4&&state.calendar==original.calendar&&state.desktop==original.desktop,
          "national preparation skipped the event, changed its order or consumed wrong owners");
    check(read_save(make_save(state))==state&&make_save(state).stream[7]=='C',"saved national appointment/order did not resume exactly");
    const auto pending=state;const auto revision=owner.revision;
    rejects([&]{app::prepare_national_appointment(state,owner,owner,rules());},"prepared appointment was replayed");
    check(state==pending&&owner.revision==revision,"replayed appointment changed RNG/revision");
    auto invalid=state;invalid.desktop->manager_order={3,3};rejects([&]{make_save(invalid);},"duplicate persistent manager order accepted");
    invalid=state;invalid.desktop->matchday_number=10001;rejects([&]{make_save(invalid);},"out-of-range persistent matchday number accepted");
    auto solo=original;solo.world.managers.erase(solo.world.managers.begin());solo.world.settings->planned_managers=1;
    solo.desktop->managers.erase(solo.desktop->managers.begin());solo.desktop->manager_order={3};
    app::ManagerActionOwner solo_owner{72,1};
    const auto coached=app::prepare_national_appointment(solo,solo_owner,solo_owner,rules());
    check(coached.coach_desk_required&&coached.nominations.size()==1&&coached.nominations[0].coach==3,
          "English single-manager appointment did not retain its coached desk boundary");
    const auto waiting=solo;const auto waiting_revision=solo_owner.revision;
    rejects([&]{app::finish_uncoached_national_appointment(solo,solo_owner,solo_owner,rules());},"coached nomination silently skipped its manager desk");
    check(solo==waiting&&solo_owner.revision==waiting_revision,"rejected coached closure changed state/RNG");
}
}
int main()try {
    set_fixture_tables(bmh::test::authored_fixture_tables());nomination_rules();persistence_and_ownership();original_nomination_equivalence();scheduled_owner();
    std::cout<<"Global national selection, quotas, availability, country/coach ownership, native boards, atomic RNG and saved squads passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
