// SPDX-License-Identifier: GPL-3.0-only
#include "annual_transition_fixture.hpp"
#include "fixture_tables.hpp"
#include "app/world12_session.hpp"
#include "app/world12_pending.hpp"
#include "game/messages.hpp"
#include "game/world12/annual_coach.hpp"
#include "game/world12/national_coach.hpp"
#include "ui/desktop.hpp"
#include <fstream>
#include <iostream>
namespace {
using namespace bmh;using namespace bmh::world12;namespace application=::bmh::app::world12;
void check(bool yes,const char* text){if(!yes)throw std::runtime_error(text);}
template<class F>void rejects(F f,const char* text){bool refused=false;try{f();}catch(const std::exception&){refused=true;}check(refused,text);}
struct Temporary {std::filesystem::path path;~Temporary(){std::error_code ignored;std::filesystem::remove_all(path,ignored);}};
ui::DeskRequest exit_request(const application::Session& session){ui::DeskRequest request;request.kind=ui::DeskRequestKind::advance_turn;request.manager_slot=*session.snapshot().desktop->active_seat;request.revision=session.revision();return request;}
void pending(application::Session& session,const std::filesystem::path& data) {
    if(!session.snapshot().desktop->initial_event->offer)return;
    PendingOfferHooks hooks;hooks.answer=[](const auto&,const auto&){return PendingOfferAnswer::no;};hooks.select_transfer=[](const auto&,const auto&){return PendingOfferTransferChoice{PendingOfferTransferChoiceKind::decline,-1};};hooks.notice=[](const auto&,const auto&){};hooks.human_buyer_allocator_gate=PendingOfferAllocatorGate::zero;
    session.resolve_pending(data,exit_request(session),hooks);
}
}
int main()try {
    set_fixture_tables(bmh::test::authored_fixture_tables());application::RuntimeResources resources;resources.reference=bmh::test::world12::annual_reference();resources.texts=std::make_shared<TextDb>(bmh::test::world12::annual_texts());resources.reference.texts=resources.texts.get();
    resources.reference.memory.add_block(layout::Desktops,5*0x360);resources.reference.memory.add_block(layout::CalendarState_FormatError_RuntimeField2,1);resources.reference.memory.add_block(layout::CalendarState_FormatError_RuntimeField,1);
    resources.german=bmh::test::world12::annual_transition_rules(false);resources.english=bmh::test::world12::annual_transition_rules(true);
    {
        auto reference=resources.reference;reference.manager_records.resize(5*0xb0c);
        auto roles=bmh::test::world12::completed_account_closing(reference,{false,true,true},false);
        const auto unknown=roles.world;check(!close_annual_national_roles(roles.world)&&roles.world==unknown,"unknown old coach role history was guessed");
        initialize_national_coaches(roles.world,reference);activate_national_coaches(roles.world,{3});
        auto expected=roles.world;for(auto& role:*expected.national_coaches){role.coach.reset();role.active=false;}for(auto& squad:expected.national_squads)squad.coach.reset();
        check(close_annual_national_roles(roles.world)&&roles.world==expected,"closing role handback discarded actual national accounts/nominees or reassigned a new coach");
        roles.world.annual_awards_completed=0;activate_national_coaches(roles.world,{3});const auto awards=stage_annual_awards(roles,reference,*resources.texts);
        check(std::none_of(awards.world.national_coaches->begin(),awards.world.national_coaches->end(),[](const auto& role){return role.active||role.coach.has_value();}),"awards published before real national club role handback");
    }
    auto source=bmh::test::world12::annual_transition_closing(resources.reference,resources.german,2);source.progress->phase=ProgressPhase::season_end;source.world.economy->managers[3].offers[4]=2;source.validate();resources.master=source.world;
    Temporary temporary{std::filesystem::temp_directory_path()/"openbmh-authored-session-annual"};std::filesystem::create_directories(temporary.path);{std::ofstream output(temporary.path/"TEXTD.BLK",std::ios::binary);const char header[7]={};output.write(header,7);const auto& blob=resources.texts->blob;output.write(reinterpret_cast<const char*>(blob.data()),blob.size());check(bool(output),"authored Source text fixture could not be written");}
    auto session=application::Session::resume(resources,source,2201,7);const auto old_balance=source.world.managers[0].finance->balance;session.begin_annual_closing(session.revision());
    check(session.revision()==8&&session.snapshot().world.managers[0].finance->balance==old_balance+400000,"Session closing failed actual sponsorship or published multiple outer revisions");
    auto saved=read_save(make_save(session.snapshot()));session=application::Session::resume(resources,saved,2202,8);pending(session,temporary.path);const auto close=exit_request(session);const auto closing_desktop=session;session.finish_annual_closing(close);
    check(session.snapshot().progress->phase==ProgressPhase::annual_transition,"Session did not retain the annual transition owner");rejects([&]{session.finish_annual_closing(close);},"stale closing desk exit replay accepted");
    saved=read_save(make_save(session.snapshot()));session=application::Session::resume(resources,saved,2203,session.revision());
    application::AnnualTransitionHooks hooks;hooks.tax.present=[](const auto&){return true;};hooks.tax.choose=[](const auto&)->std::optional<bool>{return false;};hooks.qualification=[](const auto&){return true;};hooks.promotion=[](const auto&){return true;};hooks.cup_draw=[](const auto&){return true;};hooks.highscore=[](const auto&,bool){return true;};
    const auto outcome=session.publish_annual_year(session.revision(),{},hooks);check(!outcome.career_complete&&session.snapshot().season==1&&session.snapshot().world.settings->duration==1&&session.options().duration==1,"Session did not publish the following year and actual remaining length together");
    saved=read_save(make_save(session.snapshot()));session=application::Session::resume(resources,saved,2204,session.revision());const auto opening_date=session.snapshot().date;session.begin_annual_opening(session.revision());
    auto integrated=closing_desktop;const auto before_integrated=integrated.snapshot();const auto before_revision=integrated.revision();
    rejects([&]{integrated.advance_to_next_desktop(exit_request(integrated),temporary.path,{},{});},"annual turn silently omitted actual presenters");
    check(integrated.snapshot()==before_integrated&&integrated.revision()==before_revision,"rejected complete annual turn leaked closing work");
    check(!integrated.advance_to_next_desktop(exit_request(integrated),temporary.path,{}, {},&hooks,{})&&integrated.snapshot()==session.snapshot()&&integrated.options()==session.options()&&integrated.revision()==before_revision+1,"complete turn did not match actual closing, annual publication and opening kernels");
    pending(session,temporary.path);session.finish_annual_opening(exit_request(session));
    check(session.snapshot().progress&&session.snapshot().progress->season_start==opening_date&&session.snapshot().date==opening_date+1&&session.snapshot().calendar->next_day==1,"Session annual opening did not hand the actual next date to the saved clock");
    saved=read_save(make_save(session.snapshot()));auto resumed=application::Session::resume(resources,saved,2205,session.revision());check(resumed.snapshot()==session.snapshot(),"post-annual saved clock continuation changed real state/RNG");
    std::cout<<"Session annual closing -> saved transition -> actual year publication -> saved opening -> next saved clock passed (authored closing scores, not a played calendar season)\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
