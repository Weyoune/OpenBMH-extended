// SPDX-License-Identifier: GPL-3.0-only
#include "world12_fixture.hpp"
#include "world12_national_fixture.hpp"
#include "fixture_tables.hpp"
#include "game/world12/national.hpp"
#include "game/world12/national_coach.hpp"
#include "game/world12/match_projection.hpp"
#include "game/matchday.hpp"
#include "game/week.hpp"
#include "game/world12/codec.hpp"
#include "game/world12/save.hpp"
#include "app/world12_national.hpp"
#include "game/calendar.hpp"
#include "game/national_selection.hpp"
#include "game/national.hpp"
#include "game/career.hpp"
#include "game/match_engine.hpp"
#include "game/national_identity.hpp"
#include "game/rating.hpp"
#include "game/lineup.hpp"
#include "cli/json.hpp"
#include <algorithm>
#include <iostream>
#include <set>
#include <stdexcept>
namespace {
using namespace bmh;using namespace bmh::world12;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class F>void rejects(F action,const char* message){bool failed=false;try{action();}catch(const std::exception&){failed=true;}check(failed,message);}
GameState rules(){return bmh::test::world12::national_reference();}
World fixture(){return bmh::test::world12::national_world();}
void coach_account_ownership() {
    auto world=fixture();auto reference=rules();
    constexpr size_t monthly[]={0x338,0x33c,0x85c,0x860,0x864,0x86c,0x878,0x87c,0x880,0x884,0x888,0x88c,0x890,0x894,0x898};
    const auto put=[&](size_t at,uint32_t value){for(size_t b=0;b<4;++b)reference.manager_records.at(at+b)=uint8_t(value>>(8*b));};
    for(size_t n=0;n<15;++n)put(4*0xb0c+monthly[n],uint32_t(100+n));
    put(4*0xb0c+0x854,731);put(4*0xb0c+0x858,815);put(4*0xb0c+0x318,42000);reference.manager_records[4*0xb0c+0x8ac]=9;
    world.managers[0].finance=ManagerFinance{73100000};world.managers[1].finance=ManagerFinance{42700000};
    const auto parked=world.managers;initialize_national_coaches(world,reference);
    check(world.managers==parked&&world.national_coaches&&world.national_coaches->size()==2,"national account initialization overwrote parked club state");
    const auto account=world.national_coaches->front().account;check(account.finance.balance==1000000&&account.finance.baseline->starting_balance==1000000&&account.deposit==42000&&account.training_quality==9&&std::all_of(account.training_preferences.begin(),account.training_preferences.end(),[](auto n){return n==2;}),"Source spare account/training initialization differs");
    for(size_t n=0;n<15;++n)check(account.monthly.counters[n]==100+n,"national monthly categories borrowed club fields");
    auto native=reference;initialize_national_manager_record(native);std::copy_n(native.manager_records.begin()+4*0xb0c,0xb0c,native.manager_records.begin());native.date=31;native.level=6;
    auto projected=rules();projected.date=31;projected.level=6;install_national_coach_account(projected,0,account);
    WatcomRandom nr(81),pr(81);manager_daily_interest(native,0,nr);manager_daily_interest(projected,0,pr);
    check(national_coach_account_from_legacy(native,0)==national_coach_account_from_legacy(projected,0)&&nr.seed()==pr.seed()&&nr.calls()==pr.calls(),"national interest/monthly capture differs from Source spare account");
    for(auto& manager:world.managers){manager.career.emplace();manager.career->annual_records.emplace();manager.career->annual_records->national_eligibility=4;}
    auto de=world.managers[0];de.seat=2;de.portrait=2;de.name="Second German coach";de.club=world.membership[0].clubs[1];world.managers.push_back(de);
    auto en=world.managers[1];en.seat=1;en.portrait=1;en.name="Second English coach";en.club=world.membership[5].clubs[1];world.managers.push_back(en);world.settings->planned_managers=4;world.validate();
    const std::vector<uint8_t> order{2,1,0,3};const auto selected=select_national_coaches(world,order);check(selected==std::vector<std::pair<ClubId,uint8_t>>{{{3000000},2},{{3000002},1}},"coach choice ignored saved order/nonzero21c or appointed duplicates");
    for(size_t count=1;count<=4;++count){auto seats=world;seats.managers.resize(count);seats.settings->planned_managers=uint8_t(count);std::vector<uint8_t> saved;for(auto seat:order)if(std::any_of(seats.managers.begin(),seats.managers.end(),[&](const auto& m){return m.seat==seat;}))saved.push_back(seat);const auto chosen=select_national_coaches(seats,saved);check(chosen.size()==(count==1?1u:2u),"one-to-four manager coach capability changed");for(const auto& [team,seat]:chosen){const auto country=seats.clubs.at(team).country;const auto first=std::find_if(saved.begin(),saved.end(),[&](auto id){return seats.manager_country(id)==country;});check(first!=saved.end()&&*first==seat,"coach choice sorted seats instead of saved order");}}
    activate_national_coaches(world,order);check(world.managers[0].career->annual_records->national_eligibility==4,"coach selection cleared Source eligibility");
    WatcomRandom nomination(81);nominate_national_squad(world,Country::germany,4,reference,nomination);nominate_national_squad(world,Country::england,4,reference,nomination);check(world.national_squads[0].coach==2&&world.national_squads[1].coach==1,"nomination replaced selected country coach");
    check(read_save(make_save(CareerSnapshot{.world=world,.results=SeasonResults::empty(world.catalogue),.date=4})).world==world,"separate national accounts lost on regular save roundtrip");
    ClubRecord other;other.id={3000001};other.country=Country::spain;other.kind=ClubKind::national_team;other.name="Authored rating-only nation";other.crest={other.id.value};other.rating=225;world.crests[other.crest]={"W/TEST.VGA",0};world.clubs[other.id]=other;
    CareerSnapshot national_snapshot;national_snapshot.world=world;national_snapshot.results=SeasonResults::empty(world.catalogue);national_snapshot.date=4;national_snapshot.desktop.emplace();national_snapshot.desktop->manager_order=order;national_snapshot.desktop->matchday_number=0;for(const auto& manager:world.managers)national_snapshot.desktop->managers.push_back({manager.seat});
    auto projection=AiMatchProjection::create_for_nationals(national_snapshot,{{3000000},{3000002},{3000001}},reference,{3,6,false,true});
    check(projection.human_managers()==2&&projection.global_manager(0)==2&&projection.global_manager(1)==1&&projection.state.players.size()==40,"national projection lost saved-order coaches or synthesized players");
    const auto de_team=projection.local_club({3000000}),en_team=projection.local_club({3000002});check(is_national_team(projection.state,int(de_team))&&is_national_team(projection.state,int(en_team))&&has_national_squad(projection.state,int(en_team))&&squad_range(projection.state,int(de_team)).first!=squad_range(projection.state,int(en_team)).first,"national rosters overlap or lost actual runtime identity");
    const auto rating_team=projection.local_club({3000001});check(is_national_team(projection.state,int(rating_team))&&!has_national_squad(projection.state,int(rating_team))&&squad_range(projection.state,int(rating_team)).count==0,"rating-only nation acquired fictional players");WatcomRandom rating_random(81);RatingContext rating_context{};rating_context.rand=&rating_random;(void)team_rating(projection.state,int(rating_team),kRatingProfiles[2],rating_context);check(rating_random.calls()==4,"national foreign-strength Source rating was bypassed");
    auto ratings=AiMatchProjection::create_for_nationals(national_snapshot,{{3000001}},reference,{3,6,false,true});check(ratings.state.players.empty()&&!has_national_squad(ratings.state,kNoTeam),"unplayed primary nation acquired a squad in a rating-only fixture");auto rating_noop=world;ratings.commit_match_state(rating_noop);check(rating_noop==world,"rating-only national commit changed real parked squads/accounts");
    auto unchanged=world;projection.commit_match_state(unchanged);check(unchanged==world,"national no-op projection changed parked managers/accounts/nominees");
    check(book_money(projection.state,0,12000,true,true),"national expense was refused");auto paid=world;projection.commit_match_state(paid);check(paid.managers==world.managers&&paid.national_coaches->at(0).account.finance.balance==988000&&paid.national_coaches->at(1).account.finance==world.national_coaches->at(1).account.finance,"national payment leaked into parked clubs or other country");
    auto stale=world;stale.players.at(projection.global_player(0)).cards^=1;const auto interrupted=stale;rejects([&]{projection.commit_match_state(stale);},"stale national player snapshot accepted");check(stale==interrupted,"failed national projection published partial cash/roster mutations");
    auto duplicate=world;duplicate.national_coaches->at(1).coach=2;rejects([&]{duplicate.validate();},"one human silently coached two countries");
    rejects([&]{select_national_coaches(world,{2,2,0,3});},"duplicate saved coach order accepted");
    auto missing=world;missing.national_coaches.reset();check(decode_world(encode_world(missing))==missing,"unknown national finances were reconstructed on load");
    rejects([&]{initialize_national_coaches(world,reference);},"national account initialization reset played cash");
}
void runtime_national_identity() {
    auto native=rules();const auto native_first=squad_range(kNoTeam).first;check(is_national_team(native,kNoTeam)&&has_national_squad(native,kNoTeam)&&!is_national_team(native,kNoTeam+2),"empty national map changed legacy sentinel semantics");
    auto world=fixture();world.managers.erase(world.managers.begin()+1);world.settings->planned_managers=1;WatcomRandom nomination(93);nominate_national_squad(world,Country::germany,4,native,nomination);
    const auto& squad=world.national_squads.front();native.squad_slots.assign(size_t(runtime_dimensions.total_slots),0xfff);native.manager_teams.clear();
    size_t n=0;for(const auto& row:squad.roster){if(row.empty())continue;const auto& player=world.players.at(row.player);native.players.push_back(player.attributes);native.player_bonus.push_back(player.bonus);native.player_cards.push_back(player.cards);native.squad_slots[size_t(native_first)+n]=uint16_t(n|((row.flags&1)?0x8000:0)|((row.flags&2)?0x4000:0));++n;}
    auto mapped=native;const int team=kNoTeam+2;const auto first=mapped.squad_slots.size();mapped.squad_slots.resize(first+24,0xfff);std::copy_n(native.squad_slots.begin()+native_first,24,mapped.squad_slots.begin()+first);std::fill_n(mapped.squad_slots.begin()+native_first,24,0xfff);
    mapped.projected_national_teams={{kNoTeam,std::nullopt},{team,first},{kNoTeam+1,std::nullopt}};
    check(is_national_team(mapped,team)&&has_national_squad(mapped,team)&&squad_range(mapped,team).first==first&&!has_national_squad(mapped,kNoTeam)&&squad_range(mapped,kNoTeam).count==0,"explicit national roster/rating identities alias");
    const auto expected=team_average(native,kNoTeam,0x1ff),actual=team_average(mapped,team,0x1ff);check(expected.value==actual.value&&expected.players==actual.players,"actual national team average changed by runtime identity");
    WatcomRandom nr(121),mr(121);RatingContext nc{},mc{};nc.rand=&nr;mc.rand=&mr;
    check(team_rating(native,kNoTeam,kRatingProfiles[2],nc)==team_rating(mapped,team,kRatingProfiles[2],mc)&&nr.calls()==mr.calls()&&nr.seed()==mr.seed(),"mapped real squad was replaced by rating-only random strength");
    mapped.foreign_strength.resize(size_t(team-runtime_dimensions.record_teams)+2);WatcomRandom abstract_random(121);RatingContext ac{};ac.rand=&abstract_random;(void)team_rating(mapped,kNoTeam,kRatingProfiles[2],ac);check(abstract_random.calls()==4,"abstract primary nation used an empty invented squad");
    auto_lineup(native,kNoTeam,100);auto_lineup(mapped,team,100);check(std::equal(native.squad_slots.begin()+native_first,native.squad_slots.begin()+native_first+24,mapped.squad_slots.begin()+first),"actual national auto-lineup changed by runtime identity");
}
void retained_origin_nomination() {
    auto master=fixture();master.players_initialized=false;master.markets.reset();master.free_players.reset();master.settings.reset();master.managers.clear();master.validate();
    for(const auto scope:{GameScope{false,false,true},GameScope{true,false,true},GameScope{false,true,true}}){
        const auto scoped=select_world_scope(master,scope);check(scoped.national_origins.has_value(),"disabled real roster provenance missing");
        check(national_capabilities(scoped).national_only_manager_limit==1&&national_capabilities(scoped).national_only_choices.size()==2,"real national-only choices/count were invented or omitted");
        for(auto country:{Country::germany,Country::england})check(national_nomination(scoped,country)==national_nomination(master,country),"scope changed actual national nominees or Source tie order");
        auto duplicate=scoped;duplicate.national_origins->push_back(duplicate.national_origins->front());rejects([&]{duplicate.validate();},"duplicate national provenance accepted");
        check(decode_world(encode_world(scoped))==scoped,"retained origin codec changed source identities");
    }
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
    set_fixture_tables(bmh::test::authored_fixture_tables());nomination_rules();coach_account_ownership();runtime_national_identity();retained_origin_nomination();persistence_and_ownership();original_nomination_equivalence();scheduled_owner();
    std::cout<<"Global national selection, quotas, availability, country/coach ownership, native boards, atomic RNG and saved squads passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
