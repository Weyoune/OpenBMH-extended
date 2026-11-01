// SPDX-License-Identifier: GPL-3.0-only
#include "app/world12_initial_season.hpp"
#include "app/world12_first_event.hpp"
#include "game/turn.hpp"
#include "game/matchday_setup.hpp"
#include "game/weekly.hpp"
#include "game/world12/codec.hpp"
#include "game/world12/manager_match_state.hpp"
#include "game/world12/manager_finance.hpp"
#include "game/world12/manager_tactics.hpp"
#include "game/career.hpp"
#include "game/season.hpp"
#include "game/calendar.hpp"
#include "game/schedule.hpp"
#include "formats/data_profile.hpp"
#include "formats/le.hpp"
#include "launch/fltd.hpp"
#include <iostream>
namespace {struct DrawBoundary {};}
int main(int argc,char** argv)try {
    using namespace bmh;using namespace bmh::world12;namespace owner_app=bmh::app::world12;
    if(argc!=4)throw std::runtime_error("usage: world12_initial_season_validate WORLD12.JSON German-reference FLTD.EXE");
    const auto master=decode_world(read_file(argv[1]));const std::filesystem::path data=argv[2];const auto profile=load_data_profile(data);install_runtime_profile(profile);
    const auto german=load_le(data/"BMH.exe"),english=bmh::launch::translate_fltd(argv[3]);install_career_rules(german);install_fixture_tables(german);uint8_t sizes[kLeagueCount];for(size_t i=0;i<kLeagueCount;++i)sizes[i]=profile.leagues[i];set_league_sizes(sizes);
    const auto db=load_database(data/"M3D.MAN",profile.counts);auto native=startup_state(german,db,read_file(data/"M3.MEN"),read_file(data/"M3.DAT"));const auto reference=native;
    CareerSnapshot state;state.world=select_world_scope(master,{true,false,true});state.results=SeasonResults::empty(state.world.catalogue);
    const auto ranges=read_initial_strength_ranges(state.world.catalogue,german,english);const auto plan=make_player_initialization_plan(master,state.world.catalogue.scope,ranges);
    GameRandom game(17),native_game(17);WatcomRandom random(91),native_random(91);MinuteReport report;
    startup_players(native,native_game,native_random);if(!new_career_before_dialog(native,native_game,native_random,report))throw std::runtime_error(report.reason);
    initialize_career_players(state.world,plan,game,random);initialize_career_markets_and_moods(state.world,reference,game,random);initialize_career_player_pools(state.world,plan,reference,false,game,random);
    native.level=3;native.projected_foreign_rule=false;native.memory.set8(layout::ManagerCount,1);native.memory.set8(layout::GameMode,3);native.memory.set8(layout::Difficulty,3);native.memory.set8(layout::CareerDuration,5);career_managers_begin(native,false);
    for(size_t row=0;row<24;++row)for(size_t at:{0x44u,0x46u}){native.tactics[row*0x58+at]=255;native.tactics[row*0x58+at+1]=15;}
    if(!career_add_manager(native,german,0,{"Initial manager",0,0,1,0},false,native_game,native_random,report))throw std::runtime_error(report.reason);
    add_initial_manager(state.world,{0,{0},{0},"Initial manager",0,1},{3,3,5,1,false},ranges,reference,german,game,random);
    career_managers_end(native,false);
    state.game_seed=game.seed();state.watcom_seed=random.seed();
    std::vector<ClubId> mapping;
    for(auto table:{TableGroup::germany,TableGroup::europe})for(auto league:state.world.catalogue.table_leagues(table))for(auto id:state.world.membership[league.value].clubs)mapping.push_back(id);
    mapping.resize(445);for(uint32_t id=164;id<445;++id)mapping[id]={id};
    const uint32_t lists[]={layout::Career_CompetitionPointersEnglish_ExecutableField25,layout::Career_CompetitionPointersEnglish_RuntimeField14,layout::Career_CompetitionPointersEnglish_ExecutableField19};
    const uint32_t counts[]={layout::Career_EuropeanClub_ExecutableField,layout::Career_EuropeanClub_ExecutableField2,layout::Career_EuropeanClub_ExecutableField3};
    std::array<std::vector<ClubId>,3> qualified;
    const auto prefix_game=native_game.calls(),prefix_random=native_random.calls();bool captured=false;
    native.memory.set8(layout::LeaguePresentationOptions+5*9,0);
    const auto pause=[&](const char* name){if(std::string(name)=="the German entrants (CompetitionWinnerScreen)")for(size_t k=0;k<3;++k)for(size_t i=0;i<native.memory.u8(counts[k]);++i)qualified[k].push_back(mapping.at(native.memory.u16(lists[k]+uint32_t(i)*2)));};
    try{career_season_setup(native,native_game,native_random,report,pause,[&](const auto&,const SeasonCupDrawEvent& event){if(event.competition==0&&event.phase==SeasonCupDrawPhase::overview){captured=true;throw DrawBoundary{};}});}catch(const DrawBoundary&){}
    if(!captured)throw std::runtime_error("Native initial domestic draw boundary not reached");
    std::vector<uint16_t> de(366),en(366);for(uint32_t i=0;i<366;++i){de[i]=german.u16(layout::CompetitionCalendar+i*2);en[i]=english.u16(layout::CompetitionCalendar+i*2);}
    const auto epoch=calendar_epoch_from_executable(german),en_epoch=calendar_epoch_from_fltd(read_file(argv[3]));
    const auto aligned=align_source_calendar(en,en_epoch.weekday,epoch.weekday,366);const auto schedule=SeasonCalendar::from_sources(state.world.catalogue,de,aligned.entries);
    owner_app::ManagerActionOwner owner{71,4};const auto result=owner_app::prepare_initial_competitions(state,owner,{71,4},schedule,{german,english},reference);
    if(state.game_seed!=native_game.seed()||state.watcom_seed!=native_random.seed()||result.game_draws!=native_game.calls()-prefix_game||result.watcom_draws!=native_random.calls()-prefix_random)throw std::runtime_error("initial competition seeds/draw counts differ from Native");
    const auto& fields=*state.world.initial_competitions;if(fields.qualified!=qualified)throw std::runtime_error("initial European qualifiers differ from Native");
    const size_t sizes_to_read[]={64,24,32};for(size_t k=0;k<3;++k)for(size_t i=0;i<sizes_to_read[k];++i)if(fields.fields[k][i]!=mapping.at(native.memory.u16(lists[k]+uint32_t(i)*2)))throw std::runtime_error("initial European field differs from Native");
    for(size_t i=0;i<4;++i)if(fields.national_field[i]!=mapping.at(native.memory.u16(layout::Field0+uint32_t(i*2))))throw std::runtime_error("initial national field differs from Native");
    const auto& cup=state.cups[0];const auto list=native.memory.pointer(layout::CupTable);for(size_t i=0;i<32;++i){const auto& f=cup.rounds[0].fixtures[i];if(f.home!=mapping.at(native.memory.u16(list+uint32_t(i*4)))||f.away!=mapping.at(native.memory.u16(list+uint32_t(i*4+2))))throw std::runtime_error("initial domestic draw differs from Native");}
    for(const auto& [id,p]:state.world.players)if(p.cards!=native.player_cards.at(id.value))throw std::runtime_error("initial competition lineup card rows differ");
    if(state.world.managers[0].career!=manager_career_state_from_legacy(native,0,{}))throw std::runtime_error("initial campaign record differs from Native");
    if(read_save(make_save(state))!=state)throw std::runtime_error("initial competitions snapshot roundtrip failed");
    const auto reset_before=native_random.calls();initialize_season_tickets(native,0,native_random);season_players(native,native_game,native_random,report);
    if(report.unsupported)throw std::runtime_error(report.reason);
    const auto services=owner_app::initialize_first_season_manager_services(state,owner,owner,reference);
    if(state.game_seed!=native_game.seed()||state.watcom_seed!=native_random.seed()||services.watcom_draws!=native_random.calls()-reset_before)throw std::runtime_error("initial manager services RNG mismatch");
    std::vector<PlayerId> players;for(size_t i=0;i<native.players.size();++i)players.push_back({uint32_t(i)});
    if(state.world.managers[0].career!=manager_career_state_from_legacy(native,0,players,true))throw std::runtime_error("initial ticket/advertising/history state differs from Native");
    if(state.world.managers[0].finance!=manager_initial_finance_from_legacy(native,0)||state.world.managers[0].match_state!=manager_match_state_from_legacy(native,0,{},{}))throw std::runtime_error("initial ticket/reset finance or match settings differ");
    if(state.world.managers[0].tactics!=manager_full_tactics_from_legacy(native,0,players))throw std::runtime_error("initial tactical familiarity/reset differs");
    if(read_save(make_save(state))!=state)throw std::runtime_error("initial manager services snapshot differs");
    std::cout<<"Native initial ticket/reset boundary: sales, all advertising offers/contracts, finance/tactics/history and W="<<services.watcom_draws<<" match\n";
    const auto market_before=native_random.calls();std::map<uint16_t,ClubId> identities;for(size_t i=0;i<164;++i)if(!identities.emplace(native.club_ids[i],mapping[i]).second)throw std::runtime_error("ambiguous Native club name identity");
    if(!career_first_steps(native,native_game,native_random,report))throw std::runtime_error(report.reason);
    const auto initial_market=owner_app::initialize_first_season_market(state,owner,owner);
    if(state.game_seed!=native_game.seed()||state.watcom_seed!=native_random.seed()||initial_market.watcom_draws!=native_random.calls()-market_before)throw std::runtime_error("initial shuffle/market RNG differs from Native");
    size_t ordinal=0;for(auto table:{TableGroup::germany,TableGroup::europe})for(auto league:state.world.catalogue.table_leagues(table))for(auto id:state.world.membership[league.value].clubs)if(id!=identities.at(native.club_ids[ordinal++]))throw std::runtime_error("initial shuffled membership differs from Native");
    const auto& market=*state.world.transfer_market;
    if(market.list_toggle!=native.list_toggle||uint32_t(market.checkpoint_game_seed)!=native.memory.u32(layout::CareerSession_Invalid_RuntimeField2))throw std::runtime_error("initial market toggle/checkpoint differs");
    for(size_t row=0;row<44;++row){const auto raw=native.squad_slots.at(size_t(runtime_dimensions.pool_first)+row);const auto& entry=market.rows[row];if((raw&0xfff)==0xfff){if(entry)throw std::runtime_error("initial market empty row differs");continue;}
        const auto at=layout::EditorList+uint32_t(row)*0x58;const auto seller=native.memory.u16(at+8);const auto source_seller=(seller&0x7fff)==0xfff?ClubId{}:identities.at(native.club_ids.at(seller&0x7fff));
        if(!entry||entry->player.value!=(raw&0xfff)||entry->seller!=source_seller||entry->fee!=native.memory.u32(at+0x2c)||entry->salary!=native.memory.u32(at+0x30)||entry->asking_percent!=native.memory.u8(at+0x54)||entry->demand_percent!=native.memory.u8(at+0x55)||entry->weeks!=native.memory.u8(at+0x56))throw std::runtime_error("initial market listing/quote differs from Native");
    }
    if(read_save(make_save(state))!=state)throw std::runtime_error("initial market snapshot differs");
    std::cout<<initial_market.listings<<" initial market listings and shuffled fixtures, checkpoint/toggle/quotes/snapshot match Native; W="<<initial_market.watcom_draws<<'\n';
    owner_app::initialize_desktops(state,reference);
    size_t events=0,offers=0,loans=0,lures=0;
    for(uint32_t seed=1;seed<30000&&(offers<16||!loans||!lures);++seed){
        WatcomRandom probe(seed);probe.range(0,10000);const bool expected_offer=probe.range(0,70-int16_t(state.world.managers[0].match_state->staff_ratings[0])/50)==0;
        if(!expected_offer&&events)continue;
        auto classic=native;auto typed=state;typed.watcom_seed=seed;GameRandom source_game(typed.game_seed);WatcomRandom source_random(seed);MinuteReport observed;
        std::vector<uint8_t> refs(50);MatchdayContext context;context.month=7;context.referees=&refs;
        const auto groups=prepare_event(classic,0,context,source_game,source_random,observed);
        if(!groups.empty()||observed.unsupported)throw std::runtime_error("initial Native empty event produced fixtures/error");
        const bool found=enter_event_desk(classic,0,groups,source_game,source_random,observed,true,[](const GameState&,WatcomRandom&){});
        if(observed.unsupported)throw std::runtime_error(observed.reason);
        owner_app::ManagerActionOwner event_owner{90,0};const auto entered=owner_app::prepare_first_desk_event(typed,event_owner,event_owner,reference);
        const auto& event=*typed.desktop->initial_event;
        if(typed.game_seed!=source_game.seed()||typed.watcom_seed!=source_random.seed()||entered.game_draws!=source_game.calls()||entered.watcom_draws!=source_random.calls())throw std::runtime_error("initial event RNG differs at seed "+std::to_string(seed));
        if(event.matchday_number!=classic.memory.u32(layout::CurrentMatchday)||event.manager_order!=std::vector<uint8_t>{0}||event.offer.has_value()!=found||typed.desktop->managers[0].prestige!=classic.manager_records[0x211])throw std::runtime_error("initial event order/offer/prestige differs");
        const auto& row=typed.world.transfer_market->rows[40];const auto at=layout::EditorList+40*0x58;
        if(row){if(row->player.value!=(classic.squad_slots.at(size_t(runtime_dimensions.pool_first)+40)&0xfff)||row->fee!=classic.memory.u32(at+0x2c)||row->salary!=classic.memory.u32(at+0x30)||row->asking_percent!=classic.memory.u8(at+0x54)||row->demand_percent!=classic.memory.u8(at+0x55))throw std::runtime_error("initial offer quote differs");
            const auto& bid=row->bids[0];if(bid.bidder.valid()&&(bid.bidder!=identities.at(classic.club_ids.at(classic.memory.u16(at)))||bid.fee!=classic.memory.u32(at+0x1c)||bid.salary!=classic.memory.u32(at+0xc)||bid.months!=classic.memory.u8(at+0x34)))throw std::runtime_error("initial offer bidder/amount/term differs");}
        if(found){++offers;loans+=bool(row->bids[0].months&128);lures+=event.offer->salary_only;if(event.offer->warned!=bool(classic.memory.u32(layout::OfferSlots)&0x80000000u))throw std::runtime_error("initial offer warning differs");}
        if(read_save(make_save(typed))!=typed)throw std::runtime_error("initial event snapshot differs");++events;
    }
    if(offers<16||!loans||!lures)throw std::runtime_error("initial event oracle did not exercise all offer kinds");
    std::cout<<events<<" Native first-event comparisons, "<<offers<<" offers including "<<loans<<" loan terms and "<<lures<<" salary-only approaches: quotes/bidders/terms/warnings/order/prestige/both RNGs/snapshots match\n";
    size_t scopes=0;
    for(int mask=1;mask<8;++mask){const GameScope scope{bool(mask&1),bool(mask&2),bool(mask&4)};if(!scope.club_play())continue;
        CareerSnapshot scoped;scoped.world=select_world_scope(master,scope);const auto limits=read_initial_strength_ranges(scoped.world.catalogue,german,english);const auto initial=make_player_initialization_plan(master,scope,limits);
        owner_app::ManagerActionOwner session{72,0};GameRandom scoped_game(17);WatcomRandom scoped_random(91);
        owner_app::initialize_club_player_state(scoped.world,session,{72,0},initial,reference,false,scoped_game,scoped_random);
        const CareerSettings settings{3,3,5,uint8_t(scope.germany+scope.england),false};
        for(auto country:{Country::germany,Country::england})if(scope.domestic_enabled(country)){const auto league=scoped.world.catalogue.starting_league(country,0);const auto seat=uint8_t(scoped.world.managers.size());
            owner_app::add_initial_manager(scoped.world,session,session,{seat,scoped.world.membership[league.value].clubs[0],league,"Scoped "+std::to_string(seat),seat,1},settings,limits,reference,country==Country::germany?german:english,scoped_game,scoped_random);}
        scoped.game_seed=scoped_game.seed();scoped.watcom_seed=scoped_random.seed();scoped.results=SeasonResults::empty(scoped.world.catalogue);
        const auto dates=SeasonCalendar::from_sources(scoped.world.catalogue,de,aligned.entries);
        owner_app::prepare_initial_competitions(scoped,session,session,dates,{german,english},reference);
        owner_app::initialize_first_season_manager_services(scoped,session,session,reference);
        owner_app::initialize_first_season_market(scoped,session,session);
        if(!scope.germany)for(const auto& field:scoped.world.initial_competitions->fields)for(auto id:field)if(scoped.world.clubs.at(id).country==Country::germany)throw std::runtime_error("disabled German club leaked into actual English-only European field");
        if(read_save(make_save(scoped))!=scoped)throw std::runtime_error("actual scoped initial competition snapshot differs");++scopes;
    }
    std::cout<<scopes<<" actual-data initial competition scopes with supplied German/English rules, calendars and snapshot roundtrips passed\n";
    std::cout<<"Native initial competition boundary: national/European/domestic fields, campaigns, card rows, both RNGs, calendar and snapshot match; G="<<result.game_draws<<" W="<<result.watcom_draws<<'\n';
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
