// SPDX-License-Identifier: GPL-3.0-only
#include "world12_fixture.hpp"
#include "game/world12/weekly_incidents.hpp"
#include "game/messages.hpp"
#include "game/world12/manager_tactics.hpp"
#include "game/world12/codec.hpp"
#include "cli/json.hpp"
#include <algorithm>
#include <map>
#include <iostream>
namespace {
using namespace bmh;using namespace bmh::world12;
void check(bool v,const char* message){if(!v)throw std::runtime_error(message);}
template<class F>void rejects(F f,const char* message){bool bad=false;try{f();}catch(const std::exception&){bad=true;}check(bad,message);}
GameState reference() {
    GameState state;state.club_ids.resize(445);state.team_flags.resize(445);state.team_morale.resize(445);
    state.formations.resize(0x580);state.manager_records.resize(4*0xb0c);state.manager_ratings.resize(4*60);
    state.tactics.resize(4*0x840);state.manager_options.resize(20);
    const uint8_t depth[]={0,5,5,5,5,10,10,10,10,17,17},lane[]={4,1,3,5,7,1,3,5,7,3,5};
    for(size_t m=0;m<4;++m){state.manager_options[m*5]=1;
        for(size_t mode=0;mode<2;++mode)for(size_t formation=0;formation<8;++formation)for(size_t row=0;row<11;++row){
            const auto at=m*0x160+mode*0xb0+formation*22+row;
            state.formations[at]=depth[row];state.formations[at+11]=lane[row];
        }
    }
    return state;
}
CareerSnapshot fixture(const GameState& rules) {
    CareerSnapshot state;auto& world=state.world;world=bmh::test::world12::complete_world();world.players_initialized=true;
    uint32_t next=0x90000000u;
    for(auto& [id,club]:world.clubs)for(size_t row=0;row<20;++row) {
        if(club.roster[row].empty()) {
            PlayerRecord player;player.id={next++};player.attributes.name="Additional "+std::to_string(player.id.value);
            world.players.emplace(player.id,player);club.roster[row]={player.id,uint16_t(row<17?2:0)};
        }
        auto& player=world.players.at(club.roster[row].player).attributes;
        player.w35c=60u|(75u<<7)|(85u<<14);player.w368=0;
        player.w364=uint32_t(row==0||row==11?0:row<5?1:row<9?2:3);
    }
    const auto de=world.membership[0].clubs[0],en=world.membership[9].clubs[0];
    world.managers={{1,"German",de,{0},1},{3,"English",en,{9},3}};
    WatcomRandom random(91);
    initialize_manager_contracts(world,1,rules,{1,false,false,false,6},random);
    initialize_manager_contracts(world,3,rules,{1,false,false,false,6},random);
    world.markets.emplace();world.free_players.emplace();world.settings=CareerSettings{1,6,5,2,false};
    state.results=SeasonResults::empty(world.catalogue);state.date=40;state.game_seed=17;state.watcom_seed=91;
    state.validate();return state;
}
CareerSnapshot prepared(GameState& rules) {
    rules.memory.add_block(layout::Savestate_Layout_ExecutableField,1);rules.memory.set8(layout::Savestate_Layout_ExecutableField,2);
    rules.memory.add_block(layout::Loans,4*0x78);rules.mailbox.assign(kMailboxEntries*kMailboxEntry,0);
    auto state=fixture(rules);state.date=0;state.world.settings->mode=3;state.world.transfer_market.emplace();
    for(auto& m:state.world.managers){auto& c=m.career.emplace();c.season_records.emplace();c.season_services.emplace();
        c.stadium.facility_values.fill(4);c.stadium.facility_values[0]=85;c.stadium.stand_condition.fill(100);
        c.stadium.facility_levels.fill(1);c.stadium.stand_capacity.fill(1);c.stadium.capacity_sum=8;
        c.sponsors[0]={100000,12};
    }
    auto& fields=state.world.initial_competitions.emplace();fields.manager_services_complete=true;
    std::vector<ClubId> clubs;for(const auto& [id,club]:state.world.clubs)clubs.push_back(id);
    size_t at=0;const size_t sizes[]={64,24,32};for(size_t k=0;k<3;++k){for(size_t n=0;n<sizes[k];++n)fields.fields[k].push_back(clubs.at(at++));fields.holders[k]=fields.fields[k][0];}
    for(uint32_t n=0;n<4;++n){ClubRecord nation;nation.id={0xe0000000u+n};nation.name="Nation "+std::to_string(n);nation.kind=ClubKind::national_team;nation.crest=state.world.clubs.begin()->second.crest;state.world.clubs[nation.id]=nation;fields.national_field.push_back(nation.id);}
    initialize_career_economy(state,rules);
    for(size_t f=0;f<6;++f){const size_t at=f*0x1c;const uint32_t price=1000000;
        for(size_t b=0;b<4;++b)state.world.markets->funds[at+b]=uint8_t(price>>(b*8));
        for(size_t m=0;m<4;++m)state.world.markets->funds[at+0x14+m*2]=2;
    }
    state.date=40;state.validate();return state;
}
}
int main()try {
    auto rules=reference();const auto initial=prepared(rules);
    TextDb texts;texts.strings.resize(TEXT_COUNT,"Synthetic weekly event ");texts.offsets.resize(TEXT_COUNT);texts.blob.assign(200,0);
    WeeklyIncidentContext context{initial.date,initial.season,{}};
    for(const auto& m:initial.world.managers)context.clubs.push_back({m.club,{}});
    auto source=initial;auto& first=source.world.managers[0];const auto first_club=first.club;first.match_state->designated_takers=true;
    first.match_state->takers[0]=source.world.clubs.at(first.club).roster[19].player;
    first.match_state->takers[1]=source.world.clubs.at(first.club).roster[18].player;
    auto desktop=AiMatchProjection::create_for_desktop(source,{first_club,source.world.managers[1].club},rules,{3,6,false,true});
    for(size_t n=0;n<2;++n){const size_t at=n?0xafe:0xafc;const auto raw=uint16_t(desktop.state.manager_records[at]|uint16_t(desktop.state.manager_records[at+1])<<8);
        check(raw&&desktop.global_player(raw-1)==first.match_state->takers[n],"sponsor row overwrote remapped designated taker");}
    desktop.commit_match_state(source.world);check(read_save(make_save(source))==source,"designated takers and masked sponsor backing failed snapshot");
    auto blob=encode_world(source.world);auto old=cli::parse_json(std::string(blob.begin(),blob.end()));(*old.object())["version"]=16;
    auto& managers=std::get<cli::Json::Array>((*old.object())["managers"].value);
    auto& career=(*managers[0].object())["career"];auto& services=(*career.object())["season_services"];
    auto& contracts=std::get<cli::Json::Array>((*services.object())["contracts"].value);
    auto& bytes=std::get<cli::Json::Array>(contracts[16].value);for(size_t b=8;b<12;++b)bytes[b]=7;
    const auto old_json=cli::dump_json(old);auto old_expected=source.world;for(auto& manager:old_expected.economy->managers)manager.monthly.reset();old_expected.transfer_market->entry40_history_known=false;check(decode_world(Bytes(old_json.begin(),old_json.end()))==old_expected,"old schema16 did not normalize duplicated taker words");

    std::map<int,uint32_t> seeds;
    auto sample=AiMatchProjection::create_for_humans(source,{first_club,source.world.managers[1].club},rules,{3,6,false,true});
    install_career_economy(sample,source.world);sample.state.texts=&texts;sample.state.memory.add_block(layout::CompetitionSelection,4);sample.state.memory.set32(layout::CompetitionSelection,3);
    for(uint32_t seed=1;seed<30000&&seeds.size()<8;++seed){auto local=sample.state;WatcomRandom w(seed);MinuteReport report;WeeklyOfferHooks find;
        find.answer=[&](const GameState&,const WeeklyOfferQuestion& q)->std::optional<bool>{seeds.try_emplace(q.kind,seed);return false;};
        manager_offers(local,0,w,report,&find);check(!report.unsupported,"weekly offer seed sampler failed");}
    check(seeds.size()==8,"seed sampler missed a supported weekly question kind");
    for(const auto& [kind,seed]:seeds)for(bool answer:{false,true}){
        auto state=source;state.watcom_seed=seed;const auto before=state;bool shown=false;
        WeeklyIncidentHooks hooks;hooks.answer=[&](const GameState&,uint8_t seat,const WeeklyOfferQuestion& q)->std::optional<bool>{check(seat==1||seat==3,"weekly question used a local seat as global actor");shown|=q.kind==kind;return answer;};
        const auto result=begin_weekly_managers(state,rules,texts,context,hooks);
        check(shown&&result.questions>0&&result.managers==2&&state.world.economy->week==3,"weekly incidents skipped questions/managers/week");
        check(state.date==before.date&&state.results==before.results&&state.calendar==before.calendar,"weekly incidents advanced calendar");
        check(state.world.managers[0].match_state->takers==before.world.managers[0].match_state->takers,"weekly advertising changed designated takers");
        check(read_save(make_save(state))==state,"weekly incidents/ads/mail failed snapshot roundtrip");
        auto retry=before;
        rejects([&]{begin_weekly_managers(retry,rules,texts,context,{});},"missing weekly decision silently declined");check(retry==before,"missing decision leaked staged incidents/RNG");
        WeeklyIncidentHooks cancel;cancel.answer=[](const GameState&,uint8_t,const WeeklyOfferQuestion&)->std::optional<bool>{return {};};
        rejects([&]{begin_weekly_managers(retry,rules,texts,context,cancel);},"cancelled weekly decision committed");check(retry==before,"cancelled weekly decision leaked state");
    }
    auto build=source;auto& economy=build.world.economy->managers[1];economy.construction_weeks[22]=1;economy.planned_stand_capacity[0]=5;economy.stand_flags[0]=0x33;
    check(read_save(make_save(build))==build,"unfinished stand targets/flags did not survive snapshot");
    const auto building_raw=encode_world(build.world);auto old_build=cli::parse_json(std::string(building_raw.begin(),building_raw.end()));(*old_build.object())["version"]=16;
    const auto old_build_json=cli::dump_json(old_build);
    rejects([&]{decode_world(Bytes(old_build_json.begin(),old_build_json.end()));},"schema16 guessed missing unfinished stand targets");
    for(int32_t seed=1;;++seed){GameRandom probe(seed);if(probe.range(0,175)>=6&&probe.range(0,175)>=6){build.game_seed=seed;break;}}
    WeeklyIncidentHooks yes;yes.answer=[](const GameState&,uint8_t,const WeeklyOfferQuestion&)->std::optional<bool>{return true;};
    begin_weekly_managers(build,rules,texts,context,yes);
    check(!build.world.economy->managers[1].construction_weeks[22]&&!build.world.economy->managers[1].planned_stand_capacity[0]&&build.world.economy->managers[1].stand_flags[0]==3&&build.world.managers[0].career->stadium.stand_capacity[0]==5,"stand completion lost future flags/capacity");
    auto stale=source;auto wrong=context;++wrong.date;rejects([&]{begin_weekly_managers(stale,rules,texts,wrong,yes);},"stale weekly relations accepted");check(stale==source,"stale weekly relations changed state");
    auto interrupted=source;interrupted.watcom_seed=seeds.begin()->second;const auto before=interrupted;bool changed=false;
    WeeklyIncidentHooks interrupt;interrupt.answer=[&](const GameState&,uint8_t,const WeeklyOfferQuestion&)->std::optional<bool>{if(!changed){++interrupted.watcom_seed;changed=true;}return false;};
    rejects([&]{begin_weekly_managers(interrupted,rules,texts,context,interrupt);},"weekly decisions overwrote external owner change");auto expected=before;++expected.watcom_seed;check(interrupted==expected,"weekly rollback lost external seed or leaked data");
    std::cout<<"Eight weekly question kinds, yes/no/cancel/missing answers, sparse actors, stand completion, taker/sponsor isolation, old schema upgrade and rollback passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
