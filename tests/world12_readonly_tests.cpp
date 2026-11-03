// SPDX-License-Identifier: GPL-3.0-only
#include "world12_fixture.hpp"
#include "world12_national_fixture.hpp"
#include "game/world12/national_coach.hpp"
#include "game/world12/national_setup.hpp"
#include "game/world12/national.hpp"
#include "fixture_tables.hpp"
#include "game/lineup.hpp"
#include "game/world12/manager_tactics.hpp"
#include "game/world12/desktop_projection.hpp"
#include "game/world12/prediction.hpp"
#include "game/world12/balance_planning.hpp"
#include "game/world12/codec.hpp"
#include "ui/world12_readonly.hpp"
#include "ui/world12_statistics.hpp"
#include "ui/world12_readonly_capabilities.hpp"
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
void balance_pages(const GameState& ref,const LeImage& source){
    for(uint32_t league:{0u,5u,8u}){
        auto state=fixture(ref,{league});auto& actor=state.world.managers[1];
        actor.finance->balance=-123456;actor.finance->income=700;actor.finance->expenses=1800;actor.finance->bonus_paid=91;
        actor.career->season_services.emplace();actor.career->season_services->ticket_income=999;actor.career->season_services->total_ticket_income=400;
        actor.career->training_plan=17;actor.match_state->attendance.emplace();auto& gates=*actor.match_state->attendance;gates.league_home_count=2;gates.spectators[0]=20000;gates.spectators[1]=30000;
        state.world.economy.emplace();auto& economy=state.world.economy->managers[3];economy.monthly.emplace();economy.depreciation=100;economy.interest_income=600;economy.overdraft_interest=1700;
        for(size_t i=0;i<15;++i)economy.monthly->counters[i]=uint32_t(100+i);
        state.validate();const auto before=state;const ui::World12ReadonlyIdentity id{3,28,actor.club,state.date,8,71,1ull<<32};
        const auto page=ui::world12_readonly_state(state,id,{},ref,source,Image(320,240));check(bool(page.balance),"balance lacks Original DTO");const auto& actual=*page.balance;
        const std::array<int32_t,18> expected={102,103,104,400,105,600,700,292,107,108,109,110,111,112,113,114,1700,1800};
        for(size_t row=0;row<18;++row)check(actual.draft.rows[row]==expected[row],"balance booked row differs from its real canonical owner");
        double total=-100;for(size_t row=0;row<18;++row)total+=expected[row]*(row<7?1:-1);
        check(actual.draft.total==total&&actual.draft.depreciation==100&&actual.draft.view==BalanceView::so_far,"balance total/depreciation differs from Source so_far");
        check(actual.draft.rows[kBalanceLeagueHomeGames]==int(state.world.membership[league].clubs.size())-1&&actual.draft.rows[kBalanceTicketPrice]==17&&actual.draft.rows[kBalanceGateSoFar]==25000&&actual.draft.rows[kBalanceAccount]==-123456,"balance lost actual20club count/ticket/attendance/account");
        check(!actual.actuals_only&&actual.row_available[kBalanceOtherHomeGames]&&actual.row_available[kBalanceAverageGate]&&actual.capability_notice.empty(),"balance planning controls are unavailable");
        check(state==before&&read_save(make_save(state))==state,"balance changed canonical career or RNG");
        auto p=project_manager_desktop(state,ref,state.world.membership[league].clubs);auto& local=p.state;auto draft=actual.draft;
        cycle_balance_view(local,draft);check(draft.view==BalanceView::real&&draft.rows[kBalanceLeagueHomeGames]==int(state.world.membership[league].clubs.size())-1,"country-aware real balance home count differs");
        check(edit_balance_plan(local,draft,kBalanceOtherHomeGames,4)==BalanceEditStatus::changed&&edit_balance_plan(local,draft,kBalanceTicketPrice,20)==BalanceEditStatus::changed,"editable balance inputs refused");
        cycle_balance_view(local,draft);check(draft.view==BalanceView::future,"future balance view missing");
        check(edit_balance_plan(local,draft,0,123456)==BalanceEditStatus::changed,"future income adjustment refused");
        local.memory.add_block(layout::PlanTicketPrice+uint32_t(draft.manager),1);(void)close_balance_plan(local,draft);const auto stored=balance_plan_from_game(local,draft.manager);
        auto planned=state;planned.world.economy->managers[3].balance_plan=stored;check(read_save(make_save(planned))==planned&&decode_world(encode_world(planned.world))==planned.world,"balance plan lost its save/world roundtrip");
        const auto reopened=ui::world12_readonly_state(planned,id,{},ref,source,Image(320,240));check(reopened.balance->draft.view==draft.view&&reopened.balance->draft.adjustments==draft.adjustments&&reopened.balance->draft.rows[kBalanceOtherHomeGames]==4&&reopened.balance->draft.rows[kBalanceTicketPrice]==20,"reopened plan lost edits or view");
        auto reset=reopened.balance->draft;reset_balance_plan(local,reset);check(reset.adjustments==std::array<int32_t,18>{},"actual-values reset retained annual adjustments");
        auto bytes=encode_world(planned.world);auto old=bmh::cli::parse_json(std::string(bytes.begin(),bytes.end()));old.object()->at("version")=42;const auto json=bmh::cli::dump_json(old);rejects([&]{decode_world(Bytes(json.begin(),json.end()));},"older schema silently lost an edited plan");
        auto unknown=state;unknown.world.economy->managers[3].depreciation.reset();rejects([&]{ui::world12_readonly_state(unknown,id,{},ref,source,Image(320,240));},"balance silently supplied missing depreciation");
        unknown=state;unknown.world.managers[1].match_state->attendance.reset();rejects([&]{ui::world12_readonly_state(unknown,id,{},ref,source,Image(320,240));},"balance silently supplied missing attendance");
    }
    std::cout<<"Actual balance18booking owners, DE/ENhomecounts, known attendance/ticket/account, unknown plan/depreciation and complete save/RNG preservation passed\n";
}
void statistics_pages(const GameState& ref,LeImage source){
    const uint8_t table_flags[]={3,1,2},selectors[]={0,1,2};source.patch(layout::TableFlags,table_flags,3);source.patch(layout::SeriesSelector,selectors,3);
    for(uint32_t index=0;index<9;++index){const auto id=uint8_t(index);source.patch(layout::StatisticsRankings_FormatError_RuntimeField+index,&id,1);}
    auto state=fixture(ref,{5});auto& world=state.world;world.annual_title_archive.emplace();state.transfers.emplace();
    for(uint8_t country=0;country<62;++country){ClubRecord nation;nation.id={0xff000000u+country};nation.country=Country(country);nation.kind=ClubKind::national_team;nation.name="Authored nation "+std::to_string(country);nation.crest=world.clubs.begin()->second.crest;world.clubs[nation.id]=nation;}
    const auto actor=world.managers[1].club;const auto player=world.clubs.at(actor).roster[0].player;const auto other=world.membership[0].clubs[2];
    state.transfers->history[3]={{player,other,(123u<<19)|(17u<<2)|1u},{player,{},(7u<<19)|(18u<<2)}};
    auto& actual=world.clubs.at(actor);for(size_t i=0;i<21;++i){actual.season_stats.counters[i]=uint8_t(i+1);world.managers[1].match_state->best_streaks[i]=uint8_t(i+2);}
    actual.season_stats.counters[21]=0x31;actual.season_stats.opponents[0]=other;actual.season_stats.dates[0]=17;
    for(auto& [id,club]:world.clubs)if(club.kind==ClubKind::club){const uint16_t words[]={uint16_t(10+id.value%20),30,20,4,3,2};for(size_t k=0;k<6;++k){club.season_stats.counters[30+k*2]=uint8_t(words[k]);club.season_stats.counters[31+k*2]=uint8_t(words[k]>>8);}}
    for(int round=1;round<=3;++round){const auto matches=league_fixtures(world,{5},round);for(size_t i=0;i<matches.size();++i)state.results.result(world.catalogue,{5},round,int(i))={true,uint8_t(i%3),uint8_t((i+round)%2)};}state.league_rounds[5]=3;
    for(size_t i=0;i<72;++i)world.managers[1].match_state->timing_stats[i]=uint8_t(i);
    state.validate();const auto before=state;const ui::World12ReadonlyIdentity owner{3,23,actor,state.date,8,71,1ull<<32};const Image frame(320,240);
    auto selection=ui::World12StatisticsSelection{};check(ui::world12_statistics_state(state,owner,selection,ref,source,frame).menu.owner.manager_slot==3,"statistics parent lost sparse canonical seat");
    selection.page=0;const auto forecast=ui::world12_statistics_state(state,owner,selection,ref,source,frame);const auto expected=predict_league(state,actor,1);check(forecast.opportunities&&forecast.opportunities->ranks==expected.result.ranks&&forecast.opportunities->current_rank==expected.result.best_rank,"statistics forecast differs from real Source prediction kernel");
    selection.page=1;const auto transfers=ui::world12_statistics_state(state,owner,selection,ref,source,frame);check(transfers.transfers&&transfers.transfers->rows.size()==2&&transfers.transfers->rows[0].source_fee==1230000&&transfers.transfers->rows[0].source_date_counter==17&&transfers.transfers->rows[0].counterparty_club_name==world.clubs.at(other).name&&transfers.transfers->rows[1].special_counterparty,"statistics history lost canonical player/counterparty/source word");
    for(int country=0;country<5;++country)for(bool relative:{false,true}){selection.page=3;selection.nation=country;selection.relative=relative;const auto page=ui::world12_statistics_state(state,owner,selection,ref,source,frame);check(page.alltime&&page.alltime->rows.size()==std::min<size_t>(24,page.clubs.size()),"statistics alltime omitted actual physical members/Source limit");for(const auto& row:page.alltime->rows)check(page.alltime->club_names.at(row.team)==world.clubs.at(page.clubs.at(row.team)).name&&row.played==9,"statistics alltime lost canonical record identity");}
    selection.page=6;const auto streaks=ui::world12_statistics_state(state,owner,selection,ref,source,frame);check(streaks.series&&streaks.series->explicit_league_size==20&&streaks.series->current_streaks[20]==21&&streaks.series->manager_best_streaks[20]==22&&streaks.series->records[0].opponent_name==world.clubs.at(other).name,"statistics series lost actual English size/streak/opponent records");
    for(size_t col=0;col<3;++col){LeagueTable native;native.size=20;for(int round=1;round<=3;++round){const auto matches=league_fixtures(world,{5},round);for(size_t m=0;m<matches.size();++m){const auto& result=state.results.result(world.catalogue,{5},round,int(m));const int clubs[]={int(std::find(world.membership[5].clubs.begin(),world.membership[5].clubs.end(),matches[m].home)-world.membership[5].clubs.begin()),int(std::find(world.membership[5].clubs.begin(),world.membership[5].clubs.end(),matches[m].away)-world.membership[5].clubs.begin())};const uint8_t goals[]={result.home,result.away};add_result(native,clubs,goals,table_flags[col],0);}}const auto& actual_table=streaks.series->tables[col];check(actual_table.goals_for==native.goals_for&&actual_table.goals_against==native.goals_against&&actual_table.wins==native.wins&&actual_table.defeats==native.defeats,"statistics series tables differ from Native AddResult kernel");for(int i=0;i<20;++i)check(actual_table.games(i)==native.games(i),"statistics series lost actual games denominator");}
    selection.page=7;const auto titles=ui::world12_statistics_state(state,owner,selection,ref,source,frame);check(titles.rankings&&!titles.rankings->season_known[0],"statistics historical rankings fabricated an unknown completed season");
    selection.page=8;const auto minutes=ui::world12_statistics_state(state,owner,selection,ref,source,frame);check(minutes.minutes&&minutes.minutes->source_series[3][17]==71,"statistics minute archive lost actual manager byte");
    for(int page:{2,4,5}){selection.page=page;rejects([&]{ui::world12_statistics_state(state,owner,selection,ref,source,frame);},"statistics invented a missing history/economy/national prerequisite");}
    auto known_nations=state;known_nations.world.national_campaigns.emplace();const uint8_t banks[]={14,14,17,17};source.patch(layout::WorldCupStatistics_FormatError_RuntimeField,banks,4);selection.page=4;const auto nations=ui::world12_statistics_state(known_nations,owner,selection,ref,source,frame);check(nations.world_cup&&!nations.world_cup->can_select_opponent&&std::all_of(nations.world_cup->editions.begin(),nations.world_cup->editions.end(),[](auto value){return value==0;}),"known unplayed World-Cup history fabricated podiums or selectable fixtures");for(size_t c=0;c<62;++c)check(nations.world_cup->country_names[c]=="Authored nation "+std::to_string(c)&&nations.world_cup->badge_atlases[c]=="W/TEST.VGA","World-Cup view lost actual canonical country/crest mapping");
    auto archived=state;archived.season=1;AnnualSeasonTitles year;year.season=0;for(uint32_t league:{0u,5u}){AnnualTitleRow title;title.kind=AnnualTitleKind::league;title.league={league};title.country=archived.world.catalogue.league({league}).country;title.source_category=0;title.winner=archived.world.membership[league].clubs[0];title.runner_up=archived.world.membership[league].clubs[1];year.titles.push_back(title);}archived.world.annual_title_archive->seasons.push_back(year);archived.world.annual_title_archive->unresolved_source_labels[0][1]=archived.world.clubs.at(actor).name;archived.validate();selection.page=7;const auto actual_titles=ui::world12_statistics_state(archived,owner,selection,ref,source,frame);check(actual_titles.rankings->club_names[0][0]==archived.world.clubs.at(actor).name&&actual_titles.rankings->club_name_colors[0][0]==0x7a&&actual_titles.rankings->club_names[1][0]==archived.world.clubs.at(actor).name&&actual_titles.rankings->club_name_colors[1][0]==0x5f&&!actual_titles.rankings->season_known[1],"historical titles lost country filter/canonical identity, resolved raw label by name, or invented current award");
    auto unranked=state;unranked.world.highscore_signature=0;auto high_owner=owner;high_owner.menu=26;const auto high=ui::world12_readonly_state(unranked,high_owner,{},ref,source,frame);check(high.records&&!high.records->highscore.ranked&&high.records->highscore.signature==0,"known unranked highscore invented ranked records");
    check(state==before&&read_save(make_save(state))==state,"statistics projection changed full career/seeds/wide archive");
    auto unknown=state;unknown.transfers.reset();selection.page=1;rejects([&]{ui::world12_statistics_state(unknown,owner,selection,ref,source,frame);},"statistics invented unknown transfer history");
    std::cout<<"statistics forecast/history/ten alltime variants/three Source series tables/titles/minutes preserve canonical identities and complete state; missing histories reject\n";
}

