// SPDX-License-Identifier: GPL-3.0-only
#include "world12_fixture.hpp"
#include "fixture_tables.hpp"
#include "game/lineup.hpp"
#include "game/world12/manager_tactics.hpp"
#include "game/world12/desktop_projection.hpp"
#include "ui/world12_readonly.hpp"
#include <iostream>
using namespace bmh;using namespace bmh::world12;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class F>void rejects(F action,const char* message){bool rejected=false;try{action();}catch(const std::exception&){rejected=true;}check(rejected,message);}
GameState reference(){
    GameState g;g.club_ids.resize(445);g.team_flags.resize(445);g.team_morale.resize(445);g.formations.resize(0x580);g.manager_records.resize(4*0xb0c);g.manager_ratings.resize(4*60);g.tactics.resize(4*0x840);g.manager_options.resize(20);for(size_t m=0;m<4;++m)g.manager_options[m*5]=1;
    g.memory.add_block(layout::Desktops,4*0x360);for(auto address:{layout::PlainBackground,layout::SolidPanelBackground,layout::TextGradientDisabled})g.memory.add_block(address,1);
    return g;
}
CareerSnapshot fixture(const GameState& ref,LeagueId selected){
    CareerSnapshot s;s.world=bmh::test::world12::complete_world();s.world.players_initialized=true;s.world.free_players.emplace();s.world.markets.emplace();s.world.settings=CareerSettings{3,2,5,2,false};
    const auto actor=s.world.membership[selected.value].clubs[selected.value==0?1:0];s.world.managers={{0,"First",s.world.membership[0].clubs[0],{0},0},{3,"Sparse",actor,selected,3}};WatcomRandom contracts(91);
    for(uint8_t seat:{0,3})initialize_manager_contracts(s.world,seat,ref,{3,false,false,false,2},contracts);
    for(auto& manager:s.world.managers){manager.career.emplace();manager.career->season_records.emplace();}
    s.results=SeasonResults::empty(s.world.catalogue);s.date=40;s.game_seed=17;s.watcom_seed=91;s.desktop.emplace();s.desktop->managers={{3},{0}};s.desktop->active_seat=3;
    for(size_t team=0;team<s.world.membership[selected.value].clubs.size();++team){auto& club=s.world.clubs.at(s.world.membership[selected.value].clubs[team]);club.fairplay_points=uint16_t((team%4)*256+team%7);
        for(size_t row=0;row<11;++row){auto& p=s.world.players.at(club.roster[row].player);p.attributes.w364=(p.attributes.w364&~uint32_t(0xfc))|uint32_t((team+row)%9)<<2;p.attributes.w368=(p.attributes.w368&~uint32_t(0xfc0000))|uint32_t((team*2+row)%7)<<18;}}
    const auto player=s.world.clubs.at(actor).roster[0].player;s.world.player_statistics[{actor,player}]={{700,800,900},123456,70000,80000};s.validate();return s;
}
void league_pages(const GameState& ref,const LeImage& source){
    size_t queries=0;
    for(uint32_t league=0;league<10;++league){
        auto state=fixture(ref,{league});const auto before=state;const auto actor=state.world.managers[1].club;const Image underlay(320,240);
        for(auto metric:{ui::TopScorersMetric::goals,ui::TopScorersMetric::assists,ui::TopScorersMetric::scorer})for(auto scope:{ui::TopScorersScope::league,ui::TopScorersScope::managers}){
            const ui::World12ReadonlyIdentity id{3,20,actor,state.date,8,71,1ull<<32};const auto page=ui::world12_readonly_state(state,id,{metric,scope},ref,source,underlay);
            check(page.top_scorers&&page.top_scorers->identity.manager_slot==3&&page.top_scorers->identity.league==int(league)&&page.top_scorers->explicit_teams,"readonly scorer lost sparse canonical actor or country league");
            struct Expected{ClubId club;PlayerId player;unsigned score;};std::vector<Expected> expected;
            for(auto club:state.world.membership[league].clubs){if(scope==ui::TopScorersScope::managers&&std::none_of(state.world.managers.begin(),state.world.managers.end(),[&](const auto& m){return m.club==club;}))continue;
                for(const auto& slot:state.world.clubs.at(club).roster)if(!slot.empty()){const auto& p=state.world.players.at(slot.player).attributes;const unsigned goals=(p.w364>>2)&63,assists=(p.w368>>18)&63;const auto score=metric==ui::TopScorersMetric::goals?goals:metric==ui::TopScorersMetric::assists?assists:goals+assists;if(score)expected.push_back({club,slot.player,score});}}
            std::stable_sort(expected.begin(),expected.end(),[](const auto& a,const auto& b){return a.score>b.score;});if(expected.size()>10)expected.resize(10);
            const auto& rows=page.top_scorers->projection.rows;check(rows.size()==expected.size(),"readonly scorers changed Source top-ten limit");
            for(size_t i=0;i<rows.size();++i)check(page.clubs.at(rows[i].team)==expected[i].club&&page.players.at(rows[i].player)==expected[i].player&&rows[i].score==expected[i].score&&rows[i].rank==int(i+1),"readonly scorer changed Source metric/tie/physical traversal/global identities");++queries;
        }
        const ui::World12ReadonlyIdentity id{3,25,actor,state.date,8,71,1ull<<32};const auto page=ui::world12_readonly_state(state,id,{},ref,source,underlay);check(page.records&&page.records->explicit_teams&&page.records->fair_play_rows.size()==state.world.membership[league].clubs.size(),"readonly fairplay omitted real18/20/10 members");
        std::vector<int16_t> order;std::vector<uint32_t> points;const auto& clubs=state.world.membership[league].clubs;for(size_t i=0;i<clubs.size();++i){const auto cards=state.world.clubs.at(clubs[i]).fairplay_points;order.push_back(int16_t(i));points.push_back(uint32_t((int16_t(cards)>>8)*4+(cards&255)));}quicksort_by_key(order.data(),points.data(),int(order.size()),true);
        for(size_t i=0;i<order.size();++i)check(page.clubs.at(page.records->fair_play_rows[i].team)==clubs[order[i]]&&page.records->fair_play_rows[i].points==int(points[i]),"readonly fairplay changed Source card arithmetic/quicksort/global identities");
        check(state==before&&read_save(make_save(state))==state,"readonly report changed wide player statistics, saved state or RNG");
        auto bad=id;++bad.date;rejects([&]{ui::world12_readonly_state(state,bad,{},ref,source,underlay);},"readonly accepted another date");bad=id;bad.manager=2;rejects([&]{ui::world12_readonly_state(state,bad,{},ref,source,underlay);},"readonly accepted an absent actor");
    }
    std::cout<<queries<<" top-ten country/metric/scope queries and10Source fairplay tables preserve physical ties,18/20/10members, wide global IDs/state/RNG\n";
}
}
int main()try{set_fixture_tables(bmh::test::authored_fixture_tables());const auto ref=reference();const auto source=LeImage::from_bytes(layout::GermanDataSegmentBegin,Bytes(0x20000));league_pages(ref,source);return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
