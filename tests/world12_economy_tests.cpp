// SPDX-License-Identifier: GPL-3.0-only
#include "world12_fixture.hpp"
#include "fixture_tables.hpp"
#include "game/world12/economy.hpp"
#include "game/world12/manager_tactics.hpp"
#include "game/world12/codec.hpp"
#include "game/world12/pools.hpp"
#include "game/messages.hpp"
#include "game/bank_robbery.hpp"
#include "cli/json.hpp"
#include "formats/textd.hpp"
#include <iostream>
#include <algorithm>
namespace {
using namespace bmh;using namespace bmh::world12;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class F>void rejects(F f,const char* message){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}check(rejected,message);}
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
CareerSnapshot* interrupted=nullptr;
void interrupt(int){if(interrupted){++interrupted->watcom_seed;interrupted=nullptr;}}
void portfolios(CareerSnapshot state,const GameState& rules) {
    auto& markets=*state.world.markets;
    const auto put=[](auto& bytes,size_t offset,uint32_t value,size_t width){for(size_t b=0;b<width;++b)bytes[offset+b]=uint8_t(value>>(b*8));};
    const auto read=[](const auto& bytes,size_t offset,size_t width){uint32_t value=0;for(size_t b=0;b<width;++b)value|=uint32_t(bytes[offset+b])<<(b*8);return value;};
    for(size_t n=0;n<4;++n) {
        for(size_t share=0;share<12;++share){put(markets.stocks,share*0x5c+4+n*4,100000+share*1000+n*111,4);put(markets.stocks,share*0x5c+0x50+n*2,share*10+n+1,2);}
        for(size_t fund=0;fund<6;++fund){put(markets.funds,fund*0x1c+4+n*4,200000+fund*1000+n*222,4);put(markets.funds,fund*0x1c+0x14+n*2,fund*10+n+1,2);}
    }
    const auto original=markets;
    auto p=AiMatchProjection::create_for_humans(state,{state.world.managers[0].club,state.world.managers[1].club},rules,{1,3,false,true});
    install_career_economy(p,state.world);
    check(read(p.state.stocks,4,4)==read(original.stocks,8,4)&&read(p.state.stocks,8,4)==read(original.stocks,16,4)&&
          p.state.memory.u32(layout::FundBlock+4)==read(original.funds,8,4)&&p.state.memory.u32(layout::FundBlock+8)==read(original.funds,16,4),
          "sparse managers borrowed another seat's paid investment totals");
    capture_career_portfolios(state.world,p);check(markets==original,"unchanged sparse portfolios failed exact roundtrip");
    put(p.state.stocks,4,87654321,4);put(p.state.stocks,0x50,321,2);
    p.state.memory.set32(layout::FundBlock+8,76543210);p.state.memory.set16(layout::FundBlock+0x16,123);
    capture_career_portfolios(state.world,p);
    auto expected=original;put(expected.stocks,8,87654321,4);put(expected.stocks,0x52,321,2);
    put(expected.funds,16,76543210,4);put(expected.funds,0x1a,123,2);
    check(markets==expected&&decode_world(encode_world(state.world))==state.world,
          "portfolio commit lost paid/holding identities, untouched seats, market history or save data");
}
void advertising_groups(CareerSnapshot state,const GameState& rules) {
    state.world.economy->advertising_group=254;
    auto projection=AiMatchProjection::create_for_humans(state,{state.world.managers[0].club,state.world.managers[1].club},rules,{1,3,false,true});
    install_career_economy(projection,state.world);check(projection.state.memory.u8(layout::AdGroupId)==254,"advertising group lost high byte on projection");
    projection.state.memory.set8(layout::AdGroupId,255);capture_career_economy(state.world,projection);
    check(state.world.economy->advertising_group==255&&decode_world(encode_world(state.world))==state.world,"advertising group failed capture/save roundtrip");
    const auto encoded=encode_world(state.world);auto old=cli::parse_json(std::string(encoded.begin(),encoded.end()));old.object()->at("version")=27;
    auto text=cli::dump_json(old);rejects([&]{decode_world(Bytes(text.begin(),text.end()));},"schema27 discarded a known advertising counter");
    old.object()->at("economy").object()->erase("advertising_group");text=cli::dump_json(old);state.world=decode_world(Bytes(text.begin(),text.end()));
    check(!state.world.economy->advertising_group,"older economy invented a bundle counter");
    install_career_economy(projection,state.world);capture_career_economy(state.world,projection);
    check(!state.world.economy->advertising_group,"ordinary projection initialized an unknown bundle counter");
}
void pools_settlement(CareerSnapshot state,const GameState& rules) {
    set_fixture_tables(bmh::test::authored_fixture_tables());
    state.world.economy->pools.emplace();auto& ledger=*state.world.economy->pools;ledger.previous_outcomes.fill(2);
    PreparedEvent event;event.day=uint16_t(state.date);event.desk_phase=PreparedEvent::DeskPhase::complete;event.pools_matches.emplace();
    auto fixtures=league_fixtures(state.world,{0},1);const auto extra=league_fixtures(state.world,{1},1);fixtures.insert(fixtures.end(),extra.begin(),extra.begin()+2);event.pools_matches=fixtures;
    check(fixtures.size()==11,"synthetic settlement requires eleven real catalogue fixtures");
    std::vector<PoolsMatchOutcome> receipts;for(const auto& f:fixtures)receipts.push_back({f,0});
    TextDb texts;texts.strings.assign(TEXT_COUNT,"Synthetic pools notice");
    for(int hits:{8,9,10,11}) {
        auto before=state;auto& pools=*before.world.economy->pools;pools.appointment=std::make_pair(before.season,before.date);
        pools.managers[1].stake=10000;pools.managers[1].winnings=1234;pools.managers[1].correct=5;
        for(int n=0;n<11;++n)pools.managers[1].tips[size_t(n)]=uint8_t(n<hits?0:1);
        pools.managers[3].stake=10000;pools.managers[3].winnings=4321;pools.managers[3].correct=4;
        auto& german=before.world.managers[0];auto& english=before.world.managers[1];german.finance->balance=1000000;german.finance->credit=0;english.finance->balance=0;english.finance->credit=0;
        const auto seed=before.watcom_seed;const auto game=before.game_seed;auto settled=before;settle_scheduled_pools(settled,event,receipts,rules,texts);
        const auto winnings=hits<9?0u:hits==9?100000u:hits==10?200000u:500000u;
        check(settled.world.managers[0].finance->balance==1000000-10000+int32_t(winnings)&&settled.world.economy->pools->managers[1].winnings==winnings&&settled.world.economy->pools->managers[1].correct==hits&&settled.world.economy->pools->managers[1].stake==0,"Toto quota/hits/account publication differs from Core");
        check(settled.world.managers[1].finance->balance==0&&settled.world.economy->pools->managers[3].stake==0&&settled.world.economy->pools->managers[3].winnings==4321&&settled.world.economy->pools->managers[3].correct==4&&settled.world.economy->messages.size()==before.world.economy->messages.size()+1,"insufficient funds lost old result or failed to post one notice");
        check(!settled.world.economy->pools->appointment&&settled.watcom_seed==seed&&settled.game_seed==game&&read_save(make_save(settled))==settled,"settlement lost owner/save or consumed RNG");
        const auto accepted=settled;settle_scheduled_pools(settled,event,receipts,rules,texts);check(settled==accepted,"pools settled a cleared stake twice");
        auto invalid=receipts;invalid[1]=invalid[0];rejects([&]{settle_scheduled_pools(settled,event,invalid,rules,texts);},"duplicate regulation receipt accepted");check(settled==accepted,"invalid receipt changed saved pools");
    }
    const auto raw=encode_world(state.world);auto old=cli::parse_json(std::string(raw.begin(),raw.end()));old.object()->at("version")=29;auto text=cli::dump_json(old);
    rejects([&]{decode_world(Bytes(text.begin(),text.end()));},"schema29 discarded known coupons");old.object()->at("economy").object()->erase("pools");text=cli::dump_json(old);state.world=decode_world(Bytes(text.begin(),text.end()));
    auto projection=AiMatchProjection::create_for_humans(state,{state.world.managers[0].club,state.world.managers[1].club},rules,{1,3,false,true});install_career_economy(projection,state.world);capture_career_economy(state.world,projection);
    check(!state.world.economy->pools,"ordinary projection invented older coupons");
}
void depreciation(CareerSnapshot state,const GameState& rules) {
    state.world.economy->managers[1].depreciation=0x87654321u;
    state.world.economy->managers[3].depreciation=7654321;
    auto p=AiMatchProjection::create_for_humans(state,{state.world.managers[0].club,state.world.managers[1].club},rules,{1,3,false,true});
    install_career_economy(p,state.world);
    const auto read=[](const Bytes& bytes,size_t at){uint32_t value=0;for(size_t b=0;b<4;++b)value|=uint32_t(bytes.at(at+b))<<(b*8);return value;};
    check(read(p.state.manager_records,0x30c)==0x87654321u&&read(p.state.manager_records,0xb0c+0x30c)==7654321,
          "sparse depreciation accounts lost high words or actor identity");
    capture_career_economy(state.world,p);
    const auto encoded=encode_world(state.world);check(decode_world(encoded)==state.world,"depreciation failed current schema roundtrip");
    auto old=cli::parse_json(std::string(encoded.begin(),encoded.end()));old.object()->at("version")=25;
    for(auto& manager:std::get<cli::Json::Array>(old.object()->at("managers").value))if(manager.object()->at("career").object())manager.object()->at("career").object()->erase("camp_focus");
    auto text=cli::dump_json(old);rejects([&]{decode_world(Bytes(text.begin(),text.end()));},"schema25 silently discarded known depreciation");
    auto& managers=std::get<cli::Json::Array>(old.object()->at("economy").object()->at("managers").value);
    for(auto& manager:managers)manager.object()->erase("depreciation");
    text=cli::dump_json(old);state.world=decode_world(Bytes(text.begin(),text.end()));
    check(!state.world.economy->managers[1].depreciation&&!state.world.economy->managers[3].depreciation,
          "schema25 invented a known depreciation balance");
    auto unknown=AiMatchProjection::create_for_humans(state,{state.world.managers[0].club,state.world.managers[1].club},rules,{1,3,false,true});
    install_career_economy(unknown,state.world);capture_career_economy(state.world,unknown);
    check(!state.world.economy->managers[1].depreciation&&!state.world.economy->managers[3].depreciation,
          "ordinary projection silently initialized unknown old depreciation");
}
}
int main()try {
    auto rules=reference();rules.memory.add_block(layout::Savestate_Layout_ExecutableField,1);
    rules.memory.add_block(layout::Loans,4*0x78);rules.mailbox.assign(kMailboxEntries*kMailboxEntry,0);
    auto state=fixture(rules);state.date=0;state.world.settings->difficulty=3;
    initialize_career_economy(state,rules);state.date=40;
    portfolios(state,rules);
    depreciation(state,rules);
    advertising_groups(state,rules);
    pools_settlement(state,rules);
    auto& economy=*state.world.economy;auto& de=economy.managers[1];auto& en=economy.managers[3];
    de.deposit=2000000;de.interest_income=123;de.overdraft_interest=456;
    de.loans[4]={100,120,12,10,2,24};de.loans[3]={200,240,24,10,2,48};
    economy.managers[0].deposit=77;
    state.world.managers[0].finance->balance=1000000;state.world.managers[0].finance->income=0;
    state.world.managers[1].finance->balance=-300000;state.world.managers[1].finance->credit=100000;en.overdraft_interest=100;
    auto& stocks=state.world.markets->stocks;auto& funds=state.world.markets->funds;
    for(size_t share=0;share<12;++share){const auto at=share*0x5c;uint32_t capital=300000;
        for(size_t n=0;n<4;++n)stocks[at+n]=uint8_t(capital>>(8*n));stocks[at+0x4e]=100;stocks[at+0x58]=1;stocks[at+0x59]=3;
        for(size_t seat=0;seat<4;++seat)stocks[at+0x50+seat*2]=uint8_t(seat+1);
    }
    for(size_t f=0;f<6;++f){const size_t at=f*0x1c;funds[at]=0xe8;funds[at+1]=3;for(size_t seat=0;seat<4;++seat)funds[at+0x14+seat*2]=uint8_t(seat+1);}
    EconomyMessage note;note.recipients=1u<<3;note.read_by=1u<<3;note.date=12;note.text[0]='A';economy.messages.push_back(note);economy.recipient_mask=1u<<3;
    const auto initial=state;state.validate();
    auto projection=AiMatchProjection::create_for_humans(state,{state.world.managers[0].club,state.world.managers[1].club},rules,{1,3,false,true});
    install_career_economy(projection,state.world);
    check(projection.state.memory.u32(layout::Loans+2*0x18)==100,"loan lender seat3 was not mapped to local seat1");
    check(projection.state.mailbox[0]==2&&projection.state.mailbox[4]==2,"mail recipients/read flags lost sparse manager identity");
    capture_career_economy(state.world,projection);check(state==initial,"economy projection roundtrip changed global loan/holding/mail identity");
    check(read_save(make_save(state))==state,"economy did not survive snapshot JSON16 roundtrip");

    const auto daily=daily_finance(state,rules,DailyEconomyKind::plain_day);
    check(state.world.managers[0].finance->balance==1000136&&state.world.economy->managers[1].interest_income==259,"daily deposit/account interest is wrong");
    check(state.world.managers[1].finance->balance==-300130&&state.world.economy->managers[3].overdraft_interest==230,"daily overdraft tiers are wrong");
    check(daily.game_draws>0&&!daily.watcom_draws&&state.date==40,"daily finance skipped shares or advanced calendar");
    check(state.world.economy->managers[0]==initial.world.economy->managers[0],"daily finance invented processing for absent manager0");
    for(size_t share=0;share<12;++share)for(size_t seat=0;seat<4;++seat)
        check(state.world.markets->stocks[share*0x5c+0x50+seat*2]==seat+1,"daily stock update moved another manager's holdings");
    auto event=initial;const auto event_phase=daily_finance(event,rules,DailyEconomyKind::after_event);
    check(!event_phase.game_draws&&event.world.markets==initial.world.markets,"post-event interest moved stock prices a second time");
    {
        auto guarded=initial;
        for(auto& manager:guarded.world.managers)manager.finance->balance=250000000;
        const auto grace=daily_finance(guarded,rules,DailyEconomyKind::after_event);
        check(grace.watcom_draws==0&&guarded.world.economy->managers[1].bank_robbery_month==31&&
              guarded.world.economy->managers[3].bank_robbery_month==31&&
              !guarded.world.economy->managers[0].bank_robbery_month,
              "typed unknown histories lose money/draw RNG or sparse manager month keys are misrouted");
        guarded=read_save(make_save(guarded));
        check(daily_finance(guarded,rules,DailyEconomyKind::after_event).watcom_draws==0,
              "typed snapshot load resets the grace-month guard");
        guarded.date=62;
        check(daily_finance(guarded,rules,DailyEconomyKind::after_event).watcom_draws==2&&
              guarded.world.economy->managers[1].bank_robbery_month==62&&
              guarded.world.economy->managers[3].bank_robbery_month==62,
              "typed next-month losses do not process exactly the two sparse human managers");
        const auto charged=guarded;
        guarded=read_save(make_save(guarded));
        check(daily_finance(guarded,rules,DailyEconomyKind::after_event).watcom_draws==0,
              "typed charged-month save/load consumes another robbery draw");
        const auto current=encode_world(charged.world);
        auto previous=cli::parse_json(std::string(current.begin(),current.end()));
        auto& object=*previous.object();object["version"]=21;bmh::test::world12::omit_recent_manager_records(previous);
        auto& managers=std::get<cli::Json::Array>(object.at("economy").object()->at("managers").value);
        for(auto& manager:managers)manager.object()->erase("bank_robbery_month");
        const auto text=cli::dump_json(previous);auto old_world=decode_world(Bytes(text.begin(),text.end()));
        for(const auto& manager:old_world.economy->managers)
            check(!manager.bank_robbery_month,"schema21 invents a past robbery month");
        auto old_snapshot=charged;old_snapshot.world=std::move(old_world);
        check(daily_finance(old_snapshot,rules,DailyEconomyKind::after_event).watcom_draws==0,
              "schema21 upgrade causes an immediate new loss or RNG draw");
        check(read_save(make_save(old_snapshot))==old_snapshot,"upgraded typed month guard is not saved");
    }
    auto& bank=state.world.economy->managers[1];bank.camp_weeks=1;bank.camp_bill=100000;bank.offers[2]=3;bank.weekly_count=52;
    for(size_t n=0;n<52;++n)bank.financial_history[0][n]=uint32_t(n+100);
    const auto before=state;const auto settled=settle_weekly_managers(state,rules);
    check(settled.managers==2&&settled.watcom_draws>=2&&state.world.economy->managers[1].camp_weeks==0&&state.world.economy->managers[1].camp_bill==0,"weekly camp settlement was not retained");
    check(state.world.economy->managers[1].weekly_count==52&&state.world.economy->managers[1].financial_history[0][0]==101,"rolling weekly history lost its shift");
    check(state.world.economy->managers[3].weekly_count==1,"second sparse manager's weekly history was skipped");
    check(state.world.economy->managers[1].financial_history[2][51]==uint32_t(state.world.managers[0].finance->balance),"weekly history records a stale balance");
    check(state.world.economy->managers[1].loans==before.world.economy->managers[1].loans,"weekly statistics changed a loan contract");
    check(read_save(make_save(state))==state,"weekly bank/history state failed snapshot roundtrip");

    CareerEconomy mailbox;
    for(uint16_t n=0;n<45;++n){EconomyMessage m;m.recipients=1u<<3;m.date=n;m.text[0]=uint8_t(n);append_economy_message(mailbox,m);}
    check(mailbox.messages.size()==40&&mailbox.messages.back().date==44&&mailbox.messages.back().recipients==(1u<<3)&&mailbox.recipient_mask==(1u<<3),"persistent mailbox lost capacity/order/global recipients");
    state.world.economy->messages=mailbox.messages;state.world.economy->recipient_mask=mailbox.recipient_mask;
    check(read_save(make_save(state))==state,"full persistent mailbox failed snapshot roundtrip");
    auto changed=initial;interrupted=&changed;draw_hook=interrupt;
    rejects([&]{daily_finance(changed,rules,DailyEconomyKind::plain_day);},"daily finance overwrote external state");draw_hook=nullptr;interrupted=nullptr;
    auto expected=initial;++expected.watcom_seed;check(changed==expected,"daily rollback leaked financial/market changes");
    auto missing=initial;missing.world.economy.reset();const auto untouched=missing;
    rejects([&]{daily_finance(missing,rules,DailyEconomyKind::plain_day);},"missing economy was silently initialized");check(missing==untouched,"invalid phase changed state");
    auto invalid=state;invalid.world.economy->messages[0].recipients=1u<<8;rejects([&]{make_save(invalid);},"unknown mailbox manager accepted");
    invalid=state;invalid.world.economy->managers[1].weekly_count=53;rejects([&]{make_save(invalid);},"oversized weekly history accepted");
    auto legacy_input=initial.world;bmh::test::world12::omit_recent_manager_records(legacy_input);auto raw=encode_world(legacy_input);std::string legacy(raw.begin(),raw.end());const auto version=legacy.find("\"version\":"+std::to_string(kWorldSchemaVersion));check(version!=std::string::npos,"new economy schema version missing");legacy.replace(version,std::string("\"version\":"+std::to_string(kWorldSchemaVersion)).size(),"\"version\":15");
    auto legacy_world=legacy_input;legacy_world.economy.reset();check(decode_world(Bytes(legacy.begin(),legacy.end()))==legacy_world,"previous world schema15 ceased to load");
    {
        auto sparse=initial;auto& economy=*sparse.world.economy;economy.messages.clear();economy.recipient_mask=0;
        for(uint8_t n=0;n<3;++n){EconomyMessage message;message.recipients=2;message.date=uint16_t(10+n);message.text[0]=uint8_t('A'+n);append_economy_message(economy,message);}
        auto raw=career_mailbox_bytes(economy);std::array<uint8_t,0xa0> first;std::copy_n(raw.begin(),first.size(),first.begin());
        check(edit_career_message(economy,1,0,first,MailboxEditKind::delete_for_manager)==MailboxEditFailure::none,"canonical mailbox deletion refused");
        check(economy.messages.size()==3&&economy.messages[0].recipients==0&&economy.messages[0].text[0]=='A'&&economy.messages[2].text[0]=='C',"canonical mailbox lost physical hole or following message");
        check(read_save(make_save(sparse))==sparse,"sparse mailbox snapshot failed");
        auto sparse_legacy=sparse.world;bmh::test::world12::omit_recent_manager_records(sparse_legacy);const auto encoded=encode_world(sparse_legacy);std::string old(encoded.begin(),encoded.end());const auto version=old.find("\"version\":"+std::to_string(kWorldSchemaVersion));check(version!=std::string::npos,"sparse mailbox schema version missing");old.replace(version,std::string("\"version\":"+std::to_string(kWorldSchemaVersion)).size(),"\"version\":20");rejects([&]{decode_world(Bytes(old.begin(),old.end()));},"older schema admitted unrepresentable physical mail holes");

        EconomyMessage next;next.recipients=2;next.date=20;next.text[0]='D';append_economy_message(economy,next);check(economy.messages.size()==3&&economy.messages[0].text[0]=='D'&&economy.messages[1].text[0]=='B',"new mail did not refill first Original hole");
    }
    std::cout<<"Daily interest/shares, sparse global loan/holding/mail maps, weekly camp/history, persistent/sparse mailbox, schemas15/21 and rollback passed\n";
}catch(const std::exception& error){draw_hook=nullptr;std::cerr<<error.what()<<'\n';return 1;}
