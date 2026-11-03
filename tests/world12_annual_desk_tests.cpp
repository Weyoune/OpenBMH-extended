// SPDX-License-Identifier: GPL-3.0-only
#include "annual_accounts_fixture.hpp"
#include "fixture_tables.hpp"
#include "app/world12_first_event.hpp"
#include "app/world12_clock.hpp"
#include "app/world12_pending.hpp"
#include "game/world12/annual_archive.hpp"
#include "game/world12/annual_opening.hpp"
#include "game/world12/annual_fields.hpp"
#include "game/calendar.hpp"
#include <iostream>
namespace {
using namespace bmh;using namespace bmh::world12;namespace application=::bmh::app::world12;
void check(bool yes,const char* text){if(!yes)throw std::runtime_error(text);}
template<class F>void rejects(F f,const char* text){bool refused=false;try{f();}catch(const std::exception&){refused=true;}check(refused,text);}
application::ManagerActionOwner* external=nullptr;void disturb(int){draw_hook=nullptr;++external->revision;}
}
int main()try {
    set_fixture_tables(bmh::test::authored_fixture_tables());auto reference=bmh::test::world12::annual_reference();const auto texts=bmh::test::world12::annual_texts();reference.texts=&texts;reference.memory.add_block(layout::Desktops,5*0x360);reference.memory.add_block(layout::CalendarState_FormatError_RuntimeField2,1);reference.memory.add_block(layout::CalendarState_FormatError_RuntimeField,1);
    auto source=bmh::test::world12::completed_account_closing(reference,{false,true,false},false);const auto qualification=stage_annual_qualification(source);const auto archive=stage_annual_title_archive(source);
    auto year=source;year.season=1;year.world.annual_title_archive=archive;year.world.settings->duration=4;year.results=SeasonResults::empty(year.world.catalogue);year.league_rounds.fill(0);year.calendar.reset();year.progress.reset();year.cups.clear();year.world.european_campaigns.reset();year.desktop->manager_order={3};year.desktop->matchday_number=73;year.desktop->autosave_month=7;
    const auto opening=stage_annual_opening_services(year,reference,texts,{});year=opening.snapshot;
    SeasonCalendar schedule;for(const auto& league:year.world.catalogue.leagues)if(league.active)for(uint8_t round=1;round<=league.rounds();++round)schedule.leagues.push_back({uint16_t(7+round*8),league.id,round});std::sort(schedule.leagues.begin(),schedule.leagues.end(),[](const auto& a,const auto& b){return a.day!=b.day?a.day<b.day:a.league<b.league;});year.calendar=CalendarCursor{schedule,0,19};year.validate();check(annual_opening_ready(year),"real first-Wednesday annual opening predicate was refused");
    auto invalid=year;invalid.world.annual_awards_completed=0;check(!annual_opening_ready(invalid),"annual desk accepted unpaid preceding awards");invalid=year;invalid.world.annual_title_archive.reset();check(!annual_opening_ready(invalid),"annual desk accepted missing actual archive");invalid=year;invalid.desktop->manager_order.clear();invalid.desktop->matchday_number.reset();check(!annual_opening_ready(invalid),"annual desk invented an old manager order");
    const auto before=year;application::ManagerActionOwner owner{991,0};const auto entered=application::prepare_annual_desk_event(year,owner,owner,reference);
    check(owner.revision==1&&year.desktop->initial_event&&year.desktop->active_seat==3&&year.desktop->initial_event->manager_order==std::vector<uint8_t>{3}&&year.date==before.date&&year.calendar->next_day==0,"annual desk changed real date/order or failed actor publication");
    check(read_save(make_save(year))==year,"annual owned desk/order failed saved continuation");
    if(year.desktop->initial_event->offer){PendingOfferHooks hooks;hooks.answer=[](const auto&,const auto&){return PendingOfferAnswer::no;};hooks.select_transfer=[](const auto&,const auto&){return PendingOfferTransferChoice{PendingOfferTransferChoiceKind::decline,-1};};hooks.notice=[](const auto&,const auto&){};hooks.human_buyer_allocator_gate=PendingOfferAllocatorGate::zero;application::resolve_initial_offer(year,owner,owner,reference,texts,hooks);}
    check(!year.desktop->autosave_pending,"authored closing month unexpectedly owes another autosave");const auto prepared=year;const auto revision=owner.revision;
    application::finish_annual_opening_event(year,owner,owner,reference,calendar_epoch());
    check(owner.revision==revision+1&&!year.desktop->initial_event&&!year.desktop->active_seat&&year.date==prepared.date+1&&year.calendar->next_day==1&&year.progress&&year.progress->previous_date==prepared.date&&year.progress->season_start==prepared.date&&year.progress->phase==ProgressPhase::daily_services,"annual opening did not finish one real empty Source event");
    check(read_save(make_save(year))==year,"annual event finish lost true date/RNG/progress");const auto after=year;rejects([&]{application::finish_annual_opening_event(year,owner,owner,reference,calendar_epoch());},"annual opening event replay accepted");check(year==after,"annual event replay leaked state");
    auto interrupted=before;application::ManagerActionOwner other{992,0};external=&other;draw_hook=disturb;rejects([&]{application::prepare_annual_desk_event(interrupted,other,{992,0},reference);},"changed annual desk owner was overwritten");draw_hook=nullptr;check(interrupted==before&&other.revision==1,"annual desk interruption leaked candidate or rewound owner");
    {
        auto closing=source;closing.desktop->manager_order={3};closing.desktop->matchday_number=61;closing.desktop->autosave_month=7;closing.world.economy->managers[3].offers[4]=2;
        check(annual_closing_ready(closing),"paid actual closing desk predicate refused completed standings");
        application::ManagerActionOwner close_owner{997,0};const auto old_balance=closing.world.managers[0].finance->balance;
        application::prepare_annual_closing_desk_event(closing,close_owner,close_owner,reference);
        check(closing.world.managers[0].finance->balance==old_balance+400000&&closing.world.economy->managers[3].offers[4]==0,
            "actual Source closing sponsorship goal was skipped, duplicated or booked to another manager");
        if(closing.desktop->initial_event->offer){PendingOfferHooks hooks;hooks.answer=[](const auto&,const auto&){return PendingOfferAnswer::no;};hooks.select_transfer=[](const auto&,const auto&){return PendingOfferTransferChoice{PendingOfferTransferChoiceKind::decline,-1};};hooks.notice=[](const auto&,const auto&){};hooks.human_buyer_allocator_gate=PendingOfferAllocatorGate::zero;application::resolve_initial_offer(closing,close_owner,close_owner,reference,texts,hooks);}
        const auto completed_desks=closing;application::finish_annual_closing_event(closing,close_owner,close_owner);
        check(closing.progress->phase==ProgressPhase::annual_transition&&!closing.desktop->initial_event&&!closing.desktop->active_seat&&closing.date==completed_desks.date&&closing.calendar==completed_desks.calendar&&closing.game_seed==completed_desks.game_seed&&closing.watcom_seed==completed_desks.watcom_seed,
            "closing finish performed a fake kick-off/day advance or replayed RNG");
        check(read_save(make_save(closing))==closing,"actual closing transition phase did not persist");const auto closed=closing;
        rejects([&]{application::prepare_annual_closing_desk_event(closing,close_owner,close_owner,reference);},"closing desks could replay after owned transition");check(closing==closed,"closing replay leaked payout/state");
        (void)completed_annual_titles(closing);
    }
    (void)entered;(void)qualification;std::cout<<"Actual annual opening desk: strict paid/archive/weekday/results/order, Source entry/offer/empty kick-off, save/replay and owner rollback passed (not full annual publication)\n";return 0;
}catch(const std::exception& error){draw_hook=nullptr;std::cerr<<error.what()<<'\n';return 1;}
