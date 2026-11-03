// SPDX-License-Identifier: GPL-3.0-only
#include "app/world12_national.hpp"
#include "game/world12/match_projection.hpp"
#include "game/world12/national_coach.hpp"
#include "app/world12_desktop.hpp"
#include "game/matchday_setup.hpp"
#include "game/turn.hpp"
#include "game/week.hpp"
#include "game/calendar.hpp"
#include "game/lineup.hpp"
#include "game/match_engine.hpp"
#include <algorithm>
#include <limits>
#include <set>

namespace bmh::app::world12 {
using namespace bmh::world12;
namespace {
DayTicket require_appointment(const CareerSnapshot& state,const ManagerActionOwner& owner,
    const ManagerActionOwner& expected) {
    state.validate();
    if(!owner.scope||owner.scope!=expected.scope||owner.revision!=expected.revision||
       owner.revision==std::numeric_limits<uint64_t>::max()||!state.progress||
       state.progress->phase!=ProgressPhase::scheduled_event||!state.desktop||state.desktop->manager_order.empty())
        throw FormatError("world12: missing or stale national appointment owner/order");
    const auto epoch=calendar_epoch();
    if(epoch.year!=state.progress->epoch_year||epoch.weekday!=state.progress->epoch_weekday)
        throw FormatError("world12: national appointment differs from installed calendar");
    const auto ticket=prepare_day(state);
    if(!ticket||!ticket->leagues.empty()||!ticket->cups.empty()||ticket->others.size()!=1||
       ticket->others[0].domain!=CalendarDomain::shared||ticket->others[0].source_entry!=0x4006)
        throw FormatError("world12: this day is not an isolated national nomination");
    return *ticket;
}
std::set<Country> represented_countries(const World& world) {
    std::set<Country> countries;
    for(const auto& manager:world.managers)countries.insert(world.manager_country(manager.seat));
    return countries;
}
void publish(CareerSnapshot& state,const CareerSnapshot& before,CareerSnapshot candidate,
    ManagerActionOwner& owner,const ManagerActionOwner& expected) {
    candidate.validate();
    if(state!=before||owner.scope!=expected.scope||owner.revision!=expected.revision)
        throw FormatError("world12: national appointment changed while preparing");
    state=std::move(candidate);++owner.revision;
}
}
NationalAppointmentOutcome prepare_national_appointment(CareerSnapshot& state,
    ManagerActionOwner& owner,ManagerActionOwner expected,const GameState& reference) {
    (void)require_appointment(state,owner,expected);
    const auto before=state;auto candidate=state;WatcomRandom random(state.watcom_seed);
    NationalAppointmentOutcome out;
    if(candidate.world.national_coaches)activate_national_coaches(candidate.world,candidate.desktop->manager_order);
    const auto countries=candidate.world.national_coaches?std::set<Country>{Country::germany,Country::england}:represented_countries(candidate.world);
    for(auto country:countries) {
        const auto result=nominate_national_squad(candidate.world,country,candidate.date,reference,random);
        out.coach_desk_required|=result.coach.has_value();out.nominations.push_back(result);
    }
    out.watcom_draws=random.calls();candidate.watcom_seed=random.seed();
    publish(state,before,std::move(candidate),owner,expected);return out;
}
NationalAppointmentOutcome finish_uncoached_national_appointment(CareerSnapshot& state,
    ManagerActionOwner& owner,ManagerActionOwner expected,const GameState& reference) {
    (void)require_appointment(state,owner,expected);
    if(state.world.national_coaches){
        for(auto country:{Country::germany,Country::england})if(std::none_of(state.world.national_squads.begin(),state.world.national_squads.end(),[&](const auto& squad){return state.world.clubs.at(squad.team).country==country&&squad.nominated_on==state.date;}))throw FormatError("world12: current real national nominations are incomplete");
        if(national_roles_active(state.world))throw FormatError("world12: actual national coach desks still own nomination");
        const auto before=state;auto candidate=state;std::vector<ClubId> teams;for(const auto& record:*candidate.world.national_coaches)teams.push_back(record.team);const auto settings=*candidate.world.settings;auto p=AiMatchProjection::create_for_nationals(candidate,teams,reference,{settings.mode,settings.difficulty,settings.foreign_rule,true});cycle_manager_options(p.state);p.state.memory.add_block(layout::CurrentMatchday,4);GameRandom game(candidate.game_seed);WatcomRandom random(candidate.watcom_seed);draw_matchday_number(p.state,random);event_end(p.state);p.commit_match_state(candidate.world);candidate.desktop->matchday_number=uint16_t(p.state.memory.u32(layout::CurrentMatchday));candidate.game_seed=game.seed();candidate.watcom_seed=random.seed();++candidate.date;++candidate.calendar->next_day;++candidate.calendar->revision;candidate.progress->previous_date=before.date;candidate.progress->entry=DayEntry::after_event;candidate.progress->phase=ProgressPhase::daily_services;NationalAppointmentOutcome out;out.game_draws=game.calls();out.watcom_draws=random.calls();publish(state,before,std::move(candidate),owner,expected);return out;
    }
    for(auto country:represented_countries(state.world)) {
        const auto found=std::find_if(state.world.national_squads.begin(),state.world.national_squads.end(),
            [&](const auto& squad){return state.world.clubs.at(squad.team).country==country;});
        if(found==state.world.national_squads.end()||found->nominated_on!=state.date||found->coach)
            throw FormatError("world12: national nomination is incomplete or needs its coach desk");
    }
    if(state.date==std::numeric_limits<uint32_t>::max()||state.calendar->revision==std::numeric_limits<uint64_t>::max())
        throw FormatError("world12: national appointment date/revision exhausted");
    const auto before=state;auto candidate=state;const auto settings=*state.world.settings;
    const HumanProjectionPolicy policy{settings.mode,settings.difficulty,settings.foreign_rule,true};
    std::vector<ClubId> flagged,humans;
    for(const auto& manager:candidate.world.managers)humans.push_back(manager.club);
    for(const auto& [id,club]:candidate.world.clubs)
        if(club.league.valid()&&(club.match_flags&1)&&std::find(humans.begin(),humans.end(),id)==humans.end())flagged.push_back(id);
    for(auto club:flagged) {
        auto p=AiMatchProjection::create(candidate.world,{club},reference);
        auto_lineup(p.state,0,100);p.commit_match_state(candidate.world);
    }
    auto p=AiMatchProjection::create_for_humans(candidate,humans,reference,policy);auto& local=p.state;
    const auto order_address=layout::NextDeskManager;
    local.memory.add_block(order_address,uint32_t(humans.size()*2));
    for(size_t i=0;i<candidate.desktop->manager_order.size();++i) {
        const auto seat=candidate.desktop->manager_order[i];size_t index=0;
        while(index<p.human_managers()&&p.global_manager(uint8_t(index))!=seat)++index;
        if(index==p.human_managers())throw FormatError("world12: event order cannot resolve its global manager");
        local.memory.set16(order_address+uint32_t(i*2),uint16_t(index));
    }
    local.memory.add_block(layout::CurrentMatchday,4);
    std::vector<uint8_t> referees;MatchdayContext context;
    context.month=month_code(candidate.date);context.weekday=weekday_code(candidate.date);
    context.season_day=int(candidate.date-candidate.progress->season_start);context.referees=&referees;
    GameRandom game(candidate.game_seed);WatcomRandom random(candidate.watcom_seed);MinuteReport report;
    const auto groups=setup_matchday(local,0,context,game,random,report);
    if(report.unsupported||!groups.empty())throw FormatError(report.reason.empty()?"world12: national nomination unexpectedly prepared matches":report.reason);
    draw_matchday_number(local,random);const auto order=shuffle_manager_order(local,game);event_end(local);
    p.commit_match_state(candidate.world);
    candidate.desktop->manager_order.clear();
    for(auto seat:order)candidate.desktop->manager_order.push_back(p.global_manager(uint8_t(seat)));
    candidate.desktop->matchday_number=uint16_t(local.memory.u32(layout::CurrentMatchday));
    candidate.game_seed=game.seed();candidate.watcom_seed=random.seed();
    // This owner has consumed the one non-playing appointment. It must not use
    // the league/cup score coordinator to pretend that a national match occurred.
    ++candidate.date;++candidate.calendar->next_day;++candidate.calendar->revision;
    candidate.progress->previous_date=before.date;candidate.progress->entry=DayEntry::after_event;
    candidate.progress->phase=ProgressPhase::daily_services;
    NationalAppointmentOutcome out;out.game_draws=game.calls();out.watcom_draws=random.calls();
    publish(state,before,std::move(candidate),owner,expected);return out;
}
DesktopEntryOutcome begin_national_nomination_desks(CareerSnapshot& state,ManagerActionOwner& owner,ManagerActionOwner expected,const GameState& reference){
    (void)require_appointment(state,owner,expected);if(state.desktop->initial_event||state.desktop->active_seat)throw FormatError("world12: national nomination desks already started");
    for(const auto& squad:state.world.national_squads)if(squad.coach&&squad.nominated_on!=state.date)throw FormatError("world12: national coach nomination is stale");
    const auto actors=national_desktop_actors(state);if(actors.empty())throw FormatError("world12: national nomination has no active coach");const auto before=state;auto candidate=state;std::vector<ClubId> teams;for(const auto& actor:actors)teams.push_back(actor.team);const auto settings=*candidate.world.settings;auto p=AiMatchProjection::create_for_nationals(candidate,teams,reference,{settings.mode,settings.difficulty,settings.foreign_rule,true});auto& local=p.state;cycle_manager_options(local);local.memory.add_block(layout::CurrentMatchday,4);GameRandom game(candidate.game_seed);WatcomRandom random(candidate.watcom_seed);draw_matchday_number(local,random);const auto order=shuffle_manager_order(local,game);p.commit_match_state(candidate.world);
    InitialDeskEvent event;event.matchday_number=uint16_t(local.memory.u32(layout::CurrentMatchday));for(auto index:order)event.manager_order.push_back(p.global_manager(uint8_t(index)));size_t n=0;for(auto& seat:candidate.desktop->manager_order)if(std::any_of(actors.begin(),actors.end(),[&](const auto& actor){return actor.seat==seat;}))seat=event.manager_order.at(n++);candidate.desktop->matchday_number=event.matchday_number;candidate.desktop->initial_event=event;candidate.game_seed=game.seed();candidate.watcom_seed=random.seed();ManagerActionOwner staged{owner.scope,0};const auto entered=enter_initial_manager_desktop(candidate,staged,staged,event.manager_order.front(),reference);candidate.validate();if(state!=before||owner.scope!=expected.scope||owner.revision!=expected.revision)throw FormatError("world12: national nomination desk owner changed");state=std::move(candidate);++owner.revision;return {game.calls()+entered.game_draws,random.calls()+entered.watcom_draws,entered.autosave_due};
}
NationalAppointmentOutcome finish_national_nomination_desks(CareerSnapshot& state,ManagerActionOwner& owner,ManagerActionOwner expected,const GameState& reference){
    (void)require_appointment(state,owner,expected);if(!state.desktop->initial_event||!state.desktop->active_seat||state.desktop->autosave_pending||state.desktop->initial_event->offer||size_t(state.desktop->initial_event->manager_position)+1!=state.desktop->initial_event->manager_order.size())throw FormatError("world12: national nomination desk still has an owner");
    const auto before=state;auto candidate=state;const auto actors=national_desktop_actors(candidate);std::vector<ClubId> teams;for(const auto& actor:actors)teams.push_back(actor.team);const auto settings=*candidate.world.settings;auto p=AiMatchProjection::create_for_nationals(candidate,teams,reference,{settings.mode,settings.difficulty,settings.foreign_rule,true});event_end(p.state);p.commit_match_state(candidate.world);candidate.progress->last_manager=*candidate.desktop->active_seat;candidate.desktop->initial_event.reset();candidate.desktop->active_seat.reset();++candidate.date;++candidate.calendar->next_day;++candidate.calendar->revision;candidate.progress->previous_date=before.date;candidate.progress->entry=DayEntry::after_event;candidate.progress->phase=ProgressPhase::daily_services;publish(state,before,std::move(candidate),owner,expected);return {};
}

}
