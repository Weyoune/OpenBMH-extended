// SPDX-License-Identifier: GPL-3.0-only
#include "app/world12_initial_season.hpp"
#include "app/world12_first_event.hpp"
#include "app/world12_pending.hpp"
#include "game/turn.hpp"
#include "game/matchday_setup.hpp"
#include "game/weekly.hpp"
#include "game/week.hpp"
#include "game/month.hpp"
#include "game/save.hpp"
#include "game/world12/weekly_players.hpp"
#include "game/world12/economy.hpp"
#include "game/world12/weekly_incidents.hpp"
#include "game/world12/market_bids.hpp"
#include "game/world12/market_auctions.hpp"
#include "game/world12/weekly_services.hpp"
#include "game/world12/monthly_market.hpp"
#include "game/world12/loan_returns.hpp"
#include "game/world12/monthly_accounts.hpp"
#include "game/world12/monthly_services.hpp"
#include "game/world12/daily_services.hpp"
#include "game/world12/standings.hpp"
#include "game/bank_credit.hpp"
#include "game/pending_offer.hpp"
#include <algorithm>
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
    if(argc!=4&&argc!=5)throw std::runtime_error("usage: world12_initial_season_validate WORLD12.JSON German-reference FLTD.EXE [proof-directory]");
    const std::filesystem::path proof=argc==5?argv[4]:std::filesystem::path{};
    if(!proof.empty())std::filesystem::create_directories(proof);
    const auto master=decode_world(read_file(argv[1]));const std::filesystem::path data=argv[2];const auto profile=load_data_profile(data);install_runtime_profile(profile);
    const auto german=load_le(data/"BMH.exe"),english=bmh::launch::translate_fltd(argv[3]);install_career_rules(german);install_fixture_tables(german);uint8_t sizes[kLeagueCount];for(size_t i=0;i<kLeagueCount;++i)sizes[i]=profile.leagues[i];set_league_sizes(sizes);
    const auto db=load_database(data/"M3D.MAN",profile.counts);auto native=startup_state(german,db,read_file(data/"M3.MEN"),read_file(data/"M3.DAT"));install_bank_credit_rules(native,german);const auto reference=native;const auto texts=load_textd(data/"TEXTD.BLK");native.texts=&texts;
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
    const auto initial_market=owner_app::initialize_first_season_market(state,owner,owner,&reference);
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
    {
        size_t checks=0,added=0;
        const auto compare=[&](const CareerSnapshot& typed,const GameState& core,const GameRandom& g,const WatcomRandom& w){
            if(typed.game_seed!=g.seed()||typed.watcom_seed!=w.seed())throw std::runtime_error("global market bid RNG differs from full Core");
            for(size_t row=0;row<40;++row)if(typed.world.transfer_market->rows[row]){const auto& listing=*typed.world.transfer_market->rows[row];const auto at=layout::EditorList+uint32_t(row*0x58);
                for(size_t bid=0;bid<4;++bid){const auto raw=core.memory.u16(at+uint32_t(bid*2));const auto id=raw==0xffff?ClubId{}:identities.at(core.club_ids.at(raw));const auto& b=listing.bids[bid];
                    if(b.bidder!=id)throw std::runtime_error("global market bidder differs from full Core");
                    if(id.valid()&&(b.fee!=core.memory.u32(at+0x1c+uint32_t(bid*4))||b.salary!=core.memory.u32(at+0xc+uint32_t(bid*4))||b.months!=core.memory.u8(at+0x34+uint32_t(bid))))throw std::runtime_error("global bid amount/term differs from full Core");
                    if(id.valid())for(size_t n=0;n<7;++n)if(b.bonuses[n]!=core.memory.u8(at+0x38+uint32_t(bid*7+n)))throw std::runtime_error("global bid bonus differs");
                }
            }
            if(read_save(make_save(typed))!=typed)throw std::runtime_error("global market snapshot differs");
        };
        for(size_t row=0;row<40;++row)if(state.world.transfer_market->rows[row])for(bool high:{false,true})for(uint32_t seed:{1u,91u,703u}){
            auto typed=state;typed.game_seed=17;typed.watcom_seed=seed;auto core=native;GameRandom g(17);WatcomRandom w(seed);MinuteReport report;
            for(int n=0;n<5;++n){transfer_bid(core,int(runtime_dimensions.pool_first+row),layout::EditorList+uint32_t(row*0x58),high,-1,g,w,report);if(report.unsupported)throw std::runtime_error(report.reason);
                const auto result=add_computer_market_bid(typed,uint8_t(row),reference,{true},high);added+=result.added;compare(typed,core,g,w);++checks;}
        }
        for(uint32_t seed:{1u,91u,703u,1777u}){auto typed=state;typed.game_seed=17;typed.watcom_seed=seed;auto core=native;GameRandom g(17);WatcomRandom w(seed);MinuteReport report;
            new_transfer_bids(core,g,w,report);if(report.unsupported)throw std::runtime_error(report.reason);generate_weekly_market_bids(typed,reference,{true});compare(typed,core,g,w);++checks;
            if(!proof.empty()&&seed==91){const auto file=proof/"global-market-bids.FBM";write_savegame(file,make_save(typed));if(read_save(load_savegame(file))!=typed)throw std::runtime_error("global bids disk snapshot differs");}
        }
        for(int variant=0;variant<2;++variant){auto fixture=state;auto source=native;
            size_t target_team=1;while(target_team<90&&std::any_of(fixture.world.transfer_market->rows.begin(),fixture.world.transfer_market->rows.end(),[&](const auto& row){return row&&row->seller==identities.at(source.club_ids[target_team]);}))++target_team;
            if(target_team==90)throw std::runtime_error("market fixture found no unlisted domestic club");const auto target=identities.at(source.club_ids[target_team]);
            if(variant==0){for(size_t row=13;row<24;++row){fixture.world.clubs.at(target).roster[row]={};source.squad_slots[target_team*24+row]=0xfff;}
                fixture.world.transfer_market->last_entry40_bidders={target,target,ClubId{},ClubId{}};fixture.world.transfer_market->entry40_history_known=true;
                for(size_t bid=0;bid<4;++bid)source.memory.set16(layout::EditorList+40*0x58+uint32_t(bid*2),bid<2?uint16_t(target_team):0xffff);
            }else{fixture.calendar.reset();fixture.date=source.date=100;int local_league=0;
                for(auto table:{TableGroup::germany,TableGroup::europe})for(auto league:fixture.world.catalogue.table_leagues(table)){
                    fixture.league_rounds[league.value]=12;source.league_rounds.at(size_t(local_league++))=12;
                    const auto size=fixture.world.catalogue.league(league).clubs;
                    for(int round=1;round<=12;++round)for(int match=0;match<size/2;++match)fixture.results.result(fixture.world.catalogue,league,round,match)={true,uint8_t((round+match)%3),uint8_t((round*2+match)%2)};
                    const auto ranks=compute_standings(fixture.world,fixture.results,league,12);
                    for(const auto& rank:ranks){size_t team=0;while(identities.at(source.club_ids[team])!=rank.club)++team;const size_t at=(team*13+3)*2;const auto old=uint16_t(source.position_history[at]|uint16_t(source.position_history[at+1])<<8);const auto next=uint16_t((old&~uint16_t(31<<10))|uint16_t((rank.rank-1)<<10));source.position_history[at]=uint8_t(next);source.position_history[at+1]=uint8_t(next>>8);}
                }
                for(const auto& slot:fixture.world.clubs.at(target).roster)if(!slot.empty()){auto& p=fixture.world.players.at(slot.player).attributes;p.w35c=(p.w35c&~((127u<<7)|(127u<<14)))|(10u<<7)|(10u<<14);source.players[slot.player.value].w35c=p.w35c;}
            }
            size_t row=0;while(!fixture.world.transfer_market->rows[row])++row;
            for(bool high:{false,true})for(uint32_t seed:{91u,703u}){auto typed=fixture;typed.game_seed=17;typed.watcom_seed=seed;auto core=source;GameRandom g(17);WatcomRandom w(seed);MinuteReport report;
                for(int n=0;n<5;++n){transfer_bid(core,int(runtime_dimensions.pool_first+row),layout::EditorList+uint32_t(row*0x58),high,-1,g,w,report);if(report.unsupported)throw std::runtime_error(report.reason);added+=add_computer_market_bid(typed,uint8_t(row),reference,{true},high).added;compare(typed,core,g,w);++checks;}
            }
        }
        if(!added)throw std::runtime_error("global bid oracle generated no bids");
        std::cout<<checks<<" full-Core global market bid comparisons, "<<added<<" actual bids: all buyers/fees/salaries/terms/bonuses/RNG/snapshots match\n";
    }
    {
        size_t auctions=0,moves=0,loans=0;
        for(int kind=0;kind<10;++kind)for(uint32_t seed:{1u,91u,703u})for(bool accept:{false,true}){
            auto typed=state;auto core=native;core.memory.set8(layout::ActiveManager,0);const int main_team=core.manager_teams[0];int ai_seller=0;while(ai_seller==main_team)++ai_seller;int ai_buyer=ai_seller+1;while(ai_buyer==main_team)++ai_buyer;
            for(size_t row=0;row<40;++row){typed.world.transfer_market->rows[row].reset();core.squad_slots.at(size_t(runtime_dimensions.pool_first)+row)=0xfff;}
            if(kind>=8){auto peer=typed.world.managers[0];peer.seat=1;peer.portrait=1;peer.name="Auction peer";peer.club=identities.at(core.club_ids[size_t(ai_buyer)]);typed.world.managers.push_back(peer);typed.world.settings->planned_managers=2;
                core.manager_teams.push_back(ai_buyer);core.team_morale[size_t(ai_buyer)]|=128;core.memory.set8(layout::ManagerCount,2);core.memory.set8(layout::OriginalManagerCount,2);
                std::copy_n(core.manager_records.begin(),0xb0c,core.manager_records.begin()+0xb0c);std::copy_n(core.tactics.begin(),0x840,core.tactics.begin()+0x840);std::copy_n(core.manager_options.begin(),5,core.manager_options.begin()+5);std::copy_n(core.manager_ratings.begin(),60,core.manager_ratings.begin()+60);
                for(uint32_t n=0;n<0x24;++n)core.memory.set8(layout::EnvironmentYouthTeams+0x24+n,core.memory.u8(layout::EnvironmentYouthTeams+n));
                std::fill_n(core.manager_records.begin()+0xb0c+0xa8,25,uint8_t(0));std::copy(peer.name.begin(),peer.name.end(),core.manager_records.begin()+0xb0c+0xa8);core.manager_records[0xb0c+0x290]=uint8_t(ai_buyer);core.manager_records[0xb0c+0x291]=uint8_t(ai_buyer>>8);core.manager_records[0xb0c+0x221]=1;
            }
            const bool free=kind==6||kind==7;const bool seller_human=kind==2||kind==3||kind>=8;const bool buyer_human=kind==4||kind==5||kind==7||kind>=8;const bool loan=kind<6?(kind%2):kind==9;
            const int seller_team=seller_human?main_team:ai_seller,buyer_team=kind>=8?ai_buyer:buyer_human?main_team:ai_buyer;
            PlayerId player;if(free)player=*std::find_if(typed.world.free_players->domestic.begin(),typed.world.free_players->domestic.end(),[](auto id){return id.valid();});else player={uint32_t(core.squad_slots[size_t(seller_team)*24+19]&0xfff)};
            core.players.at(player.value).w368&=0x07ffffffu;core.player_bonus.at(player.value)&=127;typed.world.players.at(player).attributes.w368=core.players[player.value].w368;typed.world.players.at(player).bonus=core.player_bonus[player.value];
            const auto seller=free?ClubId{}:identities.at(core.club_ids[size_t(seller_team)]),buyer=identities.at(core.club_ids[size_t(buyer_team)]);
            MarketListing listing;listing.player=player;listing.seller=seller;listing.human_seller=seller_human;listing.fee=100000;listing.salary=1000;listing.asking_percent=100;listing.demand_percent=100;listing.weeks=1;
            listing.bids[0]={buyer,1000000,100000,uint8_t(loan?0x8c:24),{}};typed.world.transfer_market->rows[0]=listing;
            const auto at=layout::EditorList;for(uint32_t n=0;n<0x58;++n)core.memory.set8(at+n,0);for(uint32_t n=0;n<4;++n)core.memory.set16(at+n*2,0xffff);
            core.memory.set16(at,uint16_t(buyer_team));core.memory.set16(at+8,free?0xfff:uint16_t(seller_team|(seller_human?0x8000:0)));core.memory.set32(at+0xc,100000);core.memory.set32(at+0x1c,1000000);core.memory.set32(at+0x2c,100000);core.memory.set32(at+0x30,1000);core.memory.set8(at+0x34,uint8_t(loan?0x8c:24));core.memory.set8(at+0x54,100);core.memory.set8(at+0x55,100);core.memory.set8(at+0x56,1);core.squad_slots[size_t(runtime_dimensions.pool_first)]=uint16_t(player.value);
            core.mailbox.assign(kMailboxEntries*kMailboxEntry,0);initialize_career_economy(typed,core);typed.game_seed=17;typed.watcom_seed=seed;
            core.transfer_offer_selector=[&](const GameState&,const TransferOfferListView& view){return accept?view.preferred_bid_index:-1;};core.transfer_decision_observer=[](const TransferDecisionEvent&){};
            MarketAuctionHooks hooks;hooks.active_seat=0;hooks.allocator=TransferAllocatorContext::zero;hooks.select=[&](const GameState&,const TransferOfferListView& view,const AiMatchProjection&)->std::optional<int>{return accept?view.preferred_bid_index:-1;};hooks.notice=[](const GameState&,const TransferDecisionEvent&,const AiMatchProjection&){};
            GameRandom g(17);WatcomRandom w(seed);MinuteReport report;transfer_list_countdown(core,g,w,report,false,TransferAllocatorContext::zero);if(report.unsupported)throw std::runtime_error(report.reason);
            const auto result=process_market_auctions(typed,reference,texts,hooks);moves+=result.moved;loans+=result.loans;
            if(typed.game_seed!=g.seed()||typed.watcom_seed!=w.seed())throw std::runtime_error("auction RNG differs at kind "+std::to_string(kind));
            for(const auto& [id,p]:typed.world.players)if(p.attributes.w35c!=core.players[id.value].w35c||p.attributes.w364!=core.players[id.value].w364||p.attributes.w368!=core.players[id.value].w368||p.cards!=core.player_cards[id.value]||p.bonus!=core.player_bonus[id.value])throw std::runtime_error("auction player/board attributes differ");
            for(size_t team=0;team<164;++team){const auto id=identities.at(core.club_ids[team]);const auto& roster=typed.world.clubs.at(id).roster;for(size_t row=0;row<24;++row){const auto raw=core.squad_slots[team*24+row];const uint16_t flags=uint16_t(((raw&0x8000)?1:0)|((raw&0x4000)?2:0)|((raw&0x2000)?4:0)|((raw&0x1000)?8:0));if((roster[row].empty()?0xfffu:roster[row].player.value)!=(raw&0xfff)||roster[row].flags!=flags)throw std::runtime_error("auction canonical roster differs");}}
            for(size_t m=0;m<typed.world.managers.size();++m)if(typed.world.managers[m].finance!=manager_initial_finance_from_legacy(core,int(m))||typed.world.managers[m].tactics!=manager_full_tactics_from_legacy(core,int(m),players)||typed.world.managers[m].career!=manager_career_state_from_legacy(core,uint8_t(m),players,true))throw std::runtime_error("auction human state differs kind="+std::to_string(kind)+" seed="+std::to_string(seed)+" answer="+std::to_string(accept)+" manager="+std::to_string(m)+" finance="+std::to_string(typed.world.managers[m].finance==manager_initial_finance_from_legacy(core,int(m)))+" tactics="+std::to_string(typed.world.managers[m].tactics==manager_full_tactics_from_legacy(core,int(m),players))+" career="+std::to_string(typed.world.managers[m].career==manager_career_state_from_legacy(core,uint8_t(m),players,true)));
            for(size_t m=0;m<typed.world.managers.size();++m){const auto at=m*0xb0c;const auto read32=[&](size_t offset){uint32_t v=0;for(size_t b=0;b<4;++b)v|=uint32_t(core.manager_records[at+offset+b])<<(b*8);return v;};
                if(uint32_t(typed.transfers->balance[m])!=read32(0x330))throw std::runtime_error("auction transfer balance differs");
                const auto identity=read32(0x7d8);const auto& history=typed.transfers->history[m];
                if(identity){if(history.size()!=1||history[0].player!=player||history[0].other!=((read32(0x760)&1)?seller:buyer)||history[0].source_word!=read32(0x760))throw std::runtime_error("auction qualified history differs");}else if(!history.empty())throw std::runtime_error("auction invented Source transfer history");
            }
            std::vector<Loan> expected_loans;for(uint32_t n=0;n<200;++n){const auto raw=core.memory.u32(layout::Career_RemoveJoinLoans_RuntimeField+n*4);if(raw)expected_loans.push_back({{raw&0xfff},identities.at(core.club_ids[(raw>>12)&0x3ff]),identities.at(core.club_ids[raw>>22]),core.memory.u8(layout::ClubPlayerDetail_InvalidArgument_RuntimeField+n),uint16_t(n)});}
            if(typed.world.loans!=expected_loans)throw std::runtime_error("auction global loan parties/months differ");
            if(result.moved&&loan&&seller_human){if(typed.transfers->loans.size()!=1)throw std::runtime_error("auction owner snapshot count="+std::to_string(typed.transfers->loans.size())+" kind="+std::to_string(kind)+" seed="+std::to_string(seed)+" answer="+std::to_string(accept)+" global loans="+std::to_string(typed.world.loans.size()));const auto& contract=typed.transfers->loans[0];int slot=-1;for(int n=0;n<12;++n)if(core.memory.u16(layout::Career_CareerManagersEnd_RuntimeField+uint32_t(n*2))==player.value){slot=n;break;}
                if(slot<0||contract.player!=player||contract.manager!=0)throw std::runtime_error("auction owner snapshot identity differs");const auto at=layout::ClubInfo_FormatError_RuntimeField+uint32_t(slot*0x58);
                for(size_t n=0;n<0x58;++n)if((n<0x44||n>=0x48)&&contract.board[n]!=core.memory.u8(at+uint32_t(n)))throw std::runtime_error("auction owner contract board differs");
                const auto mark=core.memory.u16(at+0x44),marked=core.memory.u16(at+0x46);if(contract.mark!=(mark==0xfff?PlayerId{}:PlayerId{mark})||contract.marked_by!=(marked==0xfff?PlayerId{}:PlayerId{marked}))throw std::runtime_error("auction loan mark references differ");
            }
            const size_t pools[]={size_t(runtime_dimensions.free_first),size_t(runtime_dimensions.foreign_free_first)};for(size_t pool=0;pool<2;++pool){const size_t count=pool?20:80;for(size_t n=0;n<count;++n){const auto expected=core.squad_slots[pools[pool]+n]&0xfff;const auto actual=pool?typed.world.free_players->foreign[n]:typed.world.free_players->domestic[n];if((actual.valid()?actual.value:0xfffu)!=expected)throw std::runtime_error("auction native free-pool slots differ");}}
            std::vector<ClubId> human_clubs;for(const auto& m:typed.world.managers)human_clubs.push_back(m.club);
            auto projection=AiMatchProjection::create_for_humans(typed,human_clubs,reference,{3,3,false,true});install_career_economy(projection,typed.world);if(compare_mailbox(projection.state.mailbox,core.mailbox))throw std::runtime_error("auction mailbox differs");
            if(read_save(make_save(typed))!=typed)throw std::runtime_error("auction snapshot differs");
            if(!proof.empty()&&kind==7&&seed==91&&accept){const auto file=proof/"free-agent-auction.FBM";write_savegame(file,make_save(typed));if(read_save(load_savegame(file))!=typed)throw std::runtime_error("auction disk snapshot differs");}
            if(result.loans){
                // Compare actual Source roster traversal, including human return notices.
                for(int pass=0;pass<2;++pass){
                    typed.world.loans[0].months=uint8_t(pass?1:2);const auto ledger_slot=*typed.world.loans[0].source_slot;core.memory.set8(layout::ClubPlayerDetail_InvalidArgument_RuntimeField+ledger_slot,uint8_t(pass?1:2));
                    core.loan_return_screen=[](GameState&,int,const std::string&){return 0;};
                    for(int slot=0;slot<runtime_dimensions.pool_first&&!report.unsupported;++slot)return_loan_at(core,slot,w,&g,report);
                    if(report.unsupported)throw std::runtime_error(report.reason);
                    LoanReturnHooks returns;returns.active_seat=0;returns.keep=[](const GameState&,uint8_t,const std::string&,const AiMatchProjection&)->std::optional<bool>{return false;};return_monthly_loans(typed,reference,texts,returns);
                    if(typed.game_seed!=g.seed()||typed.watcom_seed!=w.seed())throw std::runtime_error("loan-return RNG differs");
                    for(size_t team=0;team<164;++team){const auto& roster=typed.world.clubs.at(identities.at(core.club_ids[team])).roster;for(size_t row=0;row<24;++row){const auto raw=core.squad_slots[team*24+row];const uint16_t flags=uint16_t(((raw&0x8000)?1:0)|((raw&0x4000)?2:0)|((raw&0x2000)?4:0)|((raw&0x1000)?8:0));if((roster[row].empty()?0xfffu:roster[row].player.value)!=(raw&0xfff)||roster[row].flags!=flags)throw std::runtime_error("loan-return roster differs");}}
                    for(size_t m=0;m<typed.world.managers.size();++m)if(typed.world.managers[m].finance!=manager_initial_finance_from_legacy(core,int(m))||typed.world.managers[m].tactics!=manager_full_tactics_from_legacy(core,int(m),players)||typed.world.managers[m].career!=manager_career_state_from_legacy(core,uint8_t(m),players,true))throw std::runtime_error("loan-return human state differs");
                    auto returned=AiMatchProjection::create_for_humans(typed,human_clubs,reference,{3,3,false,true});install_career_economy(returned,typed.world);if(compare_mailbox(returned.state.mailbox,core.mailbox))throw std::runtime_error("loan-return mailbox differs");
                    if(pass&&(typed.world.loans.size()||typed.transfers->loans.size()))throw std::runtime_error("expired Source loan retained");
                    if(read_save(make_save(typed))!=typed)throw std::runtime_error("loan-return snapshot differs");
                }
            }
            ++auctions;
        }
        if(!moves||!loans)throw std::runtime_error("auction oracle exercised no movement/loans");
        std::cout<<auctions<<" full-Core auction comparisons, "<<moves<<" actual moves and "<<loans<<" loans: AI/human/free-agent rosters/accounts/boards/pool slots/mail/RNG/snapshots match; 36 loan countdown/expiry comparisons match\n";
    }
    {
        size_t passes=0;
        for(int variant=0;variant<3;++variant){
            auto typed=state;auto core=native;core.mailbox.assign(kMailboxEntries*kMailboxEntry,0);
            const auto put=[&](size_t at,uint32_t value){for(size_t b=0;b<4;++b)core.manager_records.at(at+b)=uint8_t(value>>(8*b));};
            put(0x854,uint32_t(variant+1)*15000);put(0x858,uint32_t(variant+1)*25000);
            for(size_t k=0;k<6;++k)core.memory.set32(layout::Spending+uint32_t(k*4),uint32_t(variant+1)*uint32_t(k+1)*100);
            typed.world.managers[0].career=manager_career_state_from_legacy(core,0,players,true);
            for(size_t k=0;k<6;++k){const auto fund=layout::FundBlock+uint32_t(k*0x1c);core.memory.set32(fund,1000000);core.memory.set16(layout::Advertising_OriginalShareAdjustment_RuntimeField+uint32_t(k*0x1c),uint16_t(variant+1));for(size_t b=0;b<0x1c;++b)typed.world.markets->funds[k*0x1c+b]=core.memory.u8(fund+uint32_t(b));}
            const auto credit=layout::Loans;core.memory.set32(credit,100000);core.memory.set32(credit+4,105000);core.memory.set32(credit+8,35000);core.memory.set32(credit+12,3);core.memory.set32(credit+16,0);core.memory.set32(credit+20,0);
            initialize_career_economy(typed,core);
            for(int pass=0;pass<3;++pass){
                expire_monthly_advertising(core);monthly_manager_accounts(core,0);monthly_shares_and_loans(core,true);
                expire_monthly_sponsors(typed,reference,texts);settle_monthly_accounts(typed,reference);settle_monthly_investments(typed,reference);
                if(typed.world.managers[0].finance!=manager_initial_finance_from_legacy(core,0)||typed.world.managers[0].career!=manager_career_state_from_legacy(core,0,players,true)||typed.world.managers[0].match_state!=manager_match_state_from_legacy(core,0,{},{}))throw std::runtime_error("monthly accounts human fields differ");
                auto p=AiMatchProjection::create_for_humans(typed,{typed.world.managers[0].club},reference,{3,3,false,true});install_career_economy(p,typed.world);
                for(size_t at:{size_t(0x286),size_t(0x338),size_t(0x33c),size_t(0x854),size_t(0x858),size_t(0x85c),size_t(0x860),size_t(0x864),size_t(0x86c),size_t(0x870),size_t(0x878),size_t(0x87c),size_t(0x880),size_t(0x884),size_t(0x888),size_t(0x88c),size_t(0x890),size_t(0x894),size_t(0x898),size_t(0x89c)})for(size_t b=0;b<(at==0x286?1u:4u);++b)if(p.state.manager_records[at+b]!=core.manager_records[at+b])throw std::runtime_error("monthly categorized account differs at "+std::to_string(at));
                for(uint32_t n=0;n<0x78;++n)if(p.state.memory.u8(layout::Loans+n)!=core.memory.u8(layout::Loans+n))throw std::runtime_error("monthly bank loan differs");
                for(uint32_t n=0;n<6*0x1c;++n)if(p.state.memory.u8(layout::FundBlock+n)!=core.memory.u8(layout::FundBlock+n))throw std::runtime_error("monthly funds differ");
                if(compare_mailbox(p.state.mailbox,core.mailbox))throw std::runtime_error("monthly expiry mailbox differs");
                if(read_save(make_save(typed))!=typed)throw std::runtime_error("monthly account snapshot differs");++passes;
            }
        }
        std::cout<<passes<<" full-Core monthly advertising/accounts/investment comparisons: ratings, costs, wages, categorized counters, actual-holder dividends, credit maturity, mailbox and snapshots match\n";
    }
    {
        size_t passes=0,released=0;
        for(int variant=0;variant<3;++variant)for(uint32_t seed:{1u,91u,703u}){
            auto typed=state;auto core=native;core.mailbox.assign(kMailboxEntries*kMailboxEntry,0);typed.game_seed=17;typed.watcom_seed=seed;
            const int team=core.manager_teams[0],row=19;const auto player=core.squad_slots.at(size_t(team*24+row))&0xfff;
            if(variant){core.tactics.at(size_t(row)*0x58+0xc)=1;if(variant==2)core.tactics.at(size_t(row)*0x58+0xb)|=0x20;
                typed.world.managers[0].tactics=manager_full_tactics_from_legacy(core,0,players);}
            if(variant==2)core.manager_records.at(0x286)=12;
            initialize_career_economy(typed,core);GameRandom gr(17);WatcomRandom wr(seed);MinuteReport report;
            core.contract_screen=[](GameState&,int,int){return 0;};month_change(core,wr,report,&gr,true);if(report.unsupported)throw std::runtime_error(report.reason);
            MonthlyContractHooks contracts;contracts.renew=[](GameState&,GameRandom&,WatcomRandom&,uint8_t,PlayerId,int,const AiMatchProjection&)->std::optional<bool>{return false;};
            const auto result=process_monthly_services(typed,reference,texts,{}, {},contracts);released+=result.released;
            if(typed.game_seed!=gr.seed()||typed.watcom_seed!=wr.seed()||result.game_draws!=gr.calls()||result.watcom_draws!=wr.calls())throw std::runtime_error("complete monthly RNG differs variant="+std::to_string(variant));
            for(const auto& [id,person]:typed.world.players)if(person.attributes.w35c!=core.players[id.value].w35c||person.attributes.w364!=core.players[id.value].w364||person.attributes.w368!=core.players[id.value].w368||person.bonus!=core.player_bonus[id.value]||person.cards!=core.player_cards[id.value])throw std::runtime_error("complete monthly player differs");
            for(size_t club=0;club<164;++club){const auto& roster=typed.world.clubs.at(identities.at(core.club_ids[club])).roster;for(size_t r=0;r<24;++r){const auto raw=core.squad_slots[club*24+r];const uint16_t flags=uint16_t(((raw&0x8000)?1:0)|((raw&0x4000)?2:0)|((raw&0x2000)?4:0)|((raw&0x1000)?8:0));if((roster[r].empty()?0xfffu:roster[r].player.value)!=(raw&0xfff)||roster[r].flags!=flags)throw std::runtime_error("complete monthly roster differs");}}
            if(typed.world.managers[0].finance!=manager_initial_finance_from_legacy(core,0)||typed.world.managers[0].career!=manager_career_state_from_legacy(core,0,players,true)||typed.world.managers[0].tactics!=manager_full_tactics_from_legacy(core,0,players)||typed.world.managers[0].match_state!=manager_match_state_from_legacy(core,0,{},{}))throw std::runtime_error("complete monthly human record differs");
            const size_t pool_first[]={size_t(runtime_dimensions.free_first),size_t(runtime_dimensions.foreign_free_first)};for(size_t pool=0;pool<2;++pool)for(size_t n=0;n<(pool?20u:80u);++n){const auto actual=pool?typed.world.free_players->foreign[n]:typed.world.free_players->domestic[n];if((actual.valid()?actual.value:0xfffu)!=(core.squad_slots[pool_first[pool]+n]&0xfff))throw std::runtime_error("complete monthly pool differs");}
            auto projection=AiMatchProjection::create_for_humans(typed,{typed.world.managers[0].club},reference,{3,3,false,true});install_career_economy(projection,typed.world);if(compare_mailbox(projection.state.mailbox,core.mailbox))throw std::runtime_error("complete monthly mailbox differs");
            if(read_save(make_save(typed))!=typed)throw std::runtime_error("complete month snapshot differs");
            if(!proof.empty()&&variant==2&&seed==91)write_savegame(proof/"complete-month.FBM",make_save(typed));++passes;
        }
        std::cout<<passes<<" full-Core complete monthly transactions: "<<released<<" contract/retirement releases, listings, boards, accounts, rosters, pools, messages, aggregate RNG and snapshots match (per-holder dividend fix enabled)\n";
    }
    {
        size_t passes=0;
        for(uint32_t seed:{91u,703u}){auto typed=state;auto core=native;core.mailbox.assign(kMailboxEntries*kMailboxEntry,0);core.offer_answer=false;
            core.memory.add_block(layout::CompetitionSelection,4);core.memory.set32(layout::CompetitionSelection,1u|(typed.world.catalogue.scope.international?2u:0u));
            for(uint32_t fund=0;fund<6;++fund)for(uint32_t manager=0;manager<4;++manager){const auto at=layout::Advertising_OriginalShareAdjustment_RuntimeField+fund*0x1c+manager*2;core.memory.set16(at,0);typed.world.markets->funds[at-layout::FundBlock]=0;typed.world.markets->funds[at-layout::FundBlock+1]=0;}
            initialize_career_economy(typed,core);typed.game_seed=17;typed.watcom_seed=seed;GameRandom cg(17);WatcomRandom cw(seed);
            core.transfer_offer_selector=[](const GameState&,const TransferOfferListView&){return -1;};core.transfer_decision_observer=[](const TransferDecisionEvent&){};core.contract_screen=[](GameState&,int,int){return 0;};
            DailyServiceHooks hooks;hooks.incidents.answer=[](const GameState&,uint8_t,const WeeklyOfferQuestion&)->std::optional<bool>{return false;};hooks.auctions.active_seat=0;hooks.auctions.allocator=TransferAllocatorContext::zero;hooks.auctions.select=[](const GameState&,const TransferOfferListView&,const AiMatchProjection&)->std::optional<int>{return -1;};hooks.auctions.notice=[](const GameState&,const TransferDecisionEvent&,const AiMatchProjection&){};hooks.contracts.renew=[](GameState&,GameRandom&,WatcomRandom&,uint8_t,PlayerId,int,const AiMatchProjection&)->std::optional<bool>{return false;};
            for(uint32_t day=1;day<=35;++day){const auto gc=cg.calls(),wc=cw.calls();const auto kind=day%2?DailyEconomyKind::after_event:DailyEconomyKind::plain_day;
                if(kind==DailyEconomyKind::plain_day)stock_market(core,cg);++core.date;daily_interest(core,cw);typed.date=day;
                DailyServiceContext context;context.previous_date=day-1;context.season=typed.season;context.epoch=calendar_epoch();context.entry=kind;context.weekly_relations={day,typed.season,{}};
                WeeklyClubRelations relation;relation.club=typed.world.managers[0].club;const auto team=core.manager_teams[0];
                for(size_t cup=0;cup<3&&!relation.advertising.european_cup;++cup)for(size_t n=0;n<core.memory.u8(counts[cup]);++n)if(core.memory.u16(lists[cup]+uint32_t(n*2))==team){relation.advertising.european_cup=uint8_t(cup+1);break;}
                relation.advertising.champion=core.memory.u16(layout::Cup_AdvanceCup_RuntimeField6)==team;const uint32_t entries[]={0,15,19,21};for(size_t k=0;k<4;++k)relation.advertising.holders[k]=core.memory.u16(layout::Season_CareerSeasonSetup_RuntimeField+entries[k]*0x28)==team;context.weekly_relations.clubs.push_back(relation);
                MinuteReport report;if(!day_step_rest(core,cg,cw,report,uint16_t(day-(kind==DailyEconomyKind::after_event?1:0)),TransferAllocatorContext::zero))throw std::runtime_error(report.reason);
                const auto result=process_daily_services(typed,reference,texts,context,hooks);
                if(typed.game_seed!=cg.seed()||typed.watcom_seed!=cw.seed()||result.game_draws!=cg.calls()-gc||result.watcom_draws!=cw.calls()-wc)throw std::runtime_error("daily service RNG differs day="+std::to_string(day)+" source_date="+std::to_string(core.date)+" full="+std::to_string(full_game(core))+" G="+std::to_string(result.game_draws)+"/"+std::to_string(cg.calls()-gc)+" W="+std::to_string(result.watcom_draws)+"/"+std::to_string(cw.calls()-wc));
                for(const auto& [id,person]:typed.world.players)if(person.attributes.w35c!=core.players[id.value].w35c||person.attributes.w364!=core.players[id.value].w364||person.attributes.w368!=core.players[id.value].w368||person.bonus!=core.player_bonus[id.value]||person.cards!=core.player_cards[id.value])throw std::runtime_error("daily service player differs");
                for(size_t club=0;club<164;++club){const auto& roster=typed.world.clubs.at(identities.at(core.club_ids[club])).roster;for(size_t row=0;row<24;++row){const auto raw=core.squad_slots[club*24+row];const uint16_t flags=uint16_t(((raw&0x8000)?1:0)|((raw&0x4000)?2:0)|((raw&0x2000)?4:0)|((raw&0x1000)?8:0));if((roster[row].empty()?0xfffu:roster[row].player.value)!=(raw&0xfff)||roster[row].flags!=flags)throw std::runtime_error("daily service roster differs");}}
                if(typed.world.managers[0].finance!=manager_initial_finance_from_legacy(core,0)||typed.world.managers[0].career!=manager_career_state_from_legacy(core,0,players,true)||typed.world.managers[0].tactics!=manager_full_tactics_from_legacy(core,0,players))throw std::runtime_error("daily service manager differs");
                auto p=AiMatchProjection::create_for_humans(typed,{typed.world.managers[0].club},reference,{3,3,false,true});install_career_economy(p,typed.world);if(compare_mailbox(p.state.mailbox,core.mailbox)||p.state.stocks!=core.stocks)throw std::runtime_error("daily service mailbox/stocks differ");
                if(result.post_event_stock_pending){day_stock_market(core,cg);update_daily_stock_market(typed,reference);if(typed.game_seed!=cg.seed()||typed.watcom_seed!=cw.seed())throw std::runtime_error("post-event market tick differs");}
                if(read_save(make_save(typed))!=typed)throw std::runtime_error("daily service snapshot differs");++passes;
            }
        }
        std::cout<<passes<<" dated full-Core daily-service comparisons: explicit event/plain finance, weekly/training/month transition, separate post-event market tick, rosters/boards/mail/aggregate RNG/snapshots match; fixtures not played\n";
    }
    {
        size_t passes=0;
        for(uint32_t seed:{1u,91u,703u,1777u})for(bool answer:{false,true}) {
            auto typed=state;auto core=native;core.memory.set8(layout::ActiveManager,0);core.mailbox.assign(kMailboxEntries*kMailboxEntry,0);initialize_career_economy(typed,core);typed.game_seed=17;typed.watcom_seed=seed;core.offer_answer=answer;
            WeeklyIncidentContext context{typed.date,typed.season,{}};WeeklyClubRelations relation;relation.club=typed.world.managers[0].club;const auto team=core.manager_teams[0];
            const uint32_t lists[]={layout::Career_CompetitionPointersEnglish_ExecutableField25,layout::Career_CompetitionPointersEnglish_RuntimeField14,layout::Career_CompetitionPointersEnglish_ExecutableField19};const uint32_t counts[]={layout::Career_EuropeanClub_ExecutableField,layout::Career_EuropeanClub_ExecutableField2,layout::Career_EuropeanClub_ExecutableField3};
            for(size_t cup=0;cup<3&&!relation.advertising.european_cup;++cup)for(size_t n=0;n<core.memory.u8(counts[cup]);++n)if(core.memory.u16(lists[cup]+uint32_t(n*2))==team){relation.advertising.european_cup=uint8_t(cup+1);break;}
            relation.advertising.champion=core.memory.u16(layout::Cup_AdvanceCup_RuntimeField6)==team;const uint32_t entries[]={0,15,19,21};for(size_t k=0;k<4;++k)relation.advertising.holders[k]=core.memory.u16(layout::Season_CareerSeasonSetup_RuntimeField+entries[k]*0x28)==team;context.clubs.push_back(relation);
            WeeklyIncidentHooks incidents;incidents.answer=[&](const GameState&,uint8_t,const WeeklyOfferQuestion&)->std::optional<bool>{return answer;};MarketAuctionHooks auction;auction.active_seat=0;auction.allocator=TransferAllocatorContext::zero;auction.select=[&](const GameState&,const TransferOfferListView& v,const AiMatchProjection&)->std::optional<int>{return answer?v.preferred_bid_index:-1;};auction.notice=[](const GameState&,const TransferDecisionEvent&,const AiMatchProjection&){};
            core.transfer_offer_selector=[&](const GameState&,const TransferOfferListView& v){return answer?v.preferred_bid_index:-1;};core.transfer_decision_observer=[](const TransferDecisionEvent&){};
            GameRandom g(17);WatcomRandom w(seed);
            for(int week=0;week<8;++week){const auto old_g=g.calls(),old_w=w.calls();MinuteReport report;weekly_processing(core,g,w,report,TransferAllocatorContext::zero);if(report.unsupported)throw std::runtime_error(report.reason);familiarity_decay(core,g,w);train_squad(core,0,g,w);
            const auto result=process_weekly_services(typed,reference,texts,context,incidents,auction,{true});
            if(typed.game_seed!=g.seed()||typed.watcom_seed!=w.seed()||result.game_draws!=g.calls()-old_g||result.watcom_draws!=w.calls()-old_w)throw std::runtime_error("complete weekly RNG differs from full Core");
            for(const auto& [id,p]:typed.world.players)if(p.attributes.w35c!=core.players[id.value].w35c||p.attributes.w364!=core.players[id.value].w364||p.attributes.w368!=core.players[id.value].w368||p.cards!=core.player_cards[id.value])throw std::runtime_error("complete weekly player differs");
            for(size_t team=0;team<164;++team){const auto id=identities.at(core.club_ids[team]);const auto& roster=typed.world.clubs.at(id).roster;for(size_t row=0;row<24;++row){const auto raw=core.squad_slots[team*24+row];const uint16_t flags=uint16_t(((raw&0x8000)?1:0)|((raw&0x4000)?2:0)|((raw&0x2000)?4:0)|((raw&0x1000)?8:0));if((roster[row].empty()?0xfffu:roster[row].player.value)!=(raw&0xfff)||roster[row].flags!=flags)throw std::runtime_error("complete weekly roster differs");}}
            if(typed.world.managers[0].finance!=manager_initial_finance_from_legacy(core,0)||typed.world.managers[0].tactics!=manager_full_tactics_from_legacy(core,0,players)||typed.world.managers[0].career!=manager_career_state_from_legacy(core,0,players,true))throw std::runtime_error("complete weekly manager state differs");
            auto p=AiMatchProjection::create_for_humans(typed,{relation.club},reference,{3,3,false,true});install_career_economy(p,typed.world);if(compare_mailbox(p.state.mailbox,core.mailbox)){for(size_t row=0;row<40;++row){const auto at=row*kMailboxEntry;if(!std::equal(p.state.mailbox.begin()+ptrdiff_t(at),p.state.mailbox.begin()+ptrdiff_t(at+kMailboxEntry),core.mailbox.begin()+ptrdiff_t(at))){std::cerr<<"mail row="<<row<<" actual=";for(size_t b=0;b<10;++b)std::cerr<<int(p.state.mailbox[at+b])<<",";std::cerr<<" source=";for(size_t b=0;b<10;++b)std::cerr<<int(core.mailbox[at+b])<<",";std::cerr<<"\n";break;}}throw std::runtime_error("complete weekly mail differs week="+std::to_string(week)+" seed="+std::to_string(seed)+" answer="+std::to_string(answer));}
            if(read_save(make_save(typed))!=typed)throw std::runtime_error("complete weekly snapshot differs");
            if(!proof.empty()&&seed==91&&answer){const auto file=proof/"complete-weekly.FBM";write_savegame(file,make_save(typed));if(read_save(load_savegame(file))!=typed)throw std::runtime_error("complete weekly disk snapshot differs");}++passes;
            }
        }
        std::cout<<passes<<" complete full-Core weekly+training transactions: fields, aggregate draw counts, accounts/boards/mail/snapshots match\n";
    }
    {
        size_t passes=0,listings=0;
        for(int variant=0;variant<3;++variant)for(uint32_t seed:{1u,91u,703u,1777u}){
            auto typed=state;auto core=native;core.memory.set8(layout::ActiveManager,0);core.mailbox.assign(kMailboxEntries*kMailboxEntry,0);initialize_career_economy(typed,core);typed.watcom_seed=seed;
            typed.world.managers[0].career->transfer_filters.fill(0);typed.world.managers[0].career->transfer_filters[5]=31;typed.world.managers[0].career->transfer_filters[7]=99;
            for(size_t n=0;n<18;++n)core.manager_records[0x1f6+n]=typed.world.managers[0].career->transfer_filters[n];
            if(variant>=1){for(size_t n=0;n<80;++n)if((variant==1?n%3==0:n>2)&&!std::any_of(typed.world.transfer_market->rows.begin(),typed.world.transfer_market->rows.end(),[&](const auto& row){return row&&row->player==typed.world.free_players->domestic[n];})){typed.world.free_players->domestic[n]={};core.squad_slots[size_t(runtime_dimensions.free_first)+n]=0xfff;}
                for(size_t n=0;n<20;++n)if((variant==1?n%3==0:n>2)&&!std::any_of(typed.world.transfer_market->rows.begin(),typed.world.transfer_market->rows.end(),[&](const auto& row){return row&&row->player==typed.world.free_players->foreign[n];})){typed.world.free_players->foreign[n]={};core.squad_slots[size_t(runtime_dimensions.foreign_free_first)+n]=0xfff;}
            }
            WatcomRandom w(seed);MinuteReport report;new_transfer_list_entries(core,w,report);if(report.unsupported)throw std::runtime_error(report.reason);const auto result=renew_monthly_market(typed,reference,texts,{});listings+=result.added;
            if(typed.watcom_seed!=w.seed()||result.watcom_draws!=w.calls()||typed.world.transfer_market->list_toggle!=core.list_toggle)throw std::runtime_error("monthly listings RNG/toggle differs variant="+std::to_string(variant));
            for(size_t row=0;row<40;++row){const auto raw=core.squad_slots[size_t(runtime_dimensions.pool_first)+row]&0xfff;const auto& listing=typed.world.transfer_market->rows[row];if((raw==0xfff)!=!listing)throw std::runtime_error("monthly listing empty slot differs");if(!listing)continue;const auto at=layout::EditorList+uint32_t(row*0x58);const auto seller=core.memory.u16(at+8)&0x7fff;const auto owner=seller==0xfff?ClubId{}:identities.at(core.club_ids[seller]);
                if(listing->player.value!=raw||listing->seller!=owner||listing->fee!=core.memory.u32(at+0x2c)||listing->salary!=core.memory.u32(at+0x30)||listing->asking_percent!=core.memory.u8(at+0x54)||listing->demand_percent!=core.memory.u8(at+0x55)||listing->weeks!=core.memory.u8(at+0x56))throw std::runtime_error("monthly listing quote/owner differs");
            }
            auto p=AiMatchProjection::create_for_humans(typed,{typed.world.managers[0].club},reference,{3,3,false,true});install_career_economy(p,typed.world);if(compare_mailbox(p.state.mailbox,core.mailbox))throw std::runtime_error("monthly search profile news differs");if(read_save(make_save(typed))!=typed)throw std::runtime_error("monthly listing snapshot differs");
            if(!proof.empty()&&variant==2&&seed==91){const auto file=proof/"monthly-listings.FBM";write_savegame(file,make_save(typed));if(read_save(load_savegame(file))!=typed)throw std::runtime_error("monthly listing disk snapshot differs");}++passes;
        }
        std::cout<<passes<<" full-Core monthly renewal comparisons, "<<listings<<" listings: pool holes, scarce free agents, club selection, quotes/search news/RNG/snapshots match\n";
    }
    {
        auto startup=state,source_start=state;auto initial_template=native;
        // Season setup has already posted its own messages. Compare the numeric
        // fresh-career template independently of that earlier presentation queue.
        initial_template.mailbox.assign(kMailboxEntries*kMailboxEntry,0);
        initialize_career_economy(startup,reference);initialize_career_economy(source_start,initial_template);
        if(startup.world.economy!=source_start.world.economy)throw std::runtime_error("initial economy template differs from initialized full Core");
        size_t passes=0;
        for(int scenario=0;scenario<4;++scenario)for(auto kind:{DailyEconomyKind::after_event,DailyEconomyKind::plain_day}) {
            auto core=native;auto typed=state;
            const auto put32=[&](size_t at,uint32_t value){for(size_t b=0;b<4;++b)core.manager_records.at(at+b)=uint8_t(value>>(b*8));};
            put32(0x314,scenario==0?5000000u:scenario==1?uint32_t(-2000000):140000000u);put32(0x318,2000000);put32(0x870,123);put32(0x89c,456);
            core.manager_records[0x21f]=0x80;core.manager_records[0x285]=scenario==3?52:0;
            core.manager_records[0x287]=1;put32(0x850,100000);core.manager_records[0x174]=3;
            for(size_t loan=0;loan<5;++loan)for(size_t n=0;n<6;++n)core.memory.set32(layout::Loans+uint32_t(loan*0x18+n*4),uint32_t((loan+1)*100+n));
            core.mailbox.assign(kMailboxEntries*kMailboxEntry,0);typed.world.managers[0].finance=manager_initial_finance_from_legacy(core,0);
            initialize_career_economy(typed,core);GameRandom cg(17);WatcomRandom cw(91);typed.game_seed=17;typed.watcom_seed=91;
            if(kind==DailyEconomyKind::plain_day)stock_market(core,cg);daily_interest(core,cw);
            daily_finance(typed,reference,kind);
            const auto check_economy=[&]{
                if(typed.game_seed!=cg.seed()||typed.watcom_seed!=cw.seed())throw std::runtime_error("daily/weekly economy RNG differs from full Core");
                if(typed.world.managers[0].finance!=manager_initial_finance_from_legacy(core,0))throw std::runtime_error("daily/weekly money differs from full Core");
                std::vector<ClubId> clubs{typed.world.managers[0].club};const auto s=*typed.world.settings;
                auto projected=AiMatchProjection::create_for_humans(typed,clubs,reference,{s.mode,s.difficulty,s.foreign_rule,true});install_career_economy(projected,typed.world);
                const auto& actual=projected.state;const size_t offsets[]={0x318,0x870,0x89c,0x850};
                for(auto at:offsets)for(size_t b=0;b<4;++b)if(actual.manager_records[at+b]!=core.manager_records[at+b])throw std::runtime_error("daily/weekly financial scalar differs");
                for(size_t n=0;n<52;++n){if(actual.manager_records[0x17a+n]!=core.manager_records[0x17a+n])throw std::runtime_error("weekly facility history differs");for(size_t at:{0x3d8u,0x4a8u,0x578u,0x648u})for(size_t b=0;b<4;++b)if(actual.manager_records[at+n*4+b]!=core.manager_records[at+n*4+b])throw std::runtime_error("weekly financial history differs");}
                for(auto at:{0x285u,0x287u,0x21fu})if(actual.manager_records[at]!=core.manager_records[at])throw std::runtime_error("weekly camp/history cursor differs");
                if(actual.stocks!=core.stocks)throw std::runtime_error("daily stocks/holdings differ");
                if(typed.world.managers[0].tactics!=manager_full_tactics_from_legacy(core,0,players))throw std::runtime_error("weekly contract/insurance board differs");
                if(read_save(make_save(typed))!=typed)throw std::runtime_error("economy snapshot differs");
            };
            check_economy();weekly_manager_settlement(core,0,cw);settle_weekly_managers(typed,reference);check_economy();
            if(!proof.empty()&&scenario==3&&kind==DailyEconomyKind::plain_day){const auto file=proof/"weekly-economy.FBM";write_savegame(file,make_save(typed));if(read_save(load_savegame(file))!=typed)throw std::runtime_error("weekly economy disk roundtrip differs");}
            ++passes;
        }
        std::cout<<passes<<" full-Core economy comparisons: deposits/overdraft/large balance, event-vs-plain-day shares, camp bills, loans and rolling weekly histories match\n";
    }
    {
        size_t passes=0,questions=0,credit_changes=0;
        const auto loan_fixture=[](GameState& g){const uint32_t words[]={100000,120000,5000,24,6,30000};for(size_t n=0;n<6;++n)g.memory.set32(layout::Loans+uint32_t(n*4),words[n]);};std::vector<uint32_t> incident_seeds{1u,91u,703u,1777u,5001u};
        for(uint32_t seed=1;seed<30000;++seed){auto probe=native;loan_fixture(probe);WatcomRandom random(seed);MinuteReport report;bool asked=false;WeeklyOfferHooks observe;
            observe.answer=[&](const GameState&,const WeeklyOfferQuestion&)->std::optional<bool>{asked=true;return false;};manager_offers(probe,0,random,report,&observe);
            if(report.unsupported)throw std::runtime_error(report.reason);if(asked){incident_seeds.push_back(seed);break;}}
        for(uint32_t seed=1;seed<30000;++seed){auto probe=native;loan_fixture(probe);WatcomRandom random(seed);MinuteReport report;manager_offers(probe,0,random,report);if(report.unsupported)throw std::runtime_error(report.reason);if(probe.manager_records[0x179]){incident_seeds.push_back(seed);break;}}
        for(uint8_t week:{2,11})for(uint32_t seed:incident_seeds)for(bool answer:{false,true}) {
            auto core=native;loan_fixture(core);auto typed=state;core.memory.set8(layout::Savestate_Layout_ExecutableField,week);core.offer_answer=answer;
            const auto sold=uint16_t(int(core.manager_records[0x5c])*1000+1000);core.manager_records[0x2ac]=uint8_t(sold);core.manager_records[0x2ad]=uint8_t(sold>>8);typed.world.managers[0].career->season_services->tickets_sold=sold;
            if(week==11){core.manager_records[0x34]=1;core.manager_records[0x54]=7;core.manager_records[0x44]=0x30;}
            core.mailbox.assign(kMailboxEntries*kMailboxEntry,0);initialize_career_economy(typed,core);
            typed.game_seed=17;typed.watcom_seed=seed;GameRandom cg(17);WatcomRandom cw(seed);MinuteReport report;
            WeeklyIncidentContext context{typed.date,typed.season,{}};WeeklyClubRelations relation;relation.club=typed.world.managers[0].club;const auto team=core.manager_teams[0];
            const uint32_t lists[]={layout::Career_CompetitionPointersEnglish_ExecutableField25,layout::Career_CompetitionPointersEnglish_RuntimeField14,layout::Career_CompetitionPointersEnglish_ExecutableField19};
            const uint32_t counts[]={layout::Career_EuropeanClub_ExecutableField,layout::Career_EuropeanClub_ExecutableField2,layout::Career_EuropeanClub_ExecutableField3};
            for(size_t cup=0;cup<3&&!relation.advertising.european_cup;++cup)for(size_t n=0;n<core.memory.u8(counts[cup]);++n)if(core.memory.u16(lists[cup]+uint32_t(n*2))==team){relation.advertising.european_cup=uint8_t(cup+1);break;}
            relation.advertising.champion=core.memory.u16(layout::Cup_AdvanceCup_RuntimeField6)==team;const uint32_t entries[]={0,15,19,21};
            for(size_t k=0;k<4;++k)relation.advertising.holders[k]=core.memory.u16(layout::Season_CareerSeasonSetup_RuntimeField+entries[k]*0x28)==team;
            context.clubs.push_back(relation);WeeklyIncidentHooks hooks;hooks.answer=[&](const GameState&,uint8_t seat,const WeeklyOfferQuestion&)->std::optional<bool>{if(seat!=0)throw std::runtime_error("weekly oracle got wrong global manager");return answer;};
            manager_offers(core,0,cw,report);facility_ageing(core,0,cg,cw,report);facility_damage(core,0,cg,cw);if(core.memory.u8(layout::Savestate_Layout_ExecutableField)%3==0)advertising_offers(core,0,cw,report);
            if(report.unsupported)throw std::runtime_error(report.reason);
            const auto result=begin_weekly_managers(typed,reference,texts,context,hooks);questions+=result.questions;credit_changes+=core.memory.u32(layout::Loans)!=100000;
            if(typed.game_seed!=cg.seed()||typed.watcom_seed!=cw.seed())throw std::runtime_error("weekly incidents RNG differs from full Core");
            if(typed.world.managers[0].career!=manager_career_state_from_legacy(core,0,players,true)||typed.world.managers[0].finance!=manager_initial_finance_from_legacy(core,0)||typed.world.managers[0].tactics!=manager_full_tactics_from_legacy(core,0,players))throw std::runtime_error("weekly incident stadium/sponsor/account/board differs");
            const auto s=*typed.world.settings;auto p=AiMatchProjection::create_for_humans(typed,{relation.club},reference,{s.mode,s.difficulty,s.foreign_rule,true});install_career_economy(p,typed.world);
            for(size_t n=0;n<0xa8;++n)if(p.state.manager_records[n]!=core.manager_records[n])throw std::runtime_error("weekly construction/stand fields differ");
            for(size_t n=0;n<10;++n)if(p.state.manager_records[0x170+n]!=core.manager_records[0x170+n])throw std::runtime_error("weekly offer state differs");
            for(size_t n=0;n<30;++n)if(p.state.memory.u32(layout::Loans+uint32_t(n*4))!=core.memory.u32(layout::Loans+uint32_t(n*4)))throw std::runtime_error("weekly loan-rate change differs");
            if(compare_mailbox(p.state.mailbox,core.mailbox))throw std::runtime_error("weekly incident mailbox differs");
            if(uint32_t(typed.world.transfer_market->checkpoint_game_seed)!=core.memory.u32(layout::CareerSession_Invalid_RuntimeField2))throw std::runtime_error("weekly incident market checkpoint differs");
            if(read_save(make_save(typed))!=typed)throw std::runtime_error("weekly incident snapshot differs");
            if(!proof.empty()&&week==11&&seed==91&&answer){const auto path=proof/"weekly-incidents.FBM";write_savegame(path,make_save(typed));if(read_save(load_savegame(path))!=typed)throw std::runtime_error("weekly incident disk snapshot differs");}
            ++passes;
        }
        if(!questions||!credit_changes)throw std::runtime_error("weekly incident oracle did not exercise a human choice");
        std::cout<<passes<<" full-Core weekly incident comparisons: both decisions, stadium decay/damage, ticket refunds, advertising, mailbox and RNG match; questions="<<questions<<'\n';
    }
    {
        size_t passes=0;size_t injuries=0,recoveries=0,retirements=0;
        std::vector<uint32_t> weekly_seeds{1u,91u,703u,1777u};
        const int doctor=int16_t(uint16_t(native.manager_records[0x8ae])|uint16_t(native.manager_records[0x8af])<<8);
        for(uint32_t seed=1;;++seed){WatcomRandom probe(seed);probe.range(0,6-doctor/0x113);if(!probe.range(0,0x25)){weekly_seeds.push_back(seed);break;}}
        for(int mode:{1,3})for(uint32_t seed:weekly_seeds) {
            auto classic=native;auto typed=state;typed.world.settings->mode=uint8_t(mode);
            typed.game_seed=17;typed.watcom_seed=seed;classic.memory.set8(layout::GameMode,uint8_t(mode));
            classic.mailbox.assign(kMailboxEntries*kMailboxEntry,0);
            // Include a known final-week absence and a severe injury. They
            // exercise recovery, physician odds, retirement and resulting mail.
            for(size_t r:{0u,7u}) {
                const auto player=classic.squad_slots[r]&0xfff;
                classic.players[player].w35c=(classic.players[player].w35c&0x07ffffffu)|(5u<<27);
                classic.players[player].w364=(classic.players[player].w364&0xffe0ffffu)|(uint32_t(r?30:1)<<16);
                auto& typed_player=typed.world.players.at({uint32_t(player)}).attributes;
                typed_player.w35c=classic.players[player].w35c;typed_player.w364=classic.players[player].w364;
            }
            GameRandom cg(17);WatcomRandom cw(seed);MinuteReport report;
            weekly_player_updates(classic,cg,cw,report);if(report.unsupported)throw std::runtime_error(report.reason);
            const auto result=update_weekly_players(typed,reference,texts);
            injuries+=result.injured;recoveries+=result.recovered;retirements+=result.retired;
            const auto compare=[&] {
                if(typed.game_seed!=cg.seed()||typed.watcom_seed!=cw.seed())throw std::runtime_error("weekly/training RNG differs from full Core");
                for(const auto& [id,p]:typed.world.players)if(p.attributes.w35c!=classic.players.at(id.value).w35c||p.attributes.w364!=classic.players.at(id.value).w364||p.attributes.w368!=classic.players.at(id.value).w368||p.cards!=classic.player_cards.at(id.value))throw std::runtime_error("weekly/training player differs from full Core");
                for(size_t team=0;team<164;++team){const auto id=identities.at(classic.club_ids.at(team));const auto& roster=typed.world.clubs.at(id).roster;
                    for(size_t r=0;r<24;++r){const auto raw=classic.squad_slots.at(team*24+r);const auto flags=uint16_t(((raw&0x8000)?1:0)|((raw&0x4000)?2:0)|((raw&0x2000)?4:0)|((raw&0x1000)?8:0));
                        if((roster[r].empty()?0xfffu:roster[r].player.value)!=(raw&0xfff)||roster[r].flags!=flags)throw std::runtime_error("weekly canonical roster differs from full Core");}
                }
                if(typed.world.managers[0].tactics!=manager_full_tactics_from_legacy(classic,0,players))throw std::runtime_error("weekly/training full board differs from Core");
                if(read_save(make_save(typed))!=typed)throw std::runtime_error("weekly/training snapshot differs");
            };
            compare();
            if(!proof.empty()&&mode==3&&seed==91){const auto path=proof/"weekly-players.FBM";write_savegame(path,make_save(typed));if(read_save(load_savegame(path))!=typed)throw std::runtime_error("weekly player disk snapshot differs");}
            for(size_t row=0;row<result.notices.size();++row){const auto at=row*kMailboxEntry;const auto& note=result.notices[row];
                if(classic.mailbox.at(at)!=note.recipients||note.date!=typed.date||!std::equal(note.text.begin(),note.text.end(),classic.mailbox.begin()+ptrdiff_t(at+10)))throw std::runtime_error("weekly notice differs from full Core");
            }
            if(result.notices.size()<kMailboxEntries&&classic.mailbox.at(result.notices.size()*kMailboxEntry))throw std::runtime_error("typed weekly lost a Core notice");
            familiarity_decay(classic,cg,cw);train_squad(classic,0,cg,cw);
            const auto trained=train_manager_squads(typed,reference);
            if(trained.managers!=1)throw std::runtime_error("weekly training skipped human manager");
            compare();
            if(!proof.empty()&&mode==3&&seed==91){const auto path=proof/"weekly-training.FBM";write_savegame(path,make_save(typed));if(read_save(load_savegame(path))!=typed)throw std::runtime_error("weekly training disk snapshot differs");}
            ++passes;
        }
        if(!injuries||!recoveries||!retirements)throw std::runtime_error("weekly oracle did not exercise injury, recovery and retirement");
        std::cout<<passes<<" full-Core weekly-player/training comparisons: "<<injuries<<" injuries, "<<recoveries<<" recoveries, "<<retirements<<" retirements; full boards/rosters/news/RNG/snapshots match\n";
    }
    owner_app::initialize_desktops(state,reference);
    size_t events=0,offers=0,loans=0,lures=0,settlements=0,moved_sales=0,moved_loans=0;
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
        if(found)for(int decision=0;decision<4;++decision){auto core=classic;auto resolved=typed;GameRandom cg(typed.game_seed);WatcomRandom cw(typed.watcom_seed);MinuteReport checked;
            PendingOfferHooks hooks;hooks.answer=[&](const GameState&,const PendingOfferQuestion& q){return decision==2&&q.kind==PendingOfferQuestionKind::severance?PendingOfferAnswer::no:decision==3&&q.kind==PendingOfferQuestionKind::salary_demand?PendingOfferAnswer::no:PendingOfferAnswer::yes;};hooks.select_transfer=[&](const GameState&,const TransferOfferListView&){return PendingOfferTransferChoice{decision==0?PendingOfferTransferChoiceKind::decline:PendingOfferTransferChoiceKind::bid,decision==0?-1:0};};hooks.notice=[](const GameState&,const PendingOfferNotice&){};
            PendingOfferDeskRequest request;request.runtime_mode=PendingOfferDeskRuntimeMode::normal_interactive;request.manager_count=1;request.manager_position=0;
            const auto expected=process_natural_pending_offers_after_desk(core,request,cg,cw,hooks,checked);if(checked.unsupported||!expected.completed)throw std::runtime_error("Native pending fixture: "+checked.reason);
            owner_app::ManagerActionOwner tx{93,0};const auto result=owner_app::resolve_initial_offer(resolved,tx,tx,reference,texts,hooks);
            if(result.completed!=expected.completed||result.transferred!=expected.transferred||result.salary_accepted!=expected.salary_accepted||resolved.game_seed!=cg.seed()||resolved.watcom_seed!=cw.seed())throw std::runtime_error("pending result/RNG differs at seed "+std::to_string(seed)+" choice "+std::to_string(decision));
            for(const auto& [id,p]:resolved.world.players)if(p.attributes.w35c!=core.players.at(id.value).w35c||p.attributes.w364!=core.players.at(id.value).w364||p.attributes.w368!=core.players.at(id.value).w368||p.cards!=core.player_cards.at(id.value)||p.bonus!=core.player_bonus.at(id.value))throw std::runtime_error("pending player attributes/cards differ");
            for(size_t team=0;team<164;++team){const auto id=identities.at(core.club_ids.at(team));const auto& roster=resolved.world.clubs.at(id).roster;for(size_t r=0;r<24;++r){const auto raw=core.squad_slots.at(team*24+r);if((roster[r].empty()?0xfffu:roster[r].player.value)!=(raw&0xfff))throw std::runtime_error("pending canonical roster differs");}}
            for(size_t n=0;n<4;++n){const auto local=core.memory.u16(layout::EditorList+40*0x58+uint32_t(n*2));const auto expected=local==0xffff?ClubId{}:identities.at(core.club_ids.at(local));if(!resolved.world.transfer_market->entry40_history_known||resolved.world.transfer_market->last_entry40_bidders[n]!=expected)throw std::runtime_error("resolved offer lost retained special bidder history");}
            if(resolved.world.managers[0].finance!=manager_initial_finance_from_legacy(core,0))throw std::runtime_error("pending paid account differs");
            if(resolved.world.managers[0].tactics!=manager_full_tactics_from_legacy(core,0,players))throw std::runtime_error("pending complete contract/tactics board differs");
            if(!resolved.transfers||uint32_t(resolved.transfers->balance[0])!=uint32_t(core.manager_records[0x330]|uint32_t(core.manager_records[0x331])<<8|uint32_t(core.manager_records[0x332])<<16|uint32_t(core.manager_records[0x333])<<24)||resolved.desktop->initial_event->offer||resolved.world.transfer_market->rows[40])throw std::runtime_error("pending transfer account/cleanup differs");
            if(result.transferred){
                const auto read32=[&](size_t at){return uint32_t(core.manager_records.at(at))|uint32_t(core.manager_records.at(at+1))<<8|uint32_t(core.manager_records.at(at+2))<<16|uint32_t(core.manager_records.at(at+3))<<24;};
                const auto& history=resolved.transfers->history[0];
                if(history.size()!=1||history[0].player!=row->player||history[0].other!=row->bids[0].bidder||history[0].source_word!=read32(0x760))throw std::runtime_error("pending qualified transfer history differs");
                if(row->bids[0].months&128){++moved_loans;
                    if(resolved.world.loans.size()!=1||resolved.transfers->loans.size()!=1)throw std::runtime_error("accepted loan lost contract");
                    const auto& contract=resolved.transfers->loans[0];int slot=-1;
                    for(int n=0;n<12;++n)if(core.memory.u16(layout::Career_CareerManagersEnd_RuntimeField+uint32_t(n*2))==row->player.value){slot=n;break;}
                    if(slot<0||contract.player!=row->player||contract.manager!=0)throw std::runtime_error("pending loan owner/player differs");
                    const auto at=layout::ClubInfo_FormatError_RuntimeField+uint32_t(slot*0x58);
                    for(size_t n=0;n<0x58;++n)if((n<0x44||n>=0x48)&&contract.board[n]!=core.memory.u8(at+uint32_t(n)))throw std::runtime_error("pending loan owner board differs");
                    const auto mark=core.memory.u16(at+0x44),marked=core.memory.u16(at+0x46);
                    if(contract.mark!=(mark==0xfff?PlayerId{}:PlayerId{mark})||contract.marked_by!=(marked==0xfff?PlayerId{}:PlayerId{marked}))throw std::runtime_error("pending loan board identity differs");
                }else ++moved_sales;
            }
            if(read_save(make_save(resolved))!=resolved)throw std::runtime_error("pending history/loan snapshot differs");++settlements;
        }
        if(read_save(make_save(typed))!=typed)throw std::runtime_error("initial event snapshot differs");++events;
    }
    std::cout<<settlements<<" full-Core pending decisions, "<<moved_sales<<" actual sales and "<<moved_loans<<" actual loans compared with canonical rosters/accounts/RNG/snapshots\n";
    if(offers<16||!loans||!lures||!moved_sales||!moved_loans)throw std::runtime_error("initial event oracle did not exercise all offer kinds");
    std::cout<<events<<" Native first-event comparisons, "<<offers<<" offers including "<<loans<<" loan terms and "<<lures<<" salary-only approaches: quotes/bidders/terms/warnings/order/prestige/both RNGs/snapshots match\n";
    {auto loop=state;auto classic=native;const auto model=loop.world.managers.front();loop.world.settings->mode=1;loop.world.settings->planned_managers=4;
        classic.memory.set8(layout::GameMode,1);classic.memory.set8(layout::ManagerCount,4);classic.memory.set8(layout::OriginalManagerCount,4);
        int next_team=0;for(uint8_t seat=1;seat<4;++seat){while(classic.manager_of(next_team)>=0)++next_team;const int team=next_team++;
            auto m=model;m.seat=seat;m.name="Loop "+std::to_string(seat);m.portrait=seat;m.club=identities.at(classic.club_ids.at(size_t(team)));m.start_league=loop.world.clubs.at(m.club).league;loop.world.managers.push_back(m);
            classic.manager_teams.push_back(team);classic.team_morale.at(size_t(team))|=0x80;
            std::copy_n(classic.manager_records.begin(),0xb0c,classic.manager_records.begin()+size_t(seat)*0xb0c);
            std::copy_n(classic.tactics.begin(),0x840,classic.tactics.begin()+size_t(seat)*0x840);std::copy_n(classic.manager_options.begin(),5,classic.manager_options.begin()+size_t(seat)*5);std::copy_n(classic.manager_ratings.begin(),60,classic.manager_ratings.begin()+size_t(seat)*60);
            const auto rec=size_t(seat)*0xb0c;classic.manager_records[rec+0x290]=uint8_t(team);classic.manager_records[rec+0x291]=uint8_t(team>>8);classic.manager_records[rec+0x221]=seat;
            for(size_t n=0;n<25;++n)classic.manager_records[rec+0xa8+n]=n<m.name.size()?uint8_t(m.name[n]):0;
        }
        for(uint32_t n=0;n<4;++n)classic.memory.set16(layout::NextDeskManager+n*2,uint16_t(n));
        loop.desktop.reset();owner_app::initialize_desktops(loop,reference);loop.validate();
        GameRandom g(loop.game_seed);WatcomRandom w(loop.watcom_seed);MinuteReport r;std::vector<uint8_t> refs(50);MatchdayContext context;context.month=7;context.referees=&refs;
        const auto groups=prepare_event(classic,0,context,g,w,r);if(!groups.empty()||r.unsupported)throw std::runtime_error("four-manager oracle event is not empty");
        enter_event_desk(classic,0,groups,g,w,r,true,[](const GameState&,WatcomRandom&){});owner_app::ManagerActionOwner active{92,0};owner_app::prepare_first_desk_event(loop,active,active,reference);
        for(int position=0;position<4;++position){const auto& event=*loop.desktop->initial_event;
            if(event.manager_position!=position||loop.desktop->active_seat!=classic.memory.u8(layout::ActiveManager)||loop.game_seed!=g.seed()||loop.watcom_seed!=w.seed())throw std::runtime_error("four-manager cursor/RNG differs from full Core");
            for(int n=0;n<4;++n)if(event.manager_order[size_t(n)]!=classic.memory.u16(layout::NextDeskManager+uint32_t(n*2)))throw std::runtime_error("four-manager order was reshuffled");
            const auto seat=*loop.desktop->active_seat;const auto& desk=loop.desktop->managers.at(seat);
            if(desk.prestige!=classic.manager_records.at(size_t(seat)*0xb0c+0x211))throw std::runtime_error("next manager prestige differs");
            for(size_t row=0;row<54;++row)for(size_t b=0;b<16;++b)if(desk.layout[row][b]!=classic.memory.u8(layout::Desktops+uint32_t(size_t(seat)*0x360+row*16+b)))throw std::runtime_error("next manager Source layout differs");
            if(read_save(make_save(loop))!=loop)throw std::runtime_error("four-manager saved cursor differs");
            if(position<3){PendingOfferDeskRequest request;request.runtime_mode=PendingOfferDeskRuntimeMode::normal_interactive;request.manager_count=4;request.manager_position=position;
                const auto pending=process_natural_pending_offers_after_desk(classic,request,g,w,{},r);if(!pending.completed||r.unsupported)throw std::runtime_error("four-manager native after-desk path unsupported");
                enter_event_desk(classic,position+1,groups,g,w,r,true,[](const GameState&,WatcomRandom&){});owner_app::advance_initial_manager(loop,active,active,reference);}
        }
        std::cout<<"Four-manager full-Core desk loop: order/cursor, after-desk path, all layouts/prestige, both RNGs and mid-loop saved snapshots match\n";
    }
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
        owner_app::initialize_first_season_market(scoped,session,session,&reference);
        if(!scope.germany)for(const auto& field:scoped.world.initial_competitions->fields)for(auto id:field)if(scoped.world.clubs.at(id).country==Country::germany)throw std::runtime_error("disabled German club leaked into actual English-only European field");
        if(read_save(make_save(scoped))!=scoped)throw std::runtime_error("actual scoped initial competition snapshot differs");++scopes;
    }
    std::cout<<scopes<<" actual-data initial competition scopes with supplied German/English rules, calendars and snapshot roundtrips passed\n";
    std::cout<<"Native initial competition boundary: national/European/domestic fields, campaigns, card rows, both RNGs, calendar and snapshot match; G="<<result.game_draws<<" W="<<result.watcom_draws<<'\n';
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