void national_pages(LeImage source){
    auto original=bmh::test::world12::national_world();original.managers.clear();original.players_initialized=false;original.settings.reset();original.markets.reset();original.free_players.reset();for(uint8_t c=0;c<62;++c)if(c!=0&&c!=2){ClubRecord nation;nation.id={3000000u+c};nation.country=Country(c);nation.kind=ClubKind::national_team;nation.name="Nation "+std::to_string(c);nation.crest=original.clubs.begin()->second.crest;original.clubs[nation.id]=nation;}
    CareerSnapshot state;state.world=select_world_scope(original,{false,false,true});auto& world=state.world;world.players_initialized=true;world.markets.emplace();world.free_players.emplace();for(auto& origin:*world.national_origins)origin.source_initialized=true;world.settings=CareerSettings{3,6,5,1,false};const ClubId team{3000002};auto ref=bmh::test::world12::national_reference();for(auto address:{layout::PlainBackground,layout::SolidPanelBackground,layout::TextGradientDisabled})ref.memory.add_block(address,1);ref.memory.add_block(layout::Desktops,5*0x360);initialize_national_coaches(world,ref);auto& coach=*std::find_if(world.national_coaches->begin(),world.national_coaches->end(),[&](const auto& record){return record.team==team;});coach.view_preferences=initial_national_view_preferences(ref,3);coach.startup=NationalStartupReceipt{Country::england,uint32_t(world.players.size()),uint32_t(world.national_origins->size())};world.managers={{3,"Sparse national",team,{},3}};activate_national_coaches(world,{3});WatcomRandom random(91);for(auto country:{Country::germany,Country::england})nominate_national_squad(world,country,0,ref,random);world.national_campaigns.emplace();world.annual_title_archive.emplace();world.economy.emplace();world.highscore_signature=0;state.results=SeasonResults::empty(world.catalogue);state.desktop.emplace();state.desktop->manager_order={3};state.desktop->matchday_number=71;state.desktop->active_seat=3;state.desktop->managers={{3}};state.watcom_seed=random.seed();state.validate();const auto before=state;const Image frame(320,240);ui::World12ReadonlyIdentity owner{3,23,team,0,8,71,1ull<<32};
    const uint8_t banks[]={14,14,17,17};source.patch(layout::WorldCupStatistics_FormatError_RuntimeField,banks,4);ui::World12StatisticsSelection selection;
    const auto parent=ui::world12_statistics_state(state,owner,selection,ref,source,frame);check(!parent.menu.available[0]&&!parent.menu.available[1]&&!parent.menu.available[2]&&!parent.menu.available[3]&&!parent.menu.available[6]&&parent.menu.available[4]&&parent.menu.available[7]&&parent.menu.available[8],"national statistics exposed unavailable club panes");check(parent.menu.owner.manager_slot==3&&!world.managers[0].career&&!world.managers[0].start_league.valid(),"national statistics parent fabricated club backing or lost sparse actor");
    for(int page:{4,7,8}){selection.page=page;const auto result=ui::world12_statistics_state(state,owner,selection,ref,source,frame);check(page==4?bool(result.world_cup):page==7?bool(result.rankings):bool(result.minutes),"national statistics lost real general/nominated facts");}
    for(int page:{0,1,2,3,6}){selection.page=page;rejects([&]{ui::world12_statistics_state(state,owner,selection,ref,source,frame);},"national statistics substituted a disabled or parked club league/history");}
    auto balance_owner=owner;balance_owner.menu=28;rejects([&]{ui::world12_readonly_state(state,balance_owner,{},ref,source,frame);},"national balance fabricated club accounts");
    ui::DeskState capabilities;capabilities.managers.resize(1);capabilities.managers[0].world_club=team;for(int kind:{0,1,2,4,5,8,9,20,23,25,26}){ui::DeskItem item;item.id=kind;item.kind=kind;capabilities.items.push_back(item);capabilities.layout_records[size_t(kind)][12]=uint8_t(kind);}world12_readonly_capabilities(state,capabilities);for(const auto& item:capabilities.items)check(item.disabled==(item.kind==0||item.kind==1||item.kind==2||item.kind==4||item.kind==9||item.kind==20||item.kind==25),"national desktop capability mask disabled real facts or exposed fake league views");owner.menu=26;check(ui::world12_readonly_state(state,owner,{},ref,source,frame).records.has_value(),"known unranked NoClub score required fabricated club accounts");owner.menu=23;auto wrong=owner;wrong.club={3000000};selection.page=-1;rejects([&]{ui::world12_statistics_state(state,wrong,selection,ref,source,frame);},"national readonly accepted another real nation's actor binding");check(state==before&&read_save(make_save(state))==state,"national general statistics mutated real squads, inactive origins, accounts, seeds or absent career");std::cout<<"NoClub sparse actor3: actual WC/title/minute/score views, missing club histories explicit, full snapshot/RNG unchanged\n";
}

}
int main()try{set_fixture_tables(bmh::test::authored_fixture_tables());const auto ref=reference();const auto source=LeImage::from_bytes(layout::GermanDataSegmentBegin,Bytes(0x20000));league_pages(ref,source);balance_pages(ref,source);statistics_pages(ref,source);national_pages(source);return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
