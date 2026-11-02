// SPDX-License-Identifier: GPL-3.0-only
#include "world12_fixture.hpp"
#include "app/world12_manager.hpp"
#include "game/world12/manager_creation.hpp"
#include "game/world12/match_projection.hpp"
#include "game/world12/codec.hpp"
#include "game/world12/save.hpp"
#include "cli/json.hpp"
#include <algorithm>
#include <set>
#include <iostream>
#include <stdexcept>
namespace {
using namespace bmh;using namespace bmh::world12;namespace owner_app=bmh::app::world12;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class Function>void rejects(Function f,const char* message){bool bad=false;try{f();}catch(const std::exception&){bad=true;}check(bad,message);}
// Preserve the boundary preference under test while omitting unrelated fields
// that no version26/28 writer could have emitted.
void omit_later_records(cli::Json& root) {
    root.object()->erase("annual_awards_completed");root.object()->erase("amateur_slots");root.object()->erase("european_campaigns");
    for(auto& manager:std::get<cli::Json::Array>(root.object()->at("managers").value)) {
        auto* career=manager.object()->at("career").object();
        for(const char* name:{"annual_records","cup_titles","strengths_display_word","pairings_display_byte","table_display_word","other_games_view"})career->erase(name);
        const auto services=career->find("season_services");if(services!=career->end()&&services->second.object())services->second.object()->erase("promotion_tail_flags");
    }
}
World fresh() {
    auto w=bmh::test::world12::complete_world();uint32_t player=1000000;
    for(auto& [id,c]:w.clubs){for(size_t r=0;r<20;++r){if(r>=11){PlayerRecord p;p.id={player++};p.attributes={"Reserve",55u|(65u<<7)|(75u<<14),uint32_t(c.country)<<27,1};w.players[p.id]=p;c.roster[r]={p.id,uint16_t(r<17?2:0)};}
        auto& p=w.players.at(c.roster[r].player);p.attributes.w364=uint32_t(r==0||r==11?0:r<5?1:r<9?2:3);}}
    for(uint32_t i=0;i<2;++i){ClubRecord c;c.id={0xf0000000+i};c.country=i?Country::england:Country::germany;c.name="Extra club";c.crest={c.id.value};w.crests[c.crest]={"W/TEST.VGA",0};w.clubs[c.id]=c;}
    w.players_initialized=true;w.markets=CareerMarkets{};w.free_players=CareerPlayerPools{};w.validate();return w;
}
GameState reference() {
    GameState g;g.club_ids.resize(445);g.team_flags.resize(445);g.team_morale.resize(445);
    g.formations.resize(0x580);g.tactics.resize(4*0x840);g.manager_records.resize(5*0xb0c);g.manager_options.resize(20);g.manager_ratings.resize(4*60);
    const uint8_t depths[]={0,5,5,5,5,10,10,10,10,17,17},lanes[]={4,1,3,5,7,1,3,5,7,3,5};
    for(size_t m=0;m<4;++m){g.manager_options[m*5]=1;for(size_t mode=0;mode<2;++mode)for(size_t f=0;f<8;++f)for(size_t r=0;r<11;++r){const auto at=m*0x160+mode*0xb0+f*22+r;g.formations[at]=depths[r];g.formations[at+11]=lanes[r];}}
    g.memory.add_block(layout::EnvironmentYouthTeams,4*0x24);return g;
}
LeImage image() {
    auto exe=LeImage::from_bytes(layout::GermanDataSegmentBegin,Bytes(0x6000));
    const auto byte=[&](uint32_t at,uint8_t value){exe.patch(at,&value,1);};
    for(uint32_t league=0;league<4;++league){byte(layout::Career_BuildStadium_RuntimeField4+league,uint8_t(5+league));for(uint32_t i=0;i<22;++i)byte(layout::Career_BuildStadium_RuntimeField+league*0x17+i,uint8_t(1+league));}
    for(uint32_t i=0;i<22;++i){byte(layout::Career_BuildStadium_RuntimeField2+i*5,2);byte(layout::Career_BuildStadium_RuntimeField3+i*5,5);}
    return exe;
}
InitialStrengthRanges ranges(){InitialStrengthRanges r;for(size_t i=0;i<r.size();++i)r[i]={uint8_t(20+i),uint8_t(60+i)};return r;}
void reserve(LeImage& exe,size_t ordinal){const uint8_t data[]={1,0x40};exe.patch(layout::CompetitionCalendar+uint32_t(ordinal*2),data,2);}
owner_app::ManagerActionOwner* live_owner=nullptr;
void interrupt(int){draw_hook=nullptr;++live_owner->revision;}
}
int main()try {
    const auto master=fresh();const auto ref=reference();const auto rules=image();const auto strengths=ranges();size_t cases=0;
    for(bool english:{false,true})for(uint32_t tier=0;tier<(english?4u:5u);++tier)for(uint8_t duration:{0,1,5})for(uint8_t level:{0,1,2}) {
        auto w=master;const auto country=english?Country::england:Country::germany;const LeagueId target{(english?5u:0u)+tier};
        const auto chosen=w.membership[english?5:0].clubs[0];const auto original_players=w.players;auto country_rules=rules;
        // Force a reserved top position in a player-visible initial league.
        reserve(country_rules,duration<=1?1:9);
        CareerSettings settings{3,3,duration,1,false};InitialManagerChoice choice{0,chosen,target,"Manager",0,level};
        GameRandom game(17);WatcomRandom random(91);owner_app::ManagerActionOwner owner{71,2};
        owner_app::add_initial_manager(w,owner,{71,2},choice,settings,strengths,ref,country_rules,game,random);
        check(w.managers.size()==1&&w.managers[0].career&&w.managers[0].finance&&w.managers[0].tactics&&w.settings==settings&&owner.revision==3,"manager confirmation omitted initialized state/revision");
        check(w.clubs.at(chosen).league==target&&w.managers[0].club==chosen,"manager lost stable club identity during placement");
        if(tier==0)check(w.membership[target.value].clubs[duration<=1?2:10]==chosen,"reserved top place was not skipped");
        for(const auto& [id,p]:w.players)check(p.attributes.name==original_players.at(id).attributes.name&&p.id==id,"manager strength/placement changed player identity");
        const auto& state=*w.managers[0].career;int normalized=int(tier);if(normalized>2)--normalized;
        check(state.stadium.stand_capacity[0]==5+normalized+(duration==1)&&state.stadium.capacity_sum==8*state.stadium.stand_capacity[0],"stadium capacity did not use supplied source tables/duration");
        check(state.environment.youth[0].strength==82-15*std::min(normalized,2)&&state.environment.youth[0].position==6&&state.environment.monthly[0]==0&&state.environment.monthly[3]==5000,"initial youth or staff budgets differ from Source");
        check(state.sponsors[0].months==(duration==1?1:0),"initial sponsor condition differs from Source");
        check(decode_world(encode_world(w))==w,"complete manager state lost on world roundtrip");
        CareerSnapshot snapshot;snapshot.world=w;snapshot.results=SeasonResults::empty(w.catalogue);snapshot.game_seed=game.seed();snapshot.watcom_seed=random.seed();
        check(read_save(make_save(snapshot))==snapshot,"complete manager state lost on snapshot roundtrip");
        auto repeated=w;auto changed=choice;changed.seat=1;rejects([&]{owner_app::add_initial_manager(repeated,owner,{71,3},changed,settings,strengths,ref,country_rules,game,random);},"manager confirmation replay accepted");
        ++cases;
    }
    for(bool hidden_club:{false,true}){auto world=master;const auto before=world;GameRandom game(17);WatcomRandom random(91);const auto club=world.membership[hidden_club?9:5].clubs[0];InitialManagerChoice choice{0,club,{hidden_club?5u:9u},"Hidden",0,1};
        rejects([&]{add_initial_manager(world,choice,{3,2,5,1,false},strengths,ref,rules,game,random);},"fresh human manager entered the hidden English fifth league");check(world==before&&game.calls()==0&&random.calls()==0,"hidden league refusal changed world/RNG");}
    auto mixed=master;GameRandom mg(17);WatcomRandom mw(91);owner_app::ManagerActionOwner owner{72,0};CareerSettings settings{1,6,1,4,false};
    const ClubId extra{0xf0000000};const auto donated=master.clubs.at(master.membership[0].clubs[1]).roster;
    for(uint8_t seat=0;seat<4;++seat){const LeagueId league{seat==0?0u:seat==1?5u:seat==2?3u:8u};const auto club=seat==0?extra:mixed.membership[seat==2?0:5].clubs[0];
        InitialManagerChoice choice{seat,club,league,"Manager "+std::to_string(seat),seat,uint8_t(seat%3)};
        owner_app::add_initial_manager(mixed,owner,{72,seat},choice,settings,strengths,ref,rules,mg,mw);}
    check(mixed.managers.size()==4&&owner.revision==4&&mixed.clubs.at(extra).roster[0].player.valid(),"mixed-country manager creation lost extra club/squad");
    std::set<PlayerId> before_roster,after_roster;for(const auto& row:donated)if(!row.empty())before_roster.insert(row.player);for(const auto& row:mixed.clubs.at(extra).roster)if(!row.empty())after_roster.insert(row.player);
    check(before_roster==after_roster,"extra club did not inherit exactly the displaced squad");
    auto source=reference();auto career=*mixed.managers[0].career;const PlayerId wide{0xf1000000};career.poach_attempts[0]=wide;
    install_manager_career_state(source,3,career,{wide});check(manager_career_state_from_legacy(source,3,{wide})==career,"career bridge lost numeric fields or wide poaching identity");
    const auto untouched=source.manager_records;rejects([&]{install_manager_career_state(source,3,career,{});},"unmapped poaching identity accepted");check(source.manager_records==untouched,"failed reference resolution changed raw manager record");
    CareerSnapshot history;history.world=mixed;history.results=SeasonResults::empty(mixed.catalogue);
    const auto poached=mixed.clubs.at(mixed.membership[10].clubs[0]).roster[0].player;history.world.managers[0].career->poach_attempts[0]=poached;
    auto projected=AiMatchProjection::create_for_humans(history,{mixed.managers[0].club},ref,{1,6,false,true});
    auto updated=history.world;projected.commit_match_state(updated);check(updated==history.world,"human projection lost initial facilities/environment or poaching reference");
    {
        auto focused=history;focused.world.managers[0].career->camp_focus=std::array<uint8_t,4>{3,6,9,9};
        auto workspace=AiMatchProjection::create_for_humans(focused,{focused.world.managers[0].club},ref,{1,6,false,true});
        for(size_t i=0;i<4;++i)check(workspace.state.manager_records[0xc1+i]==(*focused.world.managers[0].career->camp_focus)[i],"camp focus did not install in its actual manager record");
        workspace.commit_match_state(focused.world);
        check(decode_world(encode_world(focused.world))==focused.world,"nonzero camp focus failed current codec/projection roundtrip");
        auto invalid=focused.world;(*invalid.managers[0].career->camp_focus)[0]=16;
        rejects([&]{encode_world(invalid);},"invalid camp focus byte accepted");
        const auto bytes=encode_world(focused.world);auto old=cli::parse_json(std::string(bytes.begin(),bytes.end()));old.object()->at("version")=26;omit_later_records(old);
        for(auto& manager:std::get<cli::Json::Array>(old.object()->at("managers").value)){auto* career=manager.object()->at("career").object();career->erase("stadium_curve_flags");}
        auto text=cli::dump_json(old);rejects([&]{decode_world(Bytes(text.begin(),text.end()));},"schema26 silently discarded saved camp focus");
        for(auto& manager:std::get<cli::Json::Array>(old.object()->at("managers").value))manager.object()->at("career").object()->erase("camp_focus");
        text=cli::dump_json(old);focused.world=decode_world(Bytes(text.begin(),text.end()));
        check(!focused.world.managers[0].career->camp_focus,"schema26 invented known camp preferences");
        auto unknown=AiMatchProjection::create_for_humans(focused,{focused.world.managers[0].club},ref,{1,6,false,true});unknown.commit_match_state(focused.world);
        check(!focused.world.managers[0].career->camp_focus,"ordinary projection initialized unknown old camp preferences");
    }
    {
        auto extended=history;auto& career=*extended.world.managers[0].career;career.stadium_curve_flags=0x9f;extended.world.managers[0].match_state->staff_ratings[5]=uint16_t(int16_t(-120));
        auto p=AiMatchProjection::create_for_humans(extended,{extended.world.managers[0].club},ref,{1,6,false,true});
        check(p.state.memory.u8(layout::Flags+1)==0x9f&&uint16_t(p.state.manager_records[0x8b4]|uint16_t(p.state.manager_records[0x8b5])<<8)==uint16_t(int16_t(-120)),"stadium curve flags/discount input lost canonical seat or signed word");
        p.state.memory.set8(layout::Flags+1,0x25);p.state.manager_records[0x8b4]=0x40;p.state.manager_records[0x8b5]=6;p.commit_match_state(extended.world);
        check(extended.world.managers[0].career->stadium_curve_flags==0x25&&extended.world.managers[0].match_state->staff_ratings[5]==1600&&decode_world(encode_world(extended.world))==extended.world,"stadium preferences/discount failed capture and codec roundtrip");
        const auto bytes=encode_world(extended.world);auto old=cli::parse_json(std::string(bytes.begin(),bytes.end()));old.object()->at("version")=28;omit_later_records(old);auto text=cli::dump_json(old);
        rejects([&]{decode_world(Bytes(text.begin(),text.end()));},"schema28 silently discarded known stadium preferences");
        for(auto& manager:std::get<cli::Json::Array>(old.object()->at("managers").value)){auto* c=manager.object()->at("career").object();c->erase("stadium_curve_flags");}
        text=cli::dump_json(old);extended.world=decode_world(Bytes(text.begin(),text.end()));
        auto unknown=AiMatchProjection::create_for_humans(extended,{extended.world.managers[0].club},ref,{1,6,false,true});unknown.commit_match_state(extended.world);
        check(!extended.world.managers[0].career->stadium_curve_flags,"ordinary projection invented older stadium preferences/discount input");
    }
    auto changed_settings=history.world;++changed_settings.settings->difficulty;const auto changed_before=changed_settings;
    rejects([&]{projected.commit_match_state(changed_settings);},"projection ignored external career policy change");check(changed_settings==changed_before,"projection overwrote changed career policy");
    rejects([&]{AiMatchProjection::create_for_humans(history,{mixed.managers[0].club},ref,{3,6,false,true});},"human projection ignored persisted career policy");
    auto fresh_world=master;const auto before=fresh_world;GameRandom game(17);WatcomRandom random(91);owner={73,5};
    InitialManagerChoice first{0,fresh_world.membership[0].clubs[0],{0},"First",0,1};
    live_owner=&owner;draw_hook=interrupt;rejects([&]{owner_app::add_initial_manager(fresh_world,owner,{73,5},first,{3,2,5,1,false},strengths,ref,rules,game,random);},"changed creation owner overwritten");draw_hook=nullptr;
    check(fresh_world==before&&game.calls()==0&&random.calls()==0&&owner.revision==6,"interrupted creation leaked placement/strength/facilities");
    owner={73,5};auto short_rules=LeImage::from_bytes(layout::GermanDataSegmentBegin,Bytes(0x3000));
    rejects([&]{owner_app::add_initial_manager(fresh_world,owner,{73,5},first,{3,2,5,1,false},strengths,ref,short_rules,game,random);},"missing facility rules accepted");
    check(fresh_world==before&&game.calls()==0&&random.calls()==0&&owner.revision==5,"late facility failure leaked creation state");
    std::cout<<cases<<" German/English full manager confirmations, reserved visible-league placements and hidden-fifth refusals, four mixed seats, extra-club squads, numeric bridges, wide poaching refs and atomic rollback passed\n";
}catch(const std::exception& error){draw_hook=nullptr;std::cerr<<error.what()<<'\n';return 1;}
