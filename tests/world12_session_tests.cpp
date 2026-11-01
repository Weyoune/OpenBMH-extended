// SPDX-License-Identifier: GPL-3.0-only
#include "world12_fixture.hpp"
#include "app/world12_startup.hpp"
#include "app/world12_pending.hpp"
#include "app/world12_desktop.hpp"
#include "app/world12_save_load.hpp"
#include "game/world12/match_projection.hpp"
#include "game/world12/desktop_projection.hpp"
#include "game/desk.hpp"
#include "game/world12/codec.hpp"
#include "game/save.hpp"
#include "formats/data_profile.hpp"
#include "formats/palette.hpp"
#include "ui/pending_offer.hpp"
#include "ui/incoming_transfer.hpp"
#include "ui/world12_desktop.hpp"
#include "game/world12/economy.hpp"
#include "game/world12/desktop_projection.hpp"
#include <fstream>
#include "game/week.hpp"
#include "fixture_tables.hpp"
#include <algorithm>
#include <chrono>
#include <set>
#include <iostream>
namespace {
using namespace bmh;using namespace bmh::world12;namespace session_app=bmh::app::world12;
void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}
template<class F>void rejects(F f,const char* m){bool bad=false;try{f();}catch(const std::exception&){bad=true;}check(bad,m);}
session_app::RuntimeResources synthetic() {
    session_app::RuntimeResources r;r.master=bmh::test::world12::complete_world();
    for(uint32_t i=0;i<54;++i){PlayerRecord p;p.id={1000000+i};p.attributes={"Extra",50u|(60u<<7)|(70u<<14),0,i%4};r.master.players[p.id]=p;}
    auto image=LeImage::from_bytes(layout::GermanDataSegmentBegin,Bytes(0x6000));const auto byte=[&](uint32_t at,uint8_t v){image.patch(at,&v,1);};
    for(uint32_t i=0;i<9;++i){byte(layout::LeagueStrengthRanges+i*2,25);byte(layout::LeagueStrengthRanges+i*2+1,75);}
    for(uint32_t league=0;league<4;++league){byte(layout::Career_BuildStadium_RuntimeField4+league,5);for(uint32_t i=0;i<22;++i)byte(layout::Career_BuildStadium_RuntimeField+league*23+i,1);}
    for(uint32_t i=0;i<22;++i){byte(layout::Career_BuildStadium_RuntimeField2+i*5,2);byte(layout::Career_BuildStadium_RuntimeField3+i*5,5);}
    r.german=image;r.english=image;r.reference.stocks.resize(12*0x5c);r.reference.memory.add_block(layout::FundBlock,6*0x1c);r.reference.formations.resize(0x580);r.reference.manager_records.resize(5*0xb0c);r.reference.tactics.resize(4*0x840);r.reference.manager_options.resize(20);r.reference.manager_ratings.resize(240);
    const uint8_t depths[]={0,5,5,5,5,10,10,10,10,17,17};for(size_t m=0;m<4;++m){r.reference.manager_options[m*5]=1;for(size_t mode=0;mode<2;++mode)for(size_t f=0;f<8;++f)for(size_t row=0;row<11;++row)r.reference.formations[m*0x160+mode*0xb0+f*22+row]=depths[row];}
    r.reference.memory.add_block(layout::EnvironmentYouthTeams,4*0x24);return r;
}
void transfer_snapshot_tests(CareerSnapshot state) {
    const auto manager=state.world.managers.front();
    const auto borrower=std::find_if(state.world.clubs.begin(),state.world.clubs.end(),[&](const auto& entry){return entry.first!=manager.club&&std::none_of(state.world.managers.begin(),state.world.managers.end(),[&](const auto& m){return m.club==entry.first;});})->first;
    auto& seller=state.world.clubs.at(manager.club).roster;
    auto& buyer=state.world.clubs.at(borrower).roster;
    const auto incoming=std::find_if(buyer.begin(),buyer.end(),[](const auto& slot){return slot.empty();});
    check(incoming!=buyer.end(),"loan snapshot fixture has no borrower roster slot");
    const PlayerId player{0xf0000101u};auto record=state.world.players.at(seller[0].player);
    record.id=player;state.world.players.emplace(player,record);*incoming={player,0};
    state.world.loans.push_back({player,manager.club,borrower,6});
    state.transfers.emplace();auto& runtime=*state.transfers;
    runtime.balance={-1000000,0,2000000,-2147483647};
    runtime.history[manager.seat].push_back({player,borrower,0xabcdef01});
    LoanContract contract;contract.player=player;contract.manager=manager.seat;
    contract.board.fill(17);for(size_t b=0x44;b<0x48;++b)contract.board[b]=0;
    contract.mark=seller[0].player;contract.marked_by=seller[1].player;
    runtime.loans.push_back(contract);
    check(read_save(make_save(state))==state,"S9 lost global loan/history refs, signed balance or owner board");
    auto old_contract=make_save(state);old_contract.stream.pop_back();old_contract.stream[7]='9';
    check(read_save(old_contract)==state,"previous S9 contract layout ceased to load");
    auto invalid=state;invalid.transfers->loans[0].board[0x44]=1;
    rejects([&]{make_save(invalid);},"packed local player reference leaked into S9 backing");
    invalid=state;invalid.transfers->loans[0].mark={0xfffffffeu};
    rejects([&]{make_save(invalid);},"unknown global loan mark accepted");
    invalid=state;invalid.transfers->loans.push_back(contract);
    rejects([&]{make_save(invalid);},"duplicate saved loan contract accepted");
    invalid=state;invalid.transfers->loans[0].manager=4;
    rejects([&]{make_save(invalid);},"invalid loan manager accepted");
    invalid=state;invalid.world.loans.clear();
    rejects([&]{make_save(invalid);},"owner contract without active loan accepted");
    invalid=state;invalid.transfers->history[manager.seat][0].other={0xfffffffeu};
    rejects([&]{make_save(invalid);},"unknown transfer-history club accepted");
    auto truncated=make_save(state);truncated.stream.pop_back();
    rejects([&]{read_save(truncated);},"truncated S9 loan contract accepted");
    auto previous=state;previous.transfers.reset();previous.world.loans.clear();
    auto s8=make_save(previous);s8.stream.pop_back();s8.stream[7]='8';
    check(read_save(s8)==previous,"previous S8 snapshot ceased to load");
}
unsigned resume_draws=0;
void count_resume_draw(int){++resume_draws;}
session_app::ManagerActionOwner* interrupted_desktop_owner=nullptr;
void interrupt_desktop(int){if(interrupted_desktop_owner){++interrupted_desktop_owner->revision;interrupted_desktop_owner=nullptr;}}
void desktop_tests(CareerSnapshot prepared,const GameState& source,const std::filesystem::path& output={}) {
    auto reference=source;reference.memory.add_block(layout::Desktops,4*0x360);
    reference.memory.add_block(layout::CareerDuration,1);
    reference.club_ids.resize(445);reference.team_flags.resize(445);reference.team_morale.resize(445);
    if(!prepared.desktop)session_app::initialize_desktops(prepared,reference);
    const auto before=prepared;
    for(const auto& manager:before.world.managers){
        auto state=before;session_app::ManagerActionOwner owner{77,8};const auto settings=*state.world.settings;
        std::vector<ClubId> clubs;for(const auto& m:state.world.managers)clubs.push_back(m.club);
        auto oracle=AiMatchProjection::create_for_humans(state,clubs,reference,{settings.mode,settings.difficulty,settings.foreign_rule,true});
        size_t local=0;while(oracle.global_manager(uint8_t(local))!=manager.seat)++local;
        oracle.state.memory.set8(layout::CareerDuration,settings.duration);
        GameRandom game(state.game_seed);WatcomRandom random(state.watcom_seed);bool autosave=false;
        bmh::enter_desk(oracle.state,int(local),game,random,[&](const GameState&,WatcomRandom&){autosave=true;});
        const auto outcome=session_app::enter_initial_manager_desktop(state,owner,{77,8},manager.seat,reference);
        check(owner.revision==9&&state.game_seed==game.seed()&&state.watcom_seed==random.seed()&&outcome.game_draws==game.calls()&&outcome.watcom_draws==random.calls(),"desktop entry lost Original random stream/owner");
        const auto& runtime=*state.desktop;const auto& desk=*std::find_if(runtime.managers.begin(),runtime.managers.end(),[&](const auto& d){return d.seat==manager.seat;});
        check(runtime.active_seat==manager.seat&&desk.prestige==oracle.state.manager_records[local*0xb0c+0x211]&&runtime.autosave_month==oracle.state.autosave_month&&runtime.autosave_months==oracle.state.autosave_months&&runtime.autosave_pending==autosave&&outcome.autosave_due==autosave,"desktop lost prestige/autosave/active manager");
        for(size_t r=0;r<54;++r)for(size_t b=0;b<16;++b){auto expected=oracle.state.memory.u8(layout::Desktops+uint32_t(local*0x360+r*16+b));if(state.world.manager_country(manager.seat)==Country::england&&r==10&&b==1)expected|=0x10;check(desk.layout[r][b]==expected,"desktop layout differs from Original country policy");}
        check(state.date==before.date&&state.calendar==before.calendar&&state.results==before.results&&state.world==before.world,"desktop entry advanced time/results or mutated unrelated world state");
        check(read_save(make_save(state))==state,"entered desktop did not survive save roundtrip");
        if(!output.empty()){
            std::filesystem::create_directories(output);
            const auto path=output/("desk-seat-"+std::to_string(manager.seat)+".FBM");write_savegame(path,make_save(state));
            check(read_save(load_savegame(path))==state,"entered desktop on-disk roundtrip differs");
            std::ofstream receipt(output/("desk-seat-"+std::to_string(manager.seat)+".txt"));
            receipt<<"seat="<<int(manager.seat)<<" country="<<int(state.world.manager_country(manager.seat))<<" prestige="<<int(desk.prestige)
                <<" G="<<state.game_seed<<" W="<<state.watcom_seed<<" Gdraws="<<outcome.game_draws<<" Wdraws="<<outcome.watcom_draws<<" date="<<state.date<<'\n';
        }
        const auto entered=state;rejects([&]{session_app::enter_initial_manager_desktop(state,owner,{77,8},manager.seat,reference);},"stale desktop entry accepted");
        rejects([&]{session_app::enter_initial_manager_desktop(state,owner,owner,manager.seat,reference);},"initial desktop entry replay accepted");check(state==entered&&owner.revision==9,"rejected desktop entry changed state");
    }
    {auto interrupted=before;session_app::ManagerActionOwner owner{77,8};interrupted_desktop_owner=&owner;draw_hook=interrupt_desktop;
        rejects([&]{session_app::enter_initial_manager_desktop(interrupted,owner,{77,8},before.world.managers.front().seat,reference);},"desktop overwrote externally updated owner");draw_hook=nullptr;interrupted_desktop_owner=nullptr;
        check(interrupted==before&&owner.revision==9,"desktop rollback lost external revision or leaked staged state");}
    {auto pending=before;auto automatic=reference;automatic.memory.add_block(layout::LeaguePresentationOptions,0x62);automatic.memory.set8(layout::Desk_EnterDesk_RuntimeField,1);
        session_app::ManagerActionOwner owner{77,8};const auto outcome=session_app::enter_initial_manager_desktop(pending,owner,owner,0,automatic);
        check(outcome.autosave_due&&pending.desktop->autosave_pending&&pending.desktop->autosave_month==7&&pending.desktop->autosave_months==0,"initial monthly autosave was not retained for file owner");
        check(read_save(make_save(pending))==pending,"pending autosave lost on reload");}
    auto invalid=before;invalid.desktop->managers.push_back(invalid.desktop->managers.front());rejects([&]{make_save(invalid);},"duplicate saved desktop accepted");
    invalid=before;invalid.desktop->active_seat=4;rejects([&]{make_save(invalid);},"unknown active desktop accepted");
    auto truncated=make_save(before);truncated.stream.pop_back();rejects([&]{read_save(truncated);},"truncated desktop snapshot accepted");
    auto s5=make_save(before);s5.stream.pop_back();const size_t count=before.desktop->managers.size(),stride=54*16+4,start=s5.stream.size()-1-count*stride;
    for(size_t i=count;i-->0;)s5.stream.erase(s5.stream.begin()+start+i*stride+2,s5.stream.begin()+start+i*stride+4);
    auto s6=s5;s6.stream[7]='6';check(read_save(s6)==before,"previous S6 snapshot ceased to load");
    s5.stream[7]='5';s5.stream.pop_back();s5.checksum=0;for(auto b:s5.stream)s5.checksum=uint8_t(s5.checksum+b);check(read_save(s5)==before,"previous S5 desktop snapshot ceased to load");
    auto old=before;old.desktop.reset();auto legacy=make_save(old);legacy.stream.pop_back();legacy.stream[7]='4';legacy.stream.pop_back();legacy.checksum=0;for(auto b:legacy.stream)legacy.checksum=uint8_t(legacy.checksum+b);check(read_save(legacy)==old,"previous S4 snapshot ceased to load");
}
CareerSnapshot first_event_fixture(CareerSnapshot state,const GameState& reference) {
    InitialCompetitionFields fields;std::vector<ClubId> clubs;for(const auto& [id,club]:state.world.clubs)if(club.kind==ClubKind::club)clubs.push_back(id);
    size_t at=0;const size_t sizes[]={64,24,32};for(size_t k=0;k<3;++k){for(size_t i=0;i<sizes[k];++i)fields.fields[k].push_back(clubs.at(at++));fields.holders[k]=fields.fields[k].front();}
    if(state.world.catalogue.scope.international)for(uint32_t i=0;i<4;++i){ClubRecord nation;nation.id={3000000000u+i};nation.name="Nation "+std::to_string(i);nation.crest=state.world.clubs.begin()->second.crest;nation.kind=ClubKind::national_team;state.world.clubs[nation.id]=nation;fields.national_field.push_back(nation.id);}
    for(auto& manager:state.world.managers)manager.career->season_services.emplace();fields.manager_services_complete=true;state.world.initial_competitions=fields;state.world.transfer_market.emplace();
    SeasonCalendar dates;for(const auto& league:state.world.catalogue.leagues)for(int round=1;round<=league.rounds();++round)dates.leagues.push_back({uint16_t(10+round*7),league.id,uint8_t(round)});state.calendar=CalendarCursor{dates,0,0};
    session_app::initialize_desktops(state,reference);state.validate();return state;
}
void scheduled_desk_tests(const CareerSnapshot& pending,const GameState& source) {
    set_fixture_tables(bmh::test::authored_fixture_tables());
    auto reference=source;reference.memory.add_block(layout::Desktops,4*0x360);
    reference.club_ids.resize(445);reference.team_flags.resize(445);reference.team_morale.resize(445);
    auto state=first_event_fixture(pending,reference);state.world.settings->mode=1;
    state.date=17;state.calendar->next_day=17;state.calendar->schedule.leagues.clear();
    for(const auto& league:state.world.catalogue.leagues)for(uint8_t round=1;round<=league.rounds();++round)
        state.calendar->schedule.leagues.push_back({uint16_t(17+(round-1)*7),league.id,round});
    for(const auto& manager:state.world.managers)state.desktop->manager_order.push_back(manager.seat);
    state.desktop->matchday_number=321;
    const auto epoch=calendar_epoch();state.progress=CareerProgress{ProgressPhase::scheduled_event,DayEntry::plain_day,16,0,uint16_t(epoch.year),uint8_t(epoch.weekday),0};
    const auto de=bmh::test::world12::country_rules(false),en=bmh::test::world12::country_rules(true);
    session_app::ManagerActionOwner owner{87,3};
    session_app::prepare_event_weather(state,owner,owner,std::vector<uint8_t>(50,0x24),{de,en});
    const auto previous=state;auto wrong_mode=state;wrong_mode.world.settings->mode=3;
    rejects([&]{session_app::begin_scheduled_desks(wrong_mode,owner,owner,reference);},"scheduled mode-III bypassed its missing offer/coupon owner");
    check(owner.revision==4&&wrong_mode.world.settings->mode==3,"unsupported scheduled mode changed its owner");
    {
        auto interrupted=state;auto interrupted_owner=owner;
        interrupted_desktop_owner=&interrupted_owner;draw_hook=interrupt_desktop;
        rejects([&]{session_app::begin_scheduled_desks(interrupted,interrupted_owner,owner,reference);},"scheduled preparation overwrote an external owner change");
        draw_hook=nullptr;interrupted_desktop_owner=nullptr;
        check(interrupted==state&&interrupted_owner.revision==owner.revision+1,"scheduled rollback lost external revision or published staged changes");
    }
    session_app::begin_scheduled_desks(state,owner,owner,reference);
    check(state.date==previous.date&&state.calendar==previous.calendar&&state.results==previous.results&&
          state.prepared_event->groups==previous.prepared_event->groups&&state.desktop->initial_event&&
          state.prepared_event->desk_phase==PreparedEvent::DeskPhase::desks,"scheduled desks lost date or prepared weather");
    const auto begin=state;rejects([&]{session_app::begin_scheduled_desks(state,owner,owner,reference);},"scheduled desks prepared twice");
    check(state==begin&&owner.revision==5,"duplicate scheduled desk changed state");
    auto corrupt=state;corrupt.prepared_event->desk_phase=PreparedEvent::DeskPhase::complete;
    rejects([&]{make_save(corrupt);},"completed event retained an active manager");
    auto truncated=make_save(state);truncated.stream.pop_back();
    rejects([&]{read_save(truncated);},"truncated scheduled manager SF was accepted");
    std::set<uint8_t> seen;
    for(size_t i=0;i<state.world.managers.size();++i) {
        seen.insert(*state.desktop->active_seat);const auto saved=make_save(state);
        check(saved.stream[7]=='F'&&read_save(saved)==state,"scheduled cursor failed SF roundtrip");
        if(i+1<state.world.managers.size()) {
            const auto before=state;rejects([&]{session_app::finish_scheduled_desks(state,owner,owner);},"scheduled desks skipped another manager");
            check(state==before,"rejected early finish changed current desk");
            session_app::advance_initial_manager(state,owner,owner,reference);
        }
    }
    const auto date=state.date;const auto g=state.game_seed;const auto w=state.watcom_seed;
    session_app::finish_scheduled_desks(state,owner,owner);
    check(seen.size()==state.world.managers.size()&&!state.desktop->active_seat&&!state.desktop->initial_event&&
          state.date==date&&state.game_seed==g&&state.watcom_seed==w&&
          state.prepared_event->desk_phase==PreparedEvent::DeskPhase::complete&&read_save(make_save(state))==state,
          "scheduled desk completion lost cursor, RNG or resume boundary");
    const auto completed=state;rejects([&]{session_app::finish_scheduled_desks(state,owner,owner);},"scheduled desks finished twice");
    check(state==completed,"duplicate completion changed state");
}
void first_event_tests(const CareerSnapshot& pending,const GameState& source) {
    auto reference=source;reference.club_ids.resize(445);reference.team_flags.resize(445);reference.team_morale.resize(445);reference.memory.add_block(layout::Desktops,4*0x360);
    const auto before=first_event_fixture(pending,reference);GameRandom g(before.game_seed);std::vector<uint8_t> order;for(const auto& m:before.world.managers)order.push_back(m.seat);
    for(size_t i=0;i<order.size();++i)std::swap(order[i],order[size_t(g.range(0,int(order.size())-1))]);
    const auto& manager=before.world.managers.at(order.front());
    {auto view=before;view.desktop->active_seat=order.front();auto first=project_manager_desktop(view,reference);const auto untouched=view.world;first.commit_match_state(view.world);check(view.world==untouched,"stable desktop projection changed canonical world on roundtrip");
        const auto club=manager.club;const auto roster=view.world.clubs.at(club).roster;TacticalAction swap;swap.kind=TacticalActionKind::swap;swap.row=1;swap.second_row=2;swap.player=roster[1].player;swap.first_slot=roster[1];swap.second_slot=roster[2];session_app::ManagerActionOwner owner{82,1};
        const auto changed=session_app::apply_manager_tactical_action(view.world,owner,{manager.seat,club,82,1},swap,{3,false,false,false,2});check(changed.allowed,"stable projection swap fixture rejected");
        auto second=project_manager_desktop(view,reference);for(const auto& row:roster)if(!row.empty())check(first.local_player(row.player)==second.local_player(row.player),"desktop player IDs changed when roster rows moved");}

    for(bool offer:{false,true}){
        uint32_t seed=1;for(;;++seed){WatcomRandom probe(seed);probe.range(0,10000);if((probe.range(0,70-int16_t(manager.match_state->staff_ratings[0])/50)==0)==offer)break;}
        auto state=before;state.watcom_seed=seed;session_app::ManagerActionOwner owner{81,4};const auto result=session_app::prepare_first_desk_event(state,owner,owner,reference);
        check(owner.revision==5&&state.desktop->active_seat==order.front()&&state.desktop->initial_event->manager_order==order&&state.desktop->initial_event->offer.has_value()==offer,"initial event lost order/offer/owner");
        check(result.game_draws>=order.size()+100&&result.watcom_draws>1&&state.date==before.date&&state.calendar==before.calendar&&state.results==before.results,"initial event skipped random draws or advanced time");
        check(read_save(make_save(state))==state,"initial event/offer save roundtrip differs");
        {auto previous=state;previous.desktop->manager_order.clear();previous.desktop->matchday_number.reset();auto old=make_save(previous);old.stream.pop_back();const auto& event=*previous.desktop->initial_event;const size_t tail=7+event.manager_order.size()+(event.offer?4:0);old.stream.erase(old.stream.end()-ptrdiff_t(tail)+3);old.stream[7]='7';check(read_save(old)==previous,"S7 first-event snapshot lost its implicit position zero or invented persistent metadata");}

        {auto resources=synthetic();resources.reference=reference;resume_draws=0;draw_hook=count_resume_draw;
            auto resumed=session_app::Session::resume(resources,state,91,100);draw_hook=nullptr;
            check(!resume_draws&&resumed.snapshot()==state&&resumed.revision()==100,"resume regenerated state or consumed RNG");
            ui::SaveLoadRequest request;request.action=ui::SaveLoadAction::save;request.manager_slot=int(*state.desktop->active_seat);request.revision=100;
            WatcomRandom expected(state.watcom_seed);const uint32_t hi=expected.range(0,0x8fff),lo=expected.range(0,0x8fff);const auto key=uint8_t(expected.range(0,255)+'n'),byte=uint8_t(expected.range(0,255));
            const auto save=resumed.prepare_manual_save(request);check(resumed.revision()==101&&resumed.snapshot().watcom_seed==expected.seed()&&resumed.snapshot().game_seed==state.game_seed&&save.id2==lo+hi*0x10000u+1&&save.key==key&&save.random==byte,"manual save lost Original four-draw header/state");
            check(read_save(save)==resumed.snapshot(),"manual save retained pre-save RNG seed");
            const auto after=resumed.snapshot();rejects([&]{resumed.prepare_manual_save(request);},"save replay accepted");check(resumed.snapshot()==after,"replayed save changed state");
            auto incomplete=state;incomplete.desktop->initial_event.reset();rejects([&]{session_app::Session::resume(resources,incomplete,91,100);},"resume invented an absent event cursor");}

        if(offer){auto orphan=state;orphan.desktop->initial_event->offer.reset();rejects([&]{make_save(orphan);},"snapshot dropped a pending offer decision owner");const auto& listing=*state.world.transfer_market->rows[40];check(listing.player.valid()&&listing.seller==manager.club&&listing.human_seller&&listing.bids[0].bidder.valid(),"initial offer lost global actor IDs");}
        {auto loop=state;session_app::ManagerActionOwner turn{83,1};const auto accepted_loop=loop;
            if(offer||order.size()==1){rejects([&]{session_app::advance_initial_manager(loop,turn,turn,reference);},"manager transition skipped pending offer or final-day services");check(loop==accepted_loop&&turn.revision==1,"rejected manager transition leaked state");}
            else {session_app::advance_initial_manager(loop,turn,{83,1},reference);check(turn.revision==2&&loop.desktop->initial_event->manager_position==1&&loop.desktop->active_seat==order[1]&&loop.date==state.date&&loop.calendar==state.calendar&&loop.results==state.results,"next manager changed order/date or lost cursor");
                check(read_save(make_save(loop))==loop,"nonzero manager cursor did not survive save");const auto next=loop;rejects([&]{session_app::advance_initial_manager(loop,turn,{83,1},reference);},"stale manager advance replay accepted");check(loop==next,"stale manager transition changed state");}}
        if(offer) {
            TextDb texts;texts.strings.resize(TEXT_COUNT,"Synthetic message");
            const auto pending=state;
            session_app::ManagerActionOwner tx{84,7};
            PendingOfferHooks decline;
            decline.answer=[](const GameState&,const PendingOfferQuestion&){return PendingOfferAnswer::yes;};
            decline.select_transfer=[](const GameState&,const TransferOfferListView&){return PendingOfferTransferChoice{PendingOfferTransferChoiceKind::decline,-1};};
            rejects([&]{session_app::resolve_initial_offer(state,tx,{84,6},reference,texts,decline);},"stale pending decision accepted");
            check(state==pending&&tx.revision==7,"stale pending decision changed snapshot");
            const auto aborted=[](){throw std::runtime_error("cancelled by presenter");};
            auto cancel=decline;
            cancel.answer=[&](const GameState&,const PendingOfferQuestion&)->PendingOfferAnswer{aborted();return PendingOfferAnswer::unsupported;};
            cancel.select_transfer=[&](const GameState&,const TransferOfferListView&)->PendingOfferTransferChoice{aborted();return {};};
            rejects([&]{session_app::resolve_initial_offer(state,tx,tx,reference,texts,cancel);},"cancelled pending presenter committed");
            check(state==pending&&tx.revision==7,"cancelled pending decision leaked staged state/RNG");
            const auto result=session_app::resolve_initial_offer(state,tx,tx,reference,texts,decline);
            check(result.completed&&result.processed==1&&!result.transferred&&tx.revision==8&&!state.desktop->initial_event->offer&&!state.world.transfer_market->rows[40],"owned pending decline/salary decision did not finish");
            check(read_save(make_save(state))==state,"resolved pending state failed S9 roundtrip");
            const auto resolved=state;
            rejects([&]{session_app::resolve_initial_offer(state,tx,tx,reference,texts,decline);},"resolved pending decision replay accepted");
            check(state==resolved&&tx.revision==8,"replayed pending decision changed state");
            state=pending;
            auto interrupted=decline;
            interrupted.answer=[&](const GameState&,const PendingOfferQuestion&){++tx.revision;return PendingOfferAnswer::yes;};
            interrupted.select_transfer=[&](const GameState&,const TransferOfferListView&){++tx.revision;return PendingOfferTransferChoice{PendingOfferTransferChoiceKind::decline,-1};};
            rejects([&]{session_app::resolve_initial_offer(state,tx,tx,reference,texts,interrupted);},"pending decision overwrote externally changed owner");
            check(state==pending&&tx.revision==9,"pending rollback lost externally changed revision");
        }
        const auto accepted=state;rejects([&]{session_app::prepare_first_desk_event(state,owner,owner,reference);},"initial event replay accepted");check(state==accepted&&owner.revision==5,"replayed initial event leaked state");
    }
    auto interrupted=before;session_app::ManagerActionOwner owner{81,4};interrupted_desktop_owner=&owner;draw_hook=interrupt_desktop;
    rejects([&]{session_app::prepare_first_desk_event(interrupted,owner,{81,4},reference);},"initial event overwrote changed owner");draw_hook=nullptr;interrupted_desktop_owner=nullptr;check(interrupted==before&&owner.revision==5,"initial event rollback lost externally changed owner");
    auto occupied=before;occupied.calendar->schedule.others.push_back({0,CalendarDomain::shared,0x40});const auto busy=occupied;owner={81,4};
    rejects([&]{session_app::prepare_first_desk_event(occupied,owner,owner,reference);},"initial empty event skipped actual day-zero appointment");check(occupied==busy&&owner.revision==4,"busy day rejection changed state");
}
void file_io_tests() {
    const auto directory=std::filesystem::temp_directory_path()/("openbmh-world12-file-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));std::filesystem::create_directory(directory);
    struct Cleanup{std::filesystem::path path;~Cleanup(){std::error_code ignored;std::filesystem::remove_all(path,ignored);}} cleanup{directory};
    CareerSnapshot state;state.world=bmh::test::world12::complete_world();state.results=SeasonResults::empty(state.world.catalogue);const auto save=make_save(state);const auto path=directory/"Roundtrip.FBM";
    ui::SaveLoadRequest request;request.action=ui::SaveLoadAction::save;request.path=path;
    check(session_app::checked_game_path(directory,request)==std::filesystem::canonical(directory)/path.filename(),"valid confined game path rejected");session_app::publish_game_save(path,save,false);check(read_save(load_savegame(path))==state,"published save roundtrip differs");
    const auto bytes=read_file(path);rejects([&]{session_app::publish_game_save(path,save,false);},"unconfirmed target collision overwrote existing save");check(read_file(path)==bytes,"failed publish changed existing save");
    request.path=directory.parent_path()/"Outside.FBM";rejects([&]{session_app::checked_game_path(directory,request);},"outside save directory accepted");
    std::filesystem::create_directory(directory/"Blocked.FBM");request.path=directory/"Blocked.FBM";request.overwrite=true;rejects([&]{session_app::checked_game_path(directory,request);},"directory accepted as game file");rejects([&]{session_app::publish_game_save(request.path,save,true);},"failed replacement of directory accepted");
    std::error_code error;std::filesystem::create_symlink(path,directory/"Alias.FBM",error);if(!error){request.path=directory/"Alias.FBM";rejects([&]{session_app::checked_game_path(directory,request);},"symlink game target accepted");}
    error.clear();std::filesystem::create_directory_symlink(directory,directory/"FolderAlias",error);
    if(!error){request.path=directory/"FolderAlias"/path.filename();request.overwrite=true;
        check(session_app::checked_game_path(directory,request)==std::filesystem::canonical(directory)/path.filename(),"save-root directory alias did not resolve to canonical target");}
    for(const auto& entry:std::filesystem::directory_iterator(directory))check(entry.path().filename().string().find(".openbmh-save-")!=0,"failed save left a temporary behind");
}
void actual_management(ui::StartupFlow& flow,std::unique_ptr<session_app::Session>& session,const std::filesystem::path& data,const std::filesystem::path& output) {
    const auto game=session->snapshot().game_seed;const auto random=session->snapshot().watcom_seed;
    if(!output.empty())std::filesystem::create_directories(output);
    const auto send=[&](const ui::FlowResult& action){const auto result=session_app::serve_startup(flow,action,session,data,91,71);if(!result.error.empty())throw std::runtime_error("management input: "+result.error);};
    const auto capture=[&](const char* name){if(output.empty())return;const auto frame=flow.render();const auto palette=load_palette(ui::asset_path(data,"M3PC.COL"));std::ofstream f(output/(std::string(name)+".ppm"),std::ios::binary);f<<"P6\n320 240\n255\n";for(auto p:frame.pixels){const auto c=palette.argb[p];f.put(char(c>>16));f.put(char(c>>8));f.put(char(c));}};
    const auto open=[&](int kind){const auto& items=flow.desktop()->state().items;const auto item=std::find_if(items.begin(),items.end(),[&](const auto& i){return i.kind==kind;});check(item!=items.end(),"management desktop icon missing");send(flow.click(item->x+item->width/2,item->y+item->height/2));};
    check(session->snapshot().world.managers.front().tactics->backing->enabled_formations==std::array<uint8_t,2>{15,15},"new manager lacks four Original outdoor/indoor formation presets");
    open(3);check(flow.screen_name()=="lineup","lineup did not open");capture("lineup-before");
    const auto actor=*session->snapshot().desktop->active_seat;const auto club=session->snapshot().world.managers.at(actor).club;
    const auto roster=session->snapshot().world.clubs.at(club).roster;
    send(flow.click(20,41));const auto exchange=flow.click(20,48);send(exchange);
    check(session->snapshot().world.clubs.at(club).roster[1].player==roster[2].player&&session->snapshot().world.clubs.at(club).roster[2].player==roster[1].player,"GUI roster exchange lost global player identity");capture("lineup-after");
    const auto accepted=session->snapshot();const auto revision=session->revision();
    rejects([&]{session->apply_preparation(exchange.submenu_action);},"backend replayed an accepted lineup swap");check(session->snapshot()==accepted&&session->revision()==revision,"backend replay leaked changes");
    {auto view=session->preparation_view(data);auto wide=view.lineup;wide.players[0].name=std::string(80,'M');auto short_name=wide;short_name.players[0].name="X";
        const auto a=ui::LineupScreen(data,wide,session->rules()).render(),b=ui::LineupScreen(data,short_name,session->rules()).render();
        for(int y=29;y<38;++y)for(int x=63;x<320;++x)check(a.pixels[size_t(y)*320+x]==b.pixels[size_t(y)*320+x],"long player name overwrote another lineup column");}
    ui::FlowResult forged;forged.request=ui::FlowRequest::submenu_action;ui::LineupRequest fake;fake.kind=ui::LineupRequestKind::swap_players;fake.manager_slot=int(actor);fake.runtime_club=flow.desktop()->state().runtime_club;fake.revision=revision;forged.submenu_action=fake;
    check(!session_app::serve_startup(flow,forged,session,data,91,71).error.empty()&&session->snapshot()==accepted,"forged menu command mutated session");
    send(flow.click(100,41));check(flow.screen_name()=="player_card","player information did not open");capture("player-card");send(flow.click(0,0,true));
    send(flow.click(80,219));check(flow.in_pitch(),"pitch did not open through Source page request");capture("pitch");
    {
        const auto before_feedback=session->snapshot();const auto feedback_revision=session->revision();
        send(flow.click(20,41));flow.move(100,170);
        const auto cursor=flow.pitch_pointer_cursor(load_palette(ui::asset_path(data,"M3PC.COL")));
        check(cursor&&std::any_of(cursor->begin(),cursor->end(),[](uint32_t p){return p!=0;}),
              "typed pitch uses the visible native pointer bitmap and position feedback");
        check(session->snapshot()==before_feedback&&session->revision()==feedback_revision,
              "typed pitch pointer feedback changes no snapshot, revision or RNG state");
        send(flow.click(0,0,true));flow.move(300,220);
        check(!flow.pitch_pointer_cursor(load_palette(ui::asset_path(data,"M3PC.COL"))),
              "typed pitch cancellation restores the ordinary pointer");
    }

    send(flow.click(205,140));send(flow.click(245,214));
    auto view=session->preparation_view(data);const auto mask=view.pitch->formation_mask;int formation=0;while(formation<8&&(!(mask&(1<<formation))||formation+1==view.lineup.automatic_formation))++formation;
    if(formation==8){formation=0;while(formation<8&&!(mask&(1<<formation)))++formation;}check(formation<8,"no enabled formation in Original data");send(flow.click(formation%2?285:245,114+24*(formation/2)));capture("pitch-formation");
    send(flow.click(205,154));send(flow.click(20,41));view=session->preparation_view(data);const int order=view.lineup.players[1].board[2];send(flow.click(order>2?245:310,180));capture("pitch-orders");
    send(flow.click(205,180));send(flow.click(260,118));send(flow.click(20,41));capture("pitch-takers");
    check(session->snapshot().world.managers.at(actor).match_state->takers[0]==session->snapshot().world.clubs.at(club).roster[1].player,"GUI set-piece choice lost global player identity");
    send(flow.click(260,215));send(flow.click(205,140));
    check(read_save(make_save(session->snapshot()))==session->snapshot(),"pitch display/takers/roster save roundtrip differs");
    for(int n=0;n<3&&flow.screen_name()!="desktop";++n)send(flow.click(0,0,true));check(flow.screen_name()=="desktop","pitch did not close to desktop");
    open(18);check(flow.screen_name()=="tactics","tactics did not open");capture("tactics-before");
    const auto rules=session->rules();send(flow.click(rules.u8(layout::Tactics_Click_RuntimeField)+1,rules.u8(layout::Tactics_Click_RuntimeField3)+1));capture("tactics-after");send(flow.click(0,0,true));
    open(16);check(flow.screen_name()=="training","training did not initialize/open");capture("training-before");
    {auto wide=*session->preparation_view(data).training;wide.players[0].name=std::string(80,'M');auto short_name=wide;short_name.players[0].name="X";
        const auto a=ui::TrainingScreen(data,wide,session->rules()).render(),b=ui::TrainingScreen(data,short_name,session->rules()).render();
        for(int y=23;y<29;++y)for(int x=263;x<320;++x)check(a.pixels[size_t(y)*320+x]==b.pixels[size_t(y)*320+x],"long player name overwrote training controls");}
    send(flow.click(rules.u8(layout::Training_Click_RuntimeField2+3),rules.u8(layout::Training_Click_RuntimeField3+3)));capture("training-after");send(flow.click(0,0,true));
    check(flow.screen_name()=="desktop"&&session->snapshot().game_seed==game&&session->snapshot().watcom_seed==random&&session->snapshot().date==0,"preparation changed calendar or random streams");
    if(!output.empty())write_savegame(output/"management.FBM",make_save(session->snapshot()));
    std::cout<<"Actual lineup swap/player card/pitch/tactics/training GUI commands, stable IDs, no RNG/time advance and saved display state passed\n";
}
void actual_save_load(ui::StartupFlow& flow,std::unique_ptr<session_app::Session>& session,const std::filesystem::path& data,const std::filesystem::path& output) {
    const auto directory=flow.save_directory();const auto path=directory/"Resume.FBM";
    const auto send=[&](ui::StartupFlow& ui,const ui::FlowResult& action,std::unique_ptr<session_app::Session>& owner){const auto result=session_app::serve_startup(ui,action,owner,data,91,owner?owner->scope():71);if(!result.error.empty())throw std::runtime_error("file input: "+result.error);};
    const auto open=[&](int kind){const auto& items=flow.desktop()->state().items;const auto it=std::find_if(items.begin(),items.end(),[&](const auto& i){return i.kind==kind;});check(it!=items.end(),"file desktop icon missing");send(flow,flow.click(it->x+it->width/2,it->y+it->height/2),session);if(kind==10||kind==11)check(flow.screen_name()=="save_load","file dialog did not open");};
    const auto capture=[&](const char* name,const ui::StartupFlow& ui){if(output.empty())return;const auto frame=ui.render();const auto palette=load_palette(ui::asset_path(data,"M3PC.COL"));std::ofstream f(output/(std::string(name)+".ppm"),std::ios::binary);f<<"P6\n320 240\n255\n";for(auto p:frame.pixels){const auto c=palette.argb[p];f.put(char(c>>16));f.put(char(c>>8));f.put(char(c));}};
    auto original=session->snapshot();const auto old_revision=session->revision();open(11);check(flow.text("Resume"),"save filename input failed");capture("save-dialog",flow);
    const auto save_request=flow.send_key("ENTER");check(save_request.request==ui::FlowRequest::file_action&&flow.pending_file(),"save dialog did not emit file action");send(flow,save_request,session);
    check(flow.screen_name()=="desktop"&&session->revision()==old_revision+1,"successful save did not return to a newer desktop");const auto saved=session->snapshot();
    check(read_save(load_savegame(path))==saved,"GUI save differs from accepted Session");WatcomRandom expected(original.watcom_seed);for(int n=0;n<4;++n)expected.rand();check(saved.watcom_seed==expected.seed()&&saved.game_seed==original.game_seed,"GUI save did not own exactly four W draws");
    const auto first_bytes=read_file(path);open(11);flow.text("Resume");send(flow,flow.send_key("ENTER"),session);check(!flow.pending_file()&&read_file(path)==first_bytes,"overwrite happened without confirmation");send(flow,flow.send_key("ENTER"),session);
    check(read_save(load_savegame(path))==session->snapshot(),"confirmed overwrite did not save accepted Session");const auto checkpoint=session->snapshot();
    std::filesystem::create_directory(directory/"Blocked.FBM");open(11);flow.text("Blocked");send(flow,flow.send_key("ENTER"),session);const auto blocked=flow.send_key("ENTER");const auto before_failure=session->snapshot();const auto rev=session->revision();
    check(!session_app::serve_startup(flow,blocked,session,data,91,session->scope()).error.empty()&&session->snapshot()==before_failure&&session->revision()==rev&&!flow.pending_file(),"failed file operation changed live state or left pending request");capture("save-failure",flow);send(flow,flow.send_key("ESC"),session);
    open(18);const auto rules=session->rules();send(flow,flow.click(rules.u8(layout::Tactics_Click_RuntimeField)+1,rules.u8(layout::Tactics_Click_RuntimeField3)+1),session);send(flow,flow.click(0,0,true),session);check(session->snapshot()!=checkpoint,"load restoration test did not change current career");
    open(10);const auto info_backdrop=flow.render();check(flow.file_info_press(80,45),"typed game info did not open");const auto info=flow.render();for(int y=71;y<130;++y)for(int x=265;x<320;++x)check(info.pixels[size_t(y)*320+x]==info_backdrop.pixels[size_t(y)*320+x],"save info text escaped Original popup bounds");capture("load-info",flow);send(flow,flow.click(80,45),session);send(flow,flow.send_key("ENTER"),session);
    check(session->snapshot()==checkpoint&&flow.screen_name()=="desktop","desktop load regenerated or failed to restore saved state");capture("loaded-desktop",flow);
    auto corrupted=read_file(path);corrupted.back()^=1;{std::ofstream f(directory/"Bad.FBM",std::ios::binary);f.write(reinterpret_cast<const char*>(corrupted.data()),std::streamsize(corrupted.size()));}
    open(10);const auto bad=flow.send_key("ENTER");const auto intact=session->snapshot();const auto revision=session->revision();check(!session_app::serve_startup(flow,bad,session,data,91,session->scope()).error.empty()&&session->snapshot()==intact&&session->revision()==revision,"corrupt load replaced live state");capture("load-failure",flow);send(flow,flow.send_key("ESC"),session);
    ui::StartupFlow fresh(data,directory,ui::SetupLeagueSymbol::country_selection);std::unique_ptr<session_app::Session> loaded;fresh.click(60,215);send(fresh,fresh.click(240,215),loaded);check(fresh.screen_name()=="save_load","setup load did not open typed dialog");
    fresh.click(80,55);const auto request=fresh.send_key("ENTER");resume_draws=0;draw_hook=count_resume_draw;send(fresh,request,loaded);draw_hook=nullptr;
    check(!resume_draws&&loaded&&loaded->snapshot()==checkpoint&&fresh.desktop()->state().options==session->options(),"setup load regenerated data/RNG or kept setup options instead of saved options");
    const auto& items=fresh.desktop()->state().items;const auto lineup=std::find_if(items.begin(),items.end(),[](const auto& i){return i.kind==3;});send(fresh,fresh.click(lineup->x+lineup->width/2,lineup->y+lineup->height/2),loaded);check(fresh.in_pitch(),"loaded saved pitch page did not reopen");capture("loaded-pitch",fresh);
    if(!output.empty()){std::filesystem::copy_file(path,output/"manual-save.FBM",std::filesystem::copy_options::overwrite_existing);std::ofstream(output/"save-directory.txt")<<directory.string()<<'\n';}
    std::cout<<"Actual Original file dialogs: save/confirmed overwrite/info/desktop load/setup load/corrupt rejection/reopened pitch, four save draws and zero resume draws passed\n";
}
void actual_mailbox(const std::filesystem::path& data,const CareerSnapshot& base,const std::filesystem::path& output){
    const auto directory=std::filesystem::temp_directory_path()/("openbmh-mailbox-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));std::filesystem::create_directory(directory);
    struct Cleanup{std::filesystem::path path;~Cleanup(){std::error_code ignored;std::filesystem::remove_all(path,ignored);}} cleanup{directory};
    auto fixture=base;auto& economy=*fixture.world.economy;economy.messages.clear();economy.recipient_mask=0;
    const auto actor=*fixture.desktop->active_seat;const auto peer=fixture.world.managers[0].seat==actor?fixture.world.managers[1].seat:fixture.world.managers[0].seat;
    for(int row=0;row<4;++row){EconomyMessage message;message.recipients=row==1?0x80000000u|(1u<<actor)|(1u<<peer):1u<<(row==3?peer:actor);message.date=uint16_t(fixture.date);
        const std::string text="Synthetic mailbox message "+std::to_string(row);std::copy(text.begin(),text.end(),message.text.begin());append_economy_message(economy,message);}
    const auto file=directory/"Mailbox.FBM";write_savegame(file,make_save(fixture));
    ui::StartupFlow flow(data,directory,ui::SetupLeagueSymbol::country_selection);std::unique_ptr<session_app::Session> session;
    const auto send=[&](const ui::FlowResult& action){const auto result=session_app::serve_startup(flow,action,session,data,91,88);if(!result.error.empty())throw std::runtime_error("mailbox GUI: "+result.error);};
    flow.click(60,215);send(flow.click(240,215));send(flow.send_key("ENTER"));check(session&&flow.screen_name()=="desktop","mailbox fixture did not load through setup");
    const auto seed=session->snapshot().game_seed;const auto wr=session->snapshot().watcom_seed;
    if(!output.empty())std::filesystem::create_directories(output);
    const auto capture=[&](const char* name){if(output.empty())return;const auto frame=flow.render();const auto palette=load_palette(ui::asset_path(data,"M3PC.COL"));std::ofstream f(output/(std::string(name)+".ppm"),std::ios::binary);f<<"P6\n320 240\n255\n";for(auto pixel:frame.pixels){const auto color=palette.argb[pixel];f.put(char(color>>16));f.put(char(color>>8));f.put(char(color));}};
    flow.move(1,239);const auto opening=flow.click(182,220);check(opening.request==ui::FlowRequest::desk_action&&opening.desk_action.menu_kind==63,"Original newspaper shortcut did not request mailbox");send(opening);
    check(flow.screen_name()=="mailbox"&&session->mailbox_view().messages.size()==3,"mailbox leaked other manager or hid an owned message");capture("before");
    const auto rules=session->rules();
    const auto button=[&](int i){return flow.click(0x33+(rules.u8(layout::MailboxScreen_Click_RuntimeField+uint32_t(i))+rules.u8(layout::MailboxScreen_Click_RuntimeField3+uint32_t(i)))/2,0x16+(rules.u8(layout::MailboxScreen_Click_RuntimeField2+uint32_t(i))+rules.u8(layout::MailboxScreen_Click_RuntimeField4+uint32_t(i)))/2);};
    const auto article=[&]{return flow.click(0x33+(rules.u8(layout::MailboxScreen_Click_RuntimeField5)+rules.u8(layout::MailboxScreen_Click_RuntimeField7))/2,0x16+(rules.u8(layout::MailboxScreen_Click_RuntimeField6)+rules.u8(layout::MailboxScreen_Click_RuntimeField8))/2);};
    send(button(2));const auto keep=article();check(keep.request==ui::FlowRequest::mailbox_action,"Original keep control did not emit typed action");send(keep);
    check(session->snapshot().world.economy->messages[0].read_by&(1u<<actor),"mailbox keep was not saved for global actor");
    const auto kept=session->snapshot();const auto revision=session->revision();send(button(3));const auto refused=article();check(!session_app::serve_startup(flow,refused,session,data,91,88).error.empty()&&session->snapshot()==kept&&session->revision()==revision&&!flow.pending_mailbox(),"kept message deletion leaked state or kept a stale pending command");capture("kept-refusal");
    send(button(2));send(article());send(button(3));const auto first=article();send(first);
    check(session->snapshot().world.economy->messages[0].recipients==0&&session->mailbox_view().messages[0].entry_index==1,"mailbox deletion compacted physical slots or lost later mail");
    const auto untouched=session->snapshot();rejects([&]{session->edit_mailbox(first.mailbox_action);},"mailbox replay bypassed revision guard");check(session->snapshot()==untouched,"stale mailbox action changed state");
    send(article());check(session->snapshot().world.economy->messages[1].recipients==(0x80000000u|(1u<<peer)),"shared mail deletion removed peer recipient");capture("after");
    const auto before_close=session->revision();send(flow.click(0,0,true));
    check(flow.screen_name()=="desktop"&&!session->preparation_open()&&session->revision()==before_close&&!flow.desktop()->state().mailbox_notification_pending,"mailbox close failed to return to exact parent/clear actor latch");
    check(session->snapshot().world.economy->recipient_mask&(1u<<peer),"mailbox close acknowledged peer notifications");
    check(session->snapshot().game_seed==seed&&session->snapshot().watcom_seed==wr&&session->snapshot().date==fixture.date,"mailbox consumed RNG or advanced date");
    write_savegame(file,make_save(session->snapshot()));check(read_save(load_savegame(file))==session->snapshot(),"physical mailbox hole failed disk roundtrip");
    if(!output.empty())std::filesystem::copy_file(file,output/"after.FBM",std::filesystem::copy_options::overwrite_existing);
    std::cout<<"Actual Original mailbox shortcut, shared/private recipients, keep/refusal/delete, physical holes, actor-only close, RNG-neutral snapshots and stale rollback passed\n";
}
void actual_scheduled_desks(session_app::Session& session,const session_app::RuntimeResources& resources,
    const std::filesystem::path& data,const std::filesystem::path& output,const std::string& label) {
    const auto initial=session.snapshot();session.begin_scheduled_desks(session.revision());
    std::set<uint8_t> seats;
    for(size_t i=0;i<initial.world.managers.size();++i) {
        const auto current=session.snapshot();const auto saved=make_save(current);
        check(saved.stream[7]=='F'&&current.date==initial.date&&current.calendar==initial.calendar&&
              current.prepared_event->groups==initial.prepared_event->groups,"actual scheduled desk lost weather/date");
        seats.insert(*current.desktop->active_seat);
        session=session_app::Session::resume(resources,read_save(saved),96,session.revision());
        const auto before=session.snapshot();const auto revision=session.revision();
        rejects([&]{session.book_scheduled_attendance(revision,data);},"actual active scheduled desk was skipped by gate booking");
        check(session.snapshot()==before&&session.revision()==revision,"rejected active-desk gate booking changed state");
        auto desk=ui::world12_initial_desktop(data,session.snapshot(),session.reference(),session.rules(),session.options(),revision);
        const auto& manager=*std::find_if(current.world.managers.begin(),current.world.managers.end(),[&](const auto& m){return m.seat==*current.desktop->active_seat;});
        ClubId opponent;bool away=false;
        for(const auto& group:current.prepared_event->groups)for(const auto& match:group.matches)
            if(match.fixture.home==manager.club||match.fixture.away==manager.club){away=match.fixture.away==manager.club;opponent=away?match.fixture.home:match.fixture.away;}
        if(opponent.valid()) {
            const auto& crest=current.world.crests.at(current.world.clubs.at(opponent).crest);
            check(desk.next_opponent_name==current.world.clubs.at(opponent).name&&desk.next_opponent_badge==crest.cell&&
                  desk.next_opponent_badge_atlas==crest.path&&!desk.next_opponent_badge_fallback&&desk.next_match_away==away,
                  "scheduled desktop lost its actual opponent, atlas or home/away side");
        }else check(desk.next_opponent==std::optional<int>{-1},"nonparticipating manager received an invented opponent");
        if(!output.empty()) {
            write_savegame(output/(label+"-manager-"+std::to_string(i)+".FBM"),saved);
            ui::DesktopScreen screen(data,desk,session.rules());const auto frame=screen.render();
            const auto palette=load_palette(ui::asset_path(data,"M3PC.COL"));
            std::ofstream image(output/(label+"-manager-"+std::to_string(i)+".ppm"),std::ios::binary);
            image<<"P6\n320 240\n255\n";for(auto pixel:frame.pixels){const auto color=palette.argb[pixel];image.put(char(color>>16));image.put(char(color>>8));image.put(char(color));}
        }
        check(!session.snapshot().desktop->autosave_pending,"actual scheduled QA requires its pending autosave file owner");
        ui::DeskRequest next;next.kind=ui::DeskRequestKind::advance_turn;next.manager_slot=desk.manager_slot;next.revision=revision;
        if(i+1<initial.world.managers.size())session.advance_manager(next);else session.finish_scheduled_desks(next);
    }
    check(seats.size()==initial.world.managers.size()&&!session.snapshot().desktop->active_seat&&
          session.snapshot().prepared_event->desk_phase==PreparedEvent::DeskPhase::complete&&
          read_save(make_save(session.snapshot()))==session.snapshot(),"actual scheduled manager sequence did not close into its match owner");
    std::cout<<"Actual scheduled desks: "<<label<<", four German/English managers, SF save/reload at each cursor, no skipped seats or early gates passed\n";
}
void actual_four_managers(const std::filesystem::path& data,const std::filesystem::path& output) {
    const auto resources=session_app::load_runtime_resources(data);ui::StartupFlow flow(data,{},ui::SetupLeagueSymbol::country_selection);std::unique_ptr<session_app::Session> session;
    const auto arrow=[&](int index){flow.click(resources.german.u16(layout::Setup_FormatError_RuntimeField3+uint32_t(index*2))+10,resources.german.u8(layout::Setup_FormatError_RuntimeField4+uint32_t(index))+10);};
    for(int n=0;n<3;++n)arrow(6);for(int n=0;n<2;++n)arrow(3);check(flow.options().managers==4&&flow.options().mode==1,"four-manager mode-I setup failed");
    const auto send=[&](const ui::FlowResult& action){const auto result=session_app::serve_startup(flow,action,session,data,91,74);if(!result.error.empty())throw std::runtime_error("four managers: "+result.error);};
    send(flow.click(10,10,true));for(int i=0;i<4;++i){if(i%2)flow.click(200,95);flow.click(10,10,true);send(flow.click(100,171));}
    std::set<uint8_t> seen;
    for(int position=0;position<4;++position){const auto& snapshot=session->snapshot();check(snapshot.desktop->initial_event->manager_position==position&&snapshot.desktop->active_seat==snapshot.desktop->initial_event->manager_order[size_t(position)]&&snapshot.date==0,"four-manager UI lost current position/date");seen.insert(*snapshot.desktop->active_seat);
        if(!output.empty()){const auto file=output/("manager-"+std::to_string(position)+".FBM");write_savegame(file,make_save(snapshot));check(read_save(load_savegame(file))==snapshot,"four-manager cursor file differs");const auto image=flow.render();const auto palette=load_palette(ui::asset_path(data,"M3PC.COL"));std::ofstream f(output/("manager-"+std::to_string(position)+".ppm"),std::ios::binary);f<<"P6\n320 240\n255\n";for(auto p:image.pixels){const auto c=palette.argb[p];f.put(char(c>>16));f.put(char(c>>8));f.put(char(c));}}
        const auto before=snapshot;const auto revision=session->revision();const auto& items=flow.desktop()->state().items;const auto icon=std::find_if(items.begin(),items.end(),[](const auto& i){return i.kind==12;});const auto action=flow.click(icon->x+icon->width/2,icon->y+icon->height/2);
        if(position<3){send(action);check(session->revision()==revision+1,"UI next manager did not advance revision");}
        else {check(!session_app::serve_startup(flow,action,session,data,91,74).error.empty()&&session->snapshot()==before&&session->revision()==revision&&!flow.desktop()->pending(),"last desk silently skipped incomplete daily services");}
    }
    check(seen.size()==4,"four-manager UI repeated or skipped an owner");std::cout<<"Four actual alternating German/English managers: Original kickoff cycles saved shuffled cursor, every owner once; final daily-service boundary remains explicit\n";
    auto clocked=*session;ui::DeskRequest close;close.kind=ui::DeskRequestKind::advance_turn;close.manager_slot=flow.desktop()->state().manager_slot;close.revision=clocked.revision();clocked.finish_initial_day(close);
    check(clocked.snapshot().date==1&&clocked.snapshot().progress->phase==ProgressPhase::daily_services,"actual four-manager event did not close into saved daily services");
    auto resumed=session_app::Session::resume(resources,read_save(make_save(clocked.snapshot())),92,1);
    session_app::ClockHooks hooks;hooks.services.incidents.answer=[](const GameState&,uint8_t,const WeeklyOfferQuestion&)->std::optional<bool>{return false;};hooks.services.auctions.allocator=TransferAllocatorContext::zero;hooks.services.auctions.select=[](const GameState&,const TransferOfferListView&,const AiMatchProjection&)->std::optional<int>{return -1;};hooks.services.auctions.notice=[](const GameState&,const TransferDecisionEvent&,const AiMatchProjection&){};
    hooks.relations=[](const CareerSnapshot& current){WeeklyIncidentContext context{current.date,current.season,{}};const auto& fields=*current.world.initial_competitions;
        for(const auto& manager:current.world.managers){WeeklyClubRelations relation;relation.club=manager.club;const auto country=current.world.manager_country(manager.seat);
            for(const auto& [c,id]:fields.champions)if(c==country)relation.advertising.champion=id==manager.club;
            for(const auto& [c,id]:fields.domestic_holders)if(c==country)relation.advertising.holders[0]=id==manager.club;
            for(size_t cup=0;cup<3;++cup){relation.advertising.holders[cup+1]=fields.holders[cup]==manager.club;if(std::find(fields.fields[cup].begin(),fields.fields[cup].end(),manager.club)!=fields.fields[cup].end())relation.advertising.european_cup=uint8_t(cup+1);}
            context.clubs.push_back(relation);}return context;};
    uint16_t expected_day=1;const auto& calendar=resumed.snapshot().calendar->schedule;while(expected_day<calendar.days&&calendar.leagues_on(expected_day).empty()&&calendar.others_on(expected_day).empty())++expected_day;
    size_t phases=0;while(resumed.snapshot().progress->phase!=ProgressPhase::scheduled_event){const auto pending=resumed.snapshot();const auto owner=resumed.revision();resumed.advance_saved_clock(owner,data,hooks);auto snapshot=read_save(make_save(resumed.snapshot()));resumed=session_app::Session::resume(resources,std::move(snapshot),92,resumed.revision());if(++phases>30)throw std::runtime_error("actual resumed clock did not reach its first appointment");}
    check(resumed.snapshot().date==expected_day&&resumed.snapshot().calendar->next_day==expected_day,"actual event seeker skipped a country/national appointment");
    if(!output.empty())write_savegame(output/"first-scheduled-clock.FBM",make_save(resumed.snapshot()));
    std::cout<<"Actual four-manager saved clock: "<<phases<<" resumed phases to day "<<expected_day<<", weekly/training/finance complete; scheduled event retained for its owner\n";
    const auto appointment=prepare_day(resumed.snapshot());
    if(appointment&&appointment->leagues.empty()&&appointment->others.size()==1&&appointment->others[0].source_entry==0x4006) {
        const auto before=resumed.snapshot();
        const auto nomination=resumed.prepare_national_day(resumed.revision());
        check(nomination.nominations.size()==2&&!nomination.coach_desk_required&&resumed.snapshot().date==before.date&&
              resumed.snapshot().calendar==before.calendar,"actual national nomination skipped its calendar boundary");
        auto saved=read_save(make_save(resumed.snapshot()));
        resumed=session_app::Session::resume(resources,std::move(saved),92,resumed.revision());
        const auto waiting=resumed.snapshot();const auto revision=resumed.revision();
        rejects([&]{resumed.prepare_national_day(revision);},"actual saved nomination was replayed");
        check(resumed.snapshot()==waiting&&resumed.revision()==revision,"actual duplicate nomination changed streams");
        const auto finished=resumed.finish_uncoached_national_day(resumed.revision());
        check(resumed.snapshot().date==before.date+1&&resumed.snapshot().progress->phase==ProgressPhase::daily_services&&
              finished.watcom_draws==1&&finished.game_draws==4,"actual uncoached empty national event has wrong date/draws");
        saved=read_save(make_save(resumed.snapshot()));resumed=session_app::Session::resume(resources,std::move(saved),92,resumed.revision());
        if(!output.empty())write_savegame(output/"national-nomination-complete.FBM",make_save(resumed.snapshot()));
        const auto next=resumed.seek_saved_event(resumed.revision(),data,hooks);
        check(next.phase==ProgressPhase::scheduled_event&&resumed.snapshot().date>before.date+1,
              "actual completed nomination did not resume daily services to its next appointment");
        const auto upcoming=prepare_day(resumed.snapshot());
        check(upcoming&&upcoming->cups.size()==2,"actual aligned Super-Cup/Charity Shield fields are incomplete");
        for(auto cup:{DomesticCup::german_supercup,DomesticCup::english_supercup}) {
            const auto field=std::find_if(upcoming->cups.begin(),upcoming->cups.end(),[&](const auto& stage){return stage.cup==cup;});
            check(field!=upcoming->cups.end()&&field->matches.size()==1,"actual country Super-Cup has no initialized fixture");
        }
        if(!output.empty())write_savegame(output/"after-national-next-event.FBM",make_save(resumed.snapshot()));
        {
            auto prepared_session=resumed;const auto before_weather=prepared_session.snapshot();
            const auto weather=prepared_session.prepare_scheduled_weather(prepared_session.revision());
            check(weather.matches.size()==2&&prepared_session.snapshot().prepared_event&&
                  prepared_session.snapshot().date==before_weather.date,"actual pre-match weather was not stored without advancing the date");
            auto prepared_save=make_save(prepared_session.snapshot());
            auto weather_loaded=session_app::Session::resume(resources,read_save(prepared_save),93,prepared_session.revision());
            check(weather_loaded.snapshot()==prepared_session.snapshot(),"actual stored conditions changed after resuming SD");
            rejects([&]{weather_loaded.prepare_scheduled_weather(weather_loaded.revision());},"actual loaded weather was generated again");
            if(!output.empty())write_savegame(output/"prepared-finals-weather.FBM",prepared_save);
            actual_scheduled_desks(weather_loaded,resources,data,output,"finals");
            const auto gate=weather_loaded.book_scheduled_attendance(weather_loaded.revision(),data);
            check(gate.spectators.size()==2&&weather_loaded.snapshot().prepared_event->gates_booked,
                  "actual spectator/gate calculation did not publish its receipt");
            const auto booked=weather_loaded.snapshot();const auto booked_revision=weather_loaded.revision();
            auto gate_save=make_save(booked);check(gate_save.stream[7]=='F',"actual completed desk/gate receipt missing SF owner");
            auto gate_loaded=session_app::Session::resume(resources,read_save(gate_save),94,booked_revision);
            rejects([&]{gate_loaded.book_scheduled_attendance(gate_loaded.revision(),data);},"loaded gate receipt was charged twice");
            check(gate_loaded.snapshot()==booked,"duplicate actual gate request changed balances/streams");
            if(!output.empty())write_savegame(output/"booked-finals-gates.FBM",gate_save);
            HumanRoundHooks receipt_hooks;receipt_hooks.decline_unpresented_events=true;
            gate_loaded.play_prepared_matches(gate_loaded.revision(),receipt_hooks);
            check(gate_loaded.snapshot().date==before_weather.date+1&&!gate_loaded.snapshot().prepared_event&&
                  gate_loaded.snapshot().progress->phase==ProgressPhase::daily_services,
                  "actual saved weather/gates were not consumed by the scoring owner");
            check(read_save(make_save(gate_loaded.snapshot()))==gate_loaded.snapshot(),"actual source attendance/scoring failed save roundtrip");
            if(!output.empty())write_savegame(output/"source-prepared-finals-computed.FBM",make_save(gate_loaded.snapshot()));
            std::cout<<"Actual source-prepared finals: saved weather/referees, Core spectators/gate/catering, SE duplicate guard and score consumption passed\n";
            std::cout<<"Actual stored Super-Cup/Charity Shield weather: source catalogue, countries, both streams, SD save/resume and no redraw passed\n";
        }
        std::vector<session_app::CupDayConditions> cup_inputs;
        for(const auto& stage:upcoming->cups) {
            AiLeagueConditions match;match.date=resumed.snapshot().date;match.month=month_code(match.date);
            match.temperature=20;match.rain=0;match.spectators=20000;match.referee_properties={24};match.presentation_buttons.assign(0xb4,1);
            cup_inputs.push_back({stage.cup,stage.source_entry,stage.revision,std::vector<AiLeagueConditions>(stage.matches.size(),match)});
        }
        HumanRoundHooks match_hooks;match_hooks.decline_unpresented_events=true;
        const auto cup_date=resumed.snapshot().date;const auto played=resumed.play_scheduled_domestic_cups(resumed.revision(),cup_inputs,match_hooks);
        check(played.cups.size()==2&&resumed.snapshot().date==cup_date+1&&resumed.snapshot().progress->phase==ProgressPhase::daily_services,
              "actual country cup calculations did not retain pending daily continuation");
        for(auto cup:{DomesticCup::german_supercup,DomesticCup::english_supercup}) {
            const auto stage=std::find_if(resumed.snapshot().cups.begin(),resumed.snapshot().cups.end(),[&](const auto& s){return s.cup==cup;});
            check(stage!=resumed.snapshot().cups.end()&&stage->complete()&&stage->qualifiers().size()==1,"actual computed Super-Cup/Charity Shield unresolved");
        }
        saved=read_save(make_save(resumed.snapshot()));resumed=session_app::Session::resume(resources,std::move(saved),92,resumed.revision());
        if(!output.empty())write_savegame(output/"computed-supercups.FBM",make_save(resumed.snapshot()));
        std::cout<<"Actual data country finals: real scoring, extra-time/shootout resolution, player/manager effects, both country stages and saved calendar closure passed (fixed QA weather/crowd; gate/premiums/live owner not exercised)\n";
        resumed.seek_saved_event(resumed.revision(),data,hooks);
        const auto league_ticket=prepare_day(resumed.snapshot());
        check(league_ticket&&!league_ticket->leagues.empty()&&league_ticket->others.empty(),"actual next league calculation has unresolved non-league appointments");
        std::vector<session_app::AiDayLeagueConditions> league_inputs;
        for(const auto& round:league_ticket->leagues) {
            AiLeagueConditions match;match.date=resumed.snapshot().date;match.month=month_code(match.date);
            match.temperature=20;match.rain=0;match.spectators=20000;match.referee_properties={24};match.presentation_buttons.assign(0xb4,1);
            league_inputs.push_back({round.appointment,std::vector<AiLeagueConditions>(round.matches.size(),match)});
        }
        {
            auto source_ready=resumed;source_ready.prepare_scheduled_weather(source_ready.revision());
            actual_scheduled_desks(source_ready,resources,data,output,"leagues");
            const auto crowd=source_ready.book_scheduled_attendance(source_ready.revision(),data);
            check(!crowd.spectators.empty()&&source_ready.snapshot().prepared_event->gates_booked,
                  "actual league attendance was not calculated from source inputs");
            const auto source_gate=make_save(source_ready.snapshot());
            auto source_loaded=session_app::Session::resume(resources,read_save(source_gate),95,source_ready.revision());
            source_loaded.play_prepared_matches(source_loaded.revision(),match_hooks);
            check(source_loaded.snapshot().date==resumed.snapshot().date+1&&!source_loaded.snapshot().prepared_event,
                  "actual source-prepared league receipt did not publish its matches");
            check(read_save(make_save(source_loaded.snapshot()))==source_loaded.snapshot(),"actual booked league histories failed save roundtrip");
            if(!output.empty())write_savegame(output/"source-prepared-leagues-computed.FBM",make_save(source_loaded.snapshot()));
            std::cout<<"Actual source-prepared leagues: original weather/referee catalogue, calculated crowds and gates, persistent attendance records and real scoring passed\n";
        }
        const auto league_date=resumed.snapshot().date;
        const auto league_result=resumed.play_scheduled_leagues(resumed.revision(),league_inputs,match_hooks);
        check(league_result.leagues.size()==league_ticket->leagues.size()&&resumed.snapshot().date==league_date+1&&
              resumed.snapshot().progress->phase==ProgressPhase::daily_services,"actual league calculations lost their saved service boundary");
        saved=read_save(make_save(resumed.snapshot()));resumed=session_app::Session::resume(resources,std::move(saved),92,resumed.revision());
        if(!output.empty())write_savegame(output/"computed-first-leagues.FBM",make_save(resumed.snapshot()));
        std::cout<<"Actual first league calculations: "<<league_result.leagues.size()<<" complete country/European rounds at day "<<league_date<<", real scores, human/AI effects and saved continuation passed (fixed QA weather/crowd)\n";
        std::cout<<"Actual four-manager national nomination: two global squads, saved replay guard, Source empty setup/option cycle/order shuffle, resumed daily processing to day "<<cup_date<<" passed\n";
    }


}
void actual_pending(const std::filesystem::path& data,const std::filesystem::path& output) {
    const auto resources=session_app::load_runtime_resources(data);
    const auto palette=load_palette(ui::asset_path(data,"M3PC.COL"));
    for(auto country:{Country::germany,Country::england}) {
        ui::SetupOptions options;options.country_selection=true;
        session_app::Session prepared(resources,options,91,75);
        const auto context=prepared.selection_context();
        const auto& group=country==Country::germany?context.world->germany[0]:context.world->england[0];
        ui::ManagerChoice choice;choice.master_club=-1;choice.world_club=group.clubs[0].id;
        choice.country=country;choice.name="Offer manager";choice.roster_revision=context.revision;
        prepared.confirm_manager(choice,context.revision);prepared.prepare_start(prepared.revision());
        const auto base=prepared.snapshot();
        for(int kind=0;kind<4;++kind) {
            CareerSnapshot fixture;bool found=false;
            for(uint32_t seed=1;seed<30000;++seed) {
                WatcomRandom probe(seed);probe.range(0,10000);
                if(probe.range(0,70-int16_t(base.world.managers[0].match_state->staff_ratings[0])/50))continue;
                auto candidate=base;candidate.watcom_seed=seed;session_app::ManagerActionOwner owner{75,0};
                session_app::prepare_first_desk_event(candidate,owner,owner,resources.reference);
                const auto& event=*candidate.desktop->initial_event;if(!event.offer)continue;
                const auto& row=*candidate.world.transfer_market->rows[event.offer->row];
                const bool matches=kind==3?event.offer->warned:!event.offer->salary_only&&bool(row.bids[0].months&128)==(kind==2);
                if(matches){fixture=std::move(candidate);found=true;break;}
            }
            check(found,"could not generate actual-data pending GUI fixture");
            const auto label=std::string(country==Country::germany?"de":"en")+"-"+std::to_string(kind);
            const auto directory=std::filesystem::temp_directory_path()/("openbmh-pending-"+label+"-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
            std::filesystem::create_directory(directory);
            struct Cleanup{std::filesystem::path path;~Cleanup(){std::error_code ignored;std::filesystem::remove_all(path,ignored);}} cleanup{directory};
            write_savegame(directory/"Pending.FBM",make_save(fixture));
            ui::StartupFlow flow(data,directory,ui::SetupLeagueSymbol::country_selection);
            std::unique_ptr<session_app::Session> session;
            const auto send=[&](const ui::FlowResult& action,const PendingOfferHooks& hooks=PendingOfferHooks{}){
                const auto result=session_app::serve_startup(flow,action,session,data,777,75,hooks);
                if(!result.error.empty())throw std::runtime_error("pending GUI "+label+": "+result.error);
            };
            send(flow.click(240,215));flow.click(80,55);send(flow.send_key("ENTER"));
            check(session&&session->snapshot()==fixture&&flow.screen_name()=="desktop","pending GUI load lost offer owner");
            const auto capture=[&](std::string_view name,const Image& image){
                if(output.empty())return;std::filesystem::create_directories(output);
                std::ofstream file(output/(label+"-"+std::string(name)+".ppm"),std::ios::binary);file<<"P6\n320 240\n255\n";
                for(auto p:image.pixels){const auto c=palette.argb[p];file.put(char(c>>16));file.put(char(c>>8));file.put(char(c));}
            };
            const auto kickoff=[&]{const auto& items=flow.desktop()->state().items;
                const auto icon=std::find_if(items.begin(),items.end(),[](const auto& i){return i.kind==12;});
                return flow.click(icon->x+icon->width/2,icon->y+icon->height/2);};
            capture("before",flow.render());
            const auto rejected=session_app::serve_startup(flow,kickoff(),session,data,777,75);
            check(!rejected.error.empty()&&session->snapshot()==fixture&&!flow.desktop()->pending(),"missing pending presenter changed session or left a stuck view");
            const auto revision=session->revision();const auto& crest=fixture.world.crests.at(fixture.world.clubs.at(fixture.world.managers[0].club).crest);
            int questions=0,choices=0,notices=0;PendingOfferHooks hooks;
            const auto bind=[&](ui::TaxDialogProjection& view){view.badge_sprite=crest.cell;view.badge_atlas=crest.path;view.badge_fallback=false;};
            hooks.answer=[&](const GameState& state,const PendingOfferQuestion& q){
                auto view=ui::pending_offer_question_dialog(state,q);bind(view);
                ui::GameEndTaxDialog dialog(data,std::move(view),{},flow.render(),resources.german);
                capture(q.kind==PendingOfferQuestionKind::salary_demand?"salary":"severance",dialog.render());++questions;return PendingOfferAnswer::yes;
            };
            hooks.select_transfer=[&](const GameState& state,const TransferOfferListView& offers){
                auto view=ui::pending_offer_transfer_decision_from_game(state,offers);
                view.badge_sprite=view.seller_badge_sprite=crest.cell;view.badge_atlas=view.seller_badge_atlas=crest.path;view.notice_badge_fallback=false;
                ui::IncomingTransferScreen dialog(data,std::move(view),true,resources.german);
                capture("offer",dialog.render());if(dialog.notice_active())dialog.dismiss_notice();
                const auto selected=dialog.click(100,150,kind==0);check(selected.has_value(),"Original offer row did not select/decline");++choices;
                return PendingOfferTransferChoice{*selected<0?PendingOfferTransferChoiceKind::decline:PendingOfferTransferChoiceKind::bid,*selected};
            };
            hooks.notice=[&](const GameState& state,const PendingOfferNotice& notice){
                auto view=ui::pending_offer_notice_dialog(state,notice);bind(view);
                capture("notice",ui::GameEndTaxDialog(data,std::move(view),{},flow.render(),resources.german).render());++notices;
            };
            send(kickoff(),hooks);
            const auto& after=session->snapshot();
            check(session->revision()==revision+1&&after.date==0&&!after.desktop->initial_event->offer&&flow.screen_name()=="desktop","pending GUI lost completed decision or advanced the day");
            check(kind==3?questions>0:choices==1,"pending GUI failed to present required decision");
            const auto player=fixture.world.transfer_market->rows[40]->player;
            const auto buyer=fixture.world.transfer_market->rows[40]->bids[0].bidder;
            const auto& roster=after.world.clubs.at(buyer).roster;
            const bool moved=std::any_of(roster.begin(),roster.end(),[&](const auto& slot){return slot.player==player;});
            if(kind==0||kind==3)check(!moved,"decline/salary GUI moved player");
            if(kind==1||kind==2)check(moved,"accepted actual GUI offer did not move player");
            if(kind==1||kind==2)check(after.world.economy&&fixture.world.economy&&after.world.economy->messages.size()>fixture.world.economy->messages.size(),"accepted GUI transfer did not persist its Source news");
            if(kind==2)check(after.world.loans.size()==1&&after.transfers&&after.transfers->loans.size()==1,"actual GUI loan lost owner contract");
            check(read_save(make_save(after))==after,"actual GUI pending settlement failed save roundtrip");
            capture("after",flow.render());
            if(!output.empty()){write_savegame(output/(label+"-before.FBM"),make_save(fixture));write_savegame(output/(label+"-after.FBM"),make_save(after));}
            std::cout<<"Actual pending GUI "<<label<<": questions="<<questions<<" choices="<<choices<<" notices="<<notices<<" moved="<<moved<<'\n';
        }
    }
}
void actual_pending_multiplayer(const std::filesystem::path& data,const std::filesystem::path& output) {
    const auto resources=session_app::load_runtime_resources(data);
    ui::SetupOptions options;options.country_selection=true;options.managers=4;
    session_app::Session prepared(resources,options,91,76);
    std::vector<ui::ManagerChoice> confirmed;
    for(int seat=0;seat<4;++seat) {
        ui::ManagerScreen chooser(data,seat,confirmed,prepared.selection_context());
        if(seat%2)chooser.click(200,95);
        auto choice=chooser.choice();choice.slot=seat;choice.portrait=seat;choice.name="Pending "+std::to_string(seat);
        // The session validates current ownership, independently of the chooser.
        const auto country=seat%2?Country::england:Country::germany;
        const auto league=prepared.snapshot().world.catalogue.starting_league(country,0);
        const auto& clubs=prepared.snapshot().world.membership[league.value].clubs;
        choice.world_club=*std::find_if(clubs.begin(),clubs.end(),[&](auto id){return std::none_of(prepared.snapshot().world.managers.begin(),prepared.snapshot().world.managers.end(),[&](const auto& m){return m.club==id;});});
        prepared.confirm_manager(choice,choice.roster_revision);confirmed.push_back(choice);
    }
    prepared.prepare_start(prepared.revision());const auto base=prepared.snapshot();
    CareerSnapshot fixture;bool found=false;
    for(uint32_t seed=1;seed<30000;++seed) {
        WatcomRandom probe(seed);probe.range(0,10000);
        if(probe.range(0,70))continue;
        auto candidate=base;candidate.watcom_seed=seed;session_app::ManagerActionOwner owner{76,0};
        session_app::prepare_first_desk_event(candidate,owner,owner,resources.reference);
        const auto& event=*candidate.desktop->initial_event;
        if(event.offer&&!event.offer->salary_only&&(candidate.world.transfer_market->rows[event.offer->row]->bids[0].months&128)){fixture=std::move(candidate);found=true;break;}
    }
    check(found,"four-manager pending fixture found no initial loan");
    const auto directory=std::filesystem::temp_directory_path()/("openbmh-pending-multi-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(directory);
    struct Cleanup{std::filesystem::path path;~Cleanup(){std::error_code ignored;std::filesystem::remove_all(path,ignored);}} cleanup{directory};
    write_savegame(directory/"Pending.FBM",make_save(fixture));
    ui::StartupFlow flow(data,directory,ui::SetupLeagueSymbol::country_selection);std::unique_ptr<session_app::Session> session;
    PendingOfferHooks hooks;hooks.answer=[](const GameState&,const PendingOfferQuestion&){return PendingOfferAnswer::yes;};
    hooks.select_transfer=[](const GameState&,const TransferOfferListView&){return PendingOfferTransferChoice{PendingOfferTransferChoiceKind::bid,0};};
    const auto send=[&](const ui::FlowResult& action){const auto result=session_app::serve_startup(flow,action,session,data,777,76,hooks);if(!result.error.empty())throw std::runtime_error("multi pending: "+result.error);};
    send(flow.click(240,215));flow.click(80,55);send(flow.send_key("ENTER"));
    std::set<uint8_t> seen;
    for(size_t position=0;position<4;++position) {
        const auto& current=session->snapshot();const auto seat=*current.desktop->active_seat;seen.insert(seat);
        check(current.desktop->initial_event->manager_position==position,"pending decision lost saved manager position");
        if(!output.empty()){std::filesystem::create_directories(output);write_savegame(output/("pending-manager-"+std::to_string(position)+".FBM"),make_save(current));}
        const auto& items=flow.desktop()->state().items;const auto icon=std::find_if(items.begin(),items.end(),[](const auto& i){return i.kind==12;});
        const auto revision=session->revision();const bool offer=current.desktop->initial_event->offer.has_value();const auto before=current;
        const auto action=flow.click(icon->x+icon->width/2,icon->y+icon->height/2);
        if(position<3||offer) {
            send(action);check(session->revision()==revision+(position<3&&offer?2:1),"multi pending revision skipped/repeated transaction");
            check(!session->snapshot().world.loans.empty()&&session->snapshot().transfers&&session->snapshot().transfers->loans.size()==session->snapshot().world.loans.size(),"next manager discarded previous outgoing loan contract");
            check(read_save(make_save(session->snapshot()))==session->snapshot(),"multi-manager pending snapshot differs");
        }else {
            check(!session_app::serve_startup(flow,action,session,data,777,76,hooks).error.empty()&&session->snapshot()==before,"last pending manager skipped daily services");
        }
    }
    check(seen.size()==4,"pending loan handoff skipped/repeated a manager");
    std::cout<<"Four actual German/English Mode-III managers: accepted outgoing loan, atomic offer/handoff revisions and retained owner contract across every desk passed\n";
}
void actual(const std::filesystem::path& data,const std::filesystem::path& output) {
    auto resources=session_app::load_runtime_resources(data);
    check(resources.reference.manager_teams.empty(),"reference retained invented manager ownership");
    for(auto slot:resources.reference.squad_slots)check((slot&0xfff)==0xfff,"data-only reference imported actual packed players");
    const auto save_directory=(output.empty()?std::filesystem::temp_directory_path():output)/("save-dialog-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));std::filesystem::create_directories(save_directory);
    ui::StartupFlow flow(data,save_directory,ui::SetupLeagueSymbol::country_selection);std::unique_ptr<session_app::Session> session;
    const auto begin=flow.click(10,10,true);auto result=session_app::serve_startup(flow,begin,session,data,91,71);
    check(result.error.empty()&&result.progress==session_app::StartupProgress::selecting&&session&&flow.screen_name()=="manager_selection","actual setup did not create typed manager session");
    flow.click(200,95);flow.click(252,95);flow.click(239,154);flow.click(10,10,true);
    if(!output.empty()){std::filesystem::create_directories(output);const auto frame=flow.render();const auto palette=load_palette(ui::asset_path(data,"M3PC.COL"));std::ofstream f(output/"english-selection.ppm",std::ios::binary);f<<"P6\n320 240\n255\n";for(auto p:frame.pixels){const auto c=palette.argb[p];f.put(char(c>>16));f.put(char(c>>8));f.put(char(c));}}
    flow.click(10,10,true);const auto accepted=flow.click(100,171);result=session_app::serve_startup(flow,accepted,session,data,91,71);
    check(result.error.empty()&&result.progress==session_app::StartupProgress::prepared&&session->prepared(),"actual chooser confirmation did not prepare typed session");
    check(session->snapshot().world.manager_country(0)==Country::england&&session->snapshot().world.transfer_market,"actual country/club choice lost during complete start");
    check(read_save(make_save(session->snapshot()))==session->snapshot(),"actual session snapshot lost data");
    if(!output.empty()){write_savegame(output/"prepared-start.FBM",make_save(session->snapshot()));check(read_save(load_savegame(output/"prepared-start.FBM"))==session->snapshot(),"actual on-disk typed snapshot differs");const auto world=encode_world(session->snapshot().world);std::ofstream f(output/"prepared-world.json",std::ios::binary);f.write(reinterpret_cast<const char*>(world.data()),std::streamsize(world.size()));}
    check(flow.desktop()&&flow.screen_name()=="desktop"&&flow.desktop()->state().managers[0].world_club==session->snapshot().world.managers[0].club,"typed startup did not attach actual global-ID desktop");
    if(!output.empty()){const auto frame=flow.render();const auto palette=load_palette(ui::asset_path(data,"M3PC.COL"));std::ofstream f(output/"first-desktop.ppm",std::ios::binary);f<<"P6\n320 240\n255\n";for(auto p:frame.pixels){const auto c=palette.argb[p];f.put(char(c>>16));f.put(char(c>>8));f.put(char(c));}}
    check(flow.desktop()->state().strength&&*flow.desktop()->state().strength>30,"English desktop still uses missing/zero strength rules");
    const auto untouched=session->snapshot();const auto live_revision=session->revision();
    const auto advance=*std::find_if(flow.desktop()->state().items.begin(),flow.desktop()->state().items.end(),[](const auto& item){return item.kind==12;});
    const auto unsupported=flow.click(advance.x+advance.width/2,advance.y+advance.height/2);
    check(unsupported.request==ui::FlowRequest::kickoff,"actual desktop did not route kickoff to its owner");
    const auto declined=session_app::serve_startup(flow,unsupported,session,data,91,71);
    check(!declined.error.empty()&&session->snapshot()==untouched&&session->revision()==live_revision&&!flow.desktop()->pending(),"unsupported next day fell through to legacy or left a stuck command");
    actual_management(flow,session,data,output);
    actual_save_load(flow,session,data,output);
    ui::StartupFlow mixed(data,{},ui::SetupLeagueSymbol::country_selection);std::unique_ptr<session_app::Session> multi;
    mixed.click(resources.german.u16(layout::Setup_FormatError_RuntimeField3+6*2)+10,resources.german.u8(layout::Setup_FormatError_RuntimeField4+6)+10);
    check(mixed.options().managers==2,"actual setup failed to choose two managers");
    auto selected=session_app::serve_startup(mixed,mixed.click(10,10,true),multi,data,91,72);check(selected.error.empty(),"actual mixed session did not start");
    mixed.click(200,95);mixed.click(10,10,true);selected=session_app::serve_startup(mixed,mixed.click(100,171),multi,data,91,72);
    check(selected.error.empty()&&selected.progress==session_app::StartupProgress::selecting&&multi->snapshot().world.managers.size()==1,"actual first manager confirmation did not resume newer chooser context");
    mixed.click(10,10,true);selected=session_app::serve_startup(mixed,mixed.click(100,171),multi,data,91,72);
    check(selected.error.empty()&&selected.progress==session_app::StartupProgress::prepared&&multi->snapshot().world.manager_country(0)==Country::england&&multi->snapshot().world.manager_country(1)==Country::germany,"actual mixed manager country ownership lost across GUI confirmations");
    check(mixed.desktop()&&mixed.screen_name()=="desktop"&&multi->snapshot().desktop->initial_event&&multi->snapshot().date==0,"mixed session did not attach first desktop");
    const auto event_revision=multi->revision();const auto entered=multi->snapshot();rejects([&]{multi->begin_first_event(event_revision);},"mixed session repeated initial event");check(multi->snapshot()==entered&&multi->revision()==event_revision,"mixed session replay changed state");
    check(read_save(make_save(multi->snapshot()))==multi->snapshot(),"mixed initial event did not survive save");
    if(!output.empty())write_savegame(output/"first-event.FBM",make_save(multi->snapshot()));
    actual_management(mixed,multi,data,output.empty()?output:output/"mixed-management");
    actual_mailbox(data,multi->snapshot(),output.empty()?output:output/"mailbox");
    actual_four_managers(data,output);
    actual_pending(data,output.empty()?output:output/"pending");
    actual_pending_multiplayer(data,output.empty()?output:output/"pending");
    std::cout<<"Actual standalone data-only package: original setup/England picker -> typed confirmation -> all start services/market/snapshot passed; first empty event/desktop attached; remaining management and subsequent day owners still pending\n";
}
}
int main(int argc,char** argv)try {
    const auto resources=synthetic();
    for(int mask=1;mask<8;++mask){const GameScope scope{bool(mask&1),bool(mask&2),bool(mask&4)};if(!scope.club_play())continue;
        ui::SetupOptions options;options.country_selection=true;options.germany=scope.germany;options.england=scope.england;options.world=scope.international;options.league=true;options.managers=int(scope.germany)+int(scope.england);
        session_app::Session session(resources,options,91,71);const auto before=session.snapshot();const auto rev=session.revision();
        rejects([&]{session.prepare_start(rev);},"session allowed start without selected managers");check(session.snapshot()==before&&session.revision()==rev,"failed start leaked session state");
        for(auto country:{Country::germany,Country::england})if(scope.domestic_enabled(country)){const auto context=session.selection_context();const auto league=session.snapshot().world.catalogue.starting_league(country,0);ui::ManagerChoice choice;choice.slot=int(session.snapshot().world.managers.size());choice.master_club=-1;choice.world_club=session.snapshot().world.membership[league.value].clubs[0];choice.country=country;choice.name="Session "+std::to_string(choice.slot);choice.portrait=choice.slot;choice.roster_revision=context.revision;
            session.confirm_manager(choice,context.revision);check(session.snapshot().world.managers.back().club==choice.world_club&&session.revision()==context.revision+1,"session chooser lost stable identity/revision");const auto accepted=session.snapshot();rejects([&]{session.confirm_manager(choice,context.revision);},"session replayed accepted chooser result");check(session.snapshot()==accepted,"replayed manager changed session");}
        check(read_save(make_save(session.snapshot()))==session.snapshot(),"pending session snapshot lost state");
        transfer_snapshot_tests(session.snapshot());
        desktop_tests(session.snapshot(),resources.reference);
        first_event_tests(session.snapshot(),resources.reference);
        if(scope.germany&&scope.england&&scope.international)scheduled_desk_tests(session.snapshot(),resources.reference);
    }
    file_io_tests();
    if(argc>1)actual(argv[1],argc>2?std::filesystem::path(argv[2]):std::filesystem::path{});
    std::cout<<"Six session scopes, owned chooser revisions, Original desktop entry/RNG/country policy, S4/S5/S6/S7/S8/S9 saves, first events/offers and cancelled/stale pending rollback passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
