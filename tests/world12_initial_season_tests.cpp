// SPDX-License-Identifier: GPL-3.0-only
#include "world12_fixture.hpp"
#include "app/world12_initial_season.hpp"
#include "game/world12/codec.hpp"
#include "game/world12/match_projection.hpp"
#include "cli/json.hpp"
#include <set>
#include <iostream>
namespace {
using namespace bmh;using namespace bmh::world12;namespace owner_app=bmh::app::world12;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class F>void rejects(F f,const char* message){bool failed=false;try{f();}catch(const std::exception&){failed=true;}check(failed,message);}
LeImage source(bool english) {
    auto exe=LeImage::from_bytes(layout::GermanDataSegmentBegin,Bytes(0x6000));
    const auto byte=[&](uint32_t at,uint8_t value){exe.patch(at,&value,1);};
    byte(layout::LeagueSizes,english?20:18);
    for(uint32_t l=0;l<4;++l){byte(layout::Career_BuildStadium_RuntimeField4+l,5);for(uint32_t i=0;i<22;++i)byte(layout::Career_BuildStadium_RuntimeField+l*23+i,1);}
    for(uint32_t i=0;i<22;++i){byte(layout::Career_BuildStadium_RuntimeField2+i*5,2);byte(layout::Career_BuildStadium_RuntimeField3+i*5,5);}
    const auto cups=bmh::test::world12::country_rules(english);for(uint32_t i=0;i<36*40;++i)byte(layout::CupTable+i,cups.u8(layout::CupTable+i));return exe;
}
GameState reference() {
    GameState g;g.club_ids.resize(445);g.team_flags.resize(445);g.team_morale.resize(445);g.formations.resize(0x580);g.manager_records.resize(5*0xb0c);g.manager_options.resize(20);g.manager_ratings.resize(240);g.tactics.resize(4*0x840);
    const uint8_t depth[]={0,5,5,5,5,10,10,10,10,17,17};
    for(size_t m=0;m<4;++m){g.manager_options[m*5]=1;for(size_t mode=0;mode<2;++mode)for(size_t f=0;f<8;++f)for(size_t r=0;r<11;++r){const auto at=m*0x160+mode*0xb0+f*22+r;g.formations[at]=depth[r];g.formations[at+11]=uint8_t(r%9);}}
    g.memory.add_block(layout::EnvironmentYouthTeams,4*0x24);g.memory.add_block(layout::Career_CompetitionPointersEnglish_ExecutableField5,4);g.memory.set_pointer(layout::Career_CompetitionPointersEnglish_ExecutableField5,layout::Career_CompetitionPointersEnglish_RuntimeField14+16);return g;
}
World master() {
    auto world=bmh::test::world12::complete_world();uint32_t player=1000000,club=2000000;
    for(auto& [id,c]:world.clubs)for(size_t r=0;r<20;++r){if(r>=11){PlayerRecord p;p.id={player++};p.attributes={"Reserve",55u|(65u<<7)|(75u<<14),uint32_t(c.country)<<27,1};world.players[p.id]=p;c.roster[r]={p.id,uint16_t(r<17?2:0)};}world.players.at(c.roster[r].player).attributes.w364=uint32_t(r==0||r==11?0:r<5?1:r<9?2:3);}
    for(uint8_t country=5;country<32;++country)for(int i=0;i<7;++i){ClubRecord c;c.id={club++};c.country=Country(country);c.name="Reserve club";c.crest={c.id.value};world.crests[c.crest]={"W/TEST.VGA",0};world.clubs[c.id]=c;}
    for(uint8_t country=0;country<62;++country){ClubRecord c;c.id={club++};c.country=Country(country);c.kind=ClubKind::national_team;c.name="National team";c.crest={c.id.value};world.crests[c.crest]={"W/TEST.VGA",0};world.clubs[c.id]=c;}
    world.validate();return world;
}
CareerSnapshot initialized(const World& master,GameScope scope,const GameState& ref,const LeImage& de,const LeImage& en,uint8_t mode=3,uint8_t duration=5,uint8_t difficulty=3,uint8_t local_league=0) {
    CareerSnapshot state;auto& w=state.world;w=select_world_scope(master,scope);w.players_initialized=true;w.markets=CareerMarkets{};w.free_players=CareerPlayerPools{};
    InitialStrengthRanges ranges;for(auto& range:ranges)range={25,75};GameRandom game(17);WatcomRandom random(91);
    CareerSettings settings{mode,difficulty,duration,uint8_t(scope.germany+scope.england),false};
    for(auto country:{Country::germany,Country::england})if(scope.domestic_enabled(country)){const auto league=w.catalogue.starting_league(country,local_league);const uint8_t seat=uint8_t(w.managers.size());
        add_initial_manager(w,{seat,w.membership[league.value].clubs[0],league,"Manager "+std::to_string(seat),seat,1},settings,ranges,ref,country==Country::germany?de:en,game,random);}
    state.results=SeasonResults::empty(w.catalogue);state.game_seed=game.seed();state.watcom_seed=random.seed();return state;
}
SeasonCalendar calendar(const Catalogue& c){SeasonCalendar result;for(const auto& l:c.leagues)for(uint8_t r=1;r<=l.rounds();++r)result.leagues.push_back({uint16_t(r*7),l.id,r});result.validate(c);return result;}
owner_app::ManagerActionOwner* changed_owner=nullptr;
void interrupt(int){draw_hook=nullptr;++changed_owner->revision;}
}
int main()try {
    const auto data=master();const auto ref=reference();const auto de=source(false),en=source(true);size_t scopes=0;
    for(int mask=1;mask<8;++mask){const GameScope scope{bool(mask&1),bool(mask&2),bool(mask&4)};if(!scope.club_play())continue;
        auto state=initialized(data,scope,ref,de,en),repeat=state;const auto before=state;const auto schedule=calendar(state.world.catalogue);owner_app::ManagerActionOwner owner{71,0},other=owner;
        const auto outcome=owner_app::prepare_initial_competitions(state,owner,{71,0},schedule,{de,en},ref);
        owner_app::prepare_initial_competitions(repeat,other,{71,0},schedule,{de,en},ref);
        check(state==repeat&&owner.revision==1&&outcome.game_draws>0&&outcome.watcom_draws>0,"initial competition setup changed across repeat or lost revision/RNG");
        check(state.world.initial_competitions&&state.calendar&&state.calendar->schedule==schedule&&state.calendar->next_day==0,"initial competitions lost fields/calendar boundary");
        const auto& fields=*state.world.initial_competitions;std::set<ClubId> entrants;
        for(size_t k=0;k<3;++k)for(auto id:fields.fields[k])check(entrants.insert(id).second&&state.world.clubs.at(id).kind==ClubKind::club,"duplicate/non-club in European field");
        check(entrants.size()==120&&fields.national_field.size()==(scope.international?4u:0u),"European/national field dimensions differ");
        if(!scope.germany)for(auto id:entrants)check(state.world.clubs.at(id).country!=Country::germany,"disabled German clubs leaked into Europe");
        check(outcome.domestic_draws.size()==size_t(scope.germany+2*scope.england),"country-owned initial draw notices missing");
        check(read_save(make_save(state))==state,"initial competition snapshot lost fields/history/calendar/RNG");
        const auto prepared=state;rejects([&]{owner_app::prepare_initial_competitions(state,owner,{71,1},schedule,{de,en},ref);},"initial competition setup replay accepted");check(state==prepared&&owner.revision==1,"replay mutated prepared season");
        auto broken=before;auto bad_ref=ref;bad_ref.memory.set_pointer(layout::Career_CompetitionPointersEnglish_ExecutableField5,layout::Career_CompetitionPointersEnglish_RuntimeField14);owner={71,0};
        rejects([&]{owner_app::prepare_initial_competitions(broken,owner,{71,0},schedule,{de,en},bad_ref);},"unknown CL field layout accepted");check(broken==before&&owner.revision==0,"invalid rules leaked partial preparation");
        auto missing=before;for(auto it=missing.world.clubs.begin();it!=missing.world.clubs.end();++it)if(it->second.country==Country(31)&&it->second.kind==ClubKind::club){missing.world.clubs.erase(it);break;}const auto missing_before=missing;
        rejects([&]{owner_app::prepare_initial_competitions(missing,owner,{71,0},schedule,{de,en},ref);},"incomplete European reserve pool accepted");check(missing==missing_before&&owner.revision==0,"late field error leaked lineup/random/history state");
        broken=before;changed_owner=&owner;draw_hook=interrupt;rejects([&]{owner_app::prepare_initial_competitions(broken,owner,{71,0},schedule,{de,en},ref);},"external initial season owner overwritten");draw_hook=nullptr;check(broken==before&&owner.revision==1,"interrupted initial season lost external revision or leaked candidate");
        auto service=prepared,second_service=prepared;owner={74,0};owner_app::ManagerActionOwner second_owner=owner;
        const auto sales=owner_app::initialize_first_season_manager_services(service,owner,{74,0},ref);
        owner_app::initialize_first_season_manager_services(second_service,second_owner,{74,0},ref);
        check(service==second_service&&owner.revision==1&&sales.tickets.size()==service.world.managers.size()&&sales.watcom_draws>0,"initial ticket/reset services changed across repeat or lost owner/receipt");
        check(service.game_seed==prepared.game_seed&&service.world.initial_competitions->manager_services_complete,"ticket/reset stage changed G or lost completion marker");
        for(const auto& manager:service.world.managers){check(manager.career->season_services&&manager.career->season_services->tickets_sold>0&&manager.career->season_services->ticket_income>0,"initial tickets missing");
            check(manager.career->season_services->strength_history_count==0&&manager.match_state->goal_notices==0&&manager.match_state->tax_policy==0,"initial seasonal flags/history were not reset");
            for(const auto& row:manager.tactics->backing->scalar_rows)check(row[6]==0&&row[15]>=20&&row[15]<=50,"initial tactical reset/familiarity not retained");}
        check(read_save(make_save(service))==service,"ticket/reset state lost on snapshot roundtrip");
        const auto service_done=service;rejects([&]{owner_app::initialize_first_season_manager_services(service,owner,{74,1},ref);},"ticket/reset services replay accepted");check(service==service_done&&owner.revision==1,"ticket/reset replay mutated state");
        auto interrupted=prepared;owner={75,0};changed_owner=&owner;draw_hook=interrupt;
        rejects([&]{owner_app::initialize_first_season_manager_services(interrupted,owner,{75,0},ref);},"changed ticket/reset owner overwritten");draw_hook=nullptr;
        check(interrupted==prepared&&owner.revision==1,"interrupted ticket/reset stage leaked candidate");
        auto malformed=service;malformed.world.managers[0].career->season_services->advertising_contracts[0][0]=1;rejects([&]{malformed.validate();},"duplicate sponsor encoding accepted");
        auto bytes=encode_world(before.world);auto old=bmh::cli::parse_json(std::string(bytes.begin(),bytes.end()));old.object()->at("version")=12;const auto text=bmh::cli::dump_json(old);const auto previous=decode_world(Bytes(text.begin(),text.end()));
        check(!previous.initial_competitions&&!previous.managers[0].career->season_records,"older world invented initial fields/history");++scopes;
        CareerSnapshot old_snapshot;old_snapshot.world=previous;old_snapshot.results=SeasonResults::empty(previous.catalogue);
        rejects([&]{AiMatchProjection::create_for_humans(old_snapshot,{previous.managers[0].club},ref,{3,3,false,true});},"missing historical state was silently regenerated for scoring");
    }
    for(uint8_t mode:{1,2,3})for(uint8_t duration:{1,5})for(uint8_t difficulty:{0,9}) {
        auto state=initialized(data,{false,true,true},ref,de,en,mode,duration,difficulty,4);const auto dates=calendar(state.world.catalogue);owner_app::ManagerActionOwner owner{81,0};
        owner_app::prepare_initial_competitions(state,owner,owner,dates,{de,en},ref);
        const auto sales=owner_app::initialize_first_season_manager_services(state,owner,owner,ref);const auto& m=state.world.managers[0];
        check(sales.watcom_draws>=25&&m.career->season_services->tickets_sold>0,"lower-English ticket/reset stage omitted required draws/sales");
        if(difficulty==0)check(m.finance->balance==50000000,"difficulty-one fixed capital changed during initial sales");
        if(duration==1)for(const auto& row:m.career->season_services->advertising_offers)for(auto byte:row)check(byte==0,"one-year career invented advertising offers");
        check(m.career->sponsors[0].months==uint8_t(mode==1||duration==1),"starting sponsor condition changed during seasonal reset");
        check(read_save(make_save(state))==state,"alternate mode/duration/difficulty service snapshot lost state");
    }
    std::cout<<scopes<<" initial competition scopes: 120 unique European clubs, national entropy/scope, source layout, history/snapshot/calendar and atomic rollback passed\n";
}catch(const std::exception& error){draw_hook=nullptr;std::cerr<<error.what()<<'\n';return 1;}
