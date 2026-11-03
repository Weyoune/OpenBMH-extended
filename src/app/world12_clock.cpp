// SPDX-License-Identifier: GPL-3.0-only
#include "app/world12_clock.hpp"
#include "game/world12/annual_opening.hpp"
#include "game/week.hpp"
#include "game/world12/match_projection.hpp"
#include "game/world12/national_coach.hpp"
#include "game/matchday_setup.hpp"
#include "game/turn.hpp"
#include "game/driver.hpp"
#include <algorithm>
#include <limits>
#include <type_traits>
namespace bmh::app::world12 {
using namespace bmh::world12;
static_assert(std::is_nothrow_move_assignable_v<CareerSnapshot>);
namespace {
void owner_matches(const ManagerActionOwner& owner,const ManagerActionOwner& expected){
    if(!owner.scope||owner.scope!=expected.scope||owner.revision!=expected.revision||owner.revision==std::numeric_limits<uint64_t>::max())throw FormatError("world12: stale progression owner");
}
void epoch_matches(const CalendarEpoch& epoch){const auto installed=calendar_epoch();if(epoch.year!=installed.year||epoch.weekday!=installed.weekday)throw FormatError("world12: progression calendar differs from installed rules");}
void complete_empty_day(CareerSnapshot& state){const auto ticket=prepare_day(state);if(!ticket||!ticket->leagues.empty()||!ticket->others.empty()||!ticket->cups.empty())throw FormatError("world12: progression cannot skip a scheduled competition");commit_day(state,*ticket,{},{});}
}
static void finish_empty_opening_event(CareerSnapshot& state,ManagerActionOwner& owner,ManagerActionOwner expected,const GameState& reference,CalendarEpoch epoch,bool annual){
    owner_matches(owner,expected);state.validate();epoch_matches(epoch);
    if(state.progress||(annual?!annual_opening_ready(state):bool(state.date||state.season))||!state.desktop||!state.desktop->initial_event||!state.desktop->active_seat||state.desktop->autosave_pending||!state.calendar||state.calendar->next_day)
        throw FormatError("world12: initial event is not ready to finish");
    const auto& event=*state.desktop->initial_event;
    if(event.offer||size_t(event.manager_position)+1!=event.manager_order.size()||*state.desktop->active_seat!=event.manager_order.back())throw FormatError("world12: remaining manager/offer owns the initial desk");
    const auto before=state;auto candidate=state;CareerProgress progress;progress.previous_date=state.date;progress.season_start=annual?state.date:0;progress.last_manager=*state.desktop->active_seat;progress.epoch_year=uint16_t(epoch.year);progress.epoch_weekday=uint8_t(epoch.weekday);
    // Free scratch quotes have no OfferSlots owner. Keep entry40's separately
    // retained bidder history; actual pending offers were rejected above.
    if(candidate.world.transfer_market)for(size_t n=40;n<44;++n){const auto& row=candidate.world.transfer_market->rows[n];if(row&&std::any_of(row->bids.begin(),row->bids.end(),[](const auto& bid){return bid.bidder.valid();}))throw FormatError("world12: unresolved special offer at event end");candidate.world.transfer_market->rows[n].reset();}
    std::vector<ClubId> clubs;for(const auto& m:candidate.world.managers)clubs.push_back(m.club);
    const auto& settings=*candidate.world.settings;auto p=national_roles_active(candidate.world)?AiMatchProjection::create_for_nationals(candidate,clubs,reference,{settings.mode,settings.difficulty,settings.foreign_rule,true}):AiMatchProjection::create_for_humans(candidate,clubs,reference,{settings.mode,settings.difficulty,settings.foreign_rule,true});
    auto& g=p.state;g.pools_open=event.pools_open;g.memory.add_block(layout::CareerSession_DispatchToto_RuntimeField,1);
    const auto saved_records=g.manager_records,saved_options=g.manager_options;std::vector<MatchGroup> groups;std::vector<uint8_t> referees;
    MatchdayContext context;context.month=month_code(candidate.date);context.weekday=weekday_code(candidate.date);context.referees=&referees;
    GameRandom game(candidate.game_seed);WatcomRandom random(candidate.watcom_seed);MinuteReport report;
    kick_off(g,groups,context,random,game,report);MatchEngine engine{g,groups,game,random,referees};play_matchday(engine,g,groups,{},game,random,report);
    finish_event(g,groups,0,{},saved_records,saved_options,game,random,report);if(report.unsupported)throw FormatError(report.reason);
    p.commit_match_state(candidate.world);
    // KickOff also clears this flag on the remaining canonical squad/pool
    // references. They need no fabricated full-world numeric roster.
    const auto clear=[&](PlayerId id){if(id.valid())candidate.world.players.at(id).attributes.w364&=~0x1000u;};
    for(const auto& [id,club]:candidate.world.clubs)for(const auto& row:club.roster)clear(row.player);
    if(candidate.world.national_origins)for(const auto& origin:*candidate.world.national_origins)for(const auto& row:origin.club.roster)clear(row.player);
    for(const auto& squad:candidate.world.national_squads)for(const auto& row:squad.roster)clear(row.player);
    if(candidate.world.free_players){for(auto id:candidate.world.free_players->domestic)clear(id);for(auto id:candidate.world.free_players->foreign)clear(id);}
    if(candidate.world.transfer_market)for(const auto& row:candidate.world.transfer_market->rows)if(row)clear(row->player);
    candidate.game_seed=game.seed();candidate.watcom_seed=random.seed();
    candidate.desktop->manager_order=event.manager_order;
    candidate.desktop->matchday_number=event.matchday_number;
    candidate.desktop->initial_event.reset();candidate.desktop->active_seat.reset();complete_empty_day(candidate);candidate.progress=progress;candidate.validate();
    if(state!=before)throw FormatError("world12: initial event changed while closing");owner_matches(owner,expected);state=std::move(candidate);++owner.revision;
}
void finish_initial_event(CareerSnapshot& state,ManagerActionOwner& owner,ManagerActionOwner expected,const GameState& reference,CalendarEpoch epoch) {
    finish_empty_opening_event(state,owner,expected,reference,epoch,false);
}
void finish_annual_opening_event(CareerSnapshot& state,ManagerActionOwner& owner,ManagerActionOwner expected,const GameState& reference,CalendarEpoch epoch) {
    finish_empty_opening_event(state,owner,expected,reference,epoch,true);
}
void finish_annual_closing_event(CareerSnapshot& state,ManagerActionOwner& owner,ManagerActionOwner expected) {
    owner_matches(owner,expected);state.validate();
    if(!annual_closing_ready(state)||!state.desktop->initial_event||!state.desktop->active_seat||state.desktop->autosave_pending||state.desktop->initial_event->offer||
       size_t(state.desktop->initial_event->manager_position)+1!=state.desktop->initial_event->manager_order.size())throw FormatError("world12: actual closing desks/offers/autosave are not complete");
    const auto before=state;auto candidate=state;candidate.progress->last_manager=*candidate.desktop->active_seat;
    if(candidate.world.transfer_market)for(size_t row=40;row<44;++row){if(candidate.world.transfer_market->rows[row]&&std::any_of(candidate.world.transfer_market->rows[row]->bids.begin(),candidate.world.transfer_market->rows[row]->bids.end(),[](const auto& bid){return bid.bidder.valid();}))throw FormatError("world12: actual closing offer remains unsettled");candidate.world.transfer_market->rows[row].reset();}
    candidate.desktop->initial_event.reset();candidate.desktop->active_seat.reset();candidate.progress->phase=ProgressPhase::annual_transition;candidate.validate();
    if(state!=before)throw FormatError("world12: annual closing owner changed before transition");owner_matches(owner,expected);state=std::move(candidate);++owner.revision;
}
ClockStepOutcome advance_clock_step(CareerSnapshot& state,ManagerActionOwner& owner,ManagerActionOwner expected,const GameState& reference,const TextDb& texts,const ClockHooks& hooks){
    owner_matches(owner,expected);state.validate();if(!state.progress)throw FormatError("world12: no saved progression owner");
    epoch_matches({state.progress->epoch_year,state.progress->epoch_weekday});const auto before=state;auto candidate=state;auto& clock=*candidate.progress;ClockStepOutcome out;
    switch(clock.phase){
    case ProgressPhase::daily_services:{
        DailyServiceContext context;context.previous_date=clock.previous_date;context.season=candidate.season;context.epoch={clock.epoch_year,clock.epoch_weekday};context.entry=clock.entry==DayEntry::after_event?DailyEconomyKind::after_event:DailyEconomyKind::plain_day;
        if(candidate.world.catalogue.scope.club_play()&&weekday_code(clock.previous_date)==2&&weekday_code(candidate.date)==3){if(!hooks.relations)throw FormatError("world12: weekly progression needs current title relations");context.weekly_relations=hooks.relations(candidate);}
        auto services=hooks.services;
        for(auto* active:{&services.auctions.active_seat,&services.loans.active_seat}){if(*active&&**active!=clock.last_manager)throw FormatError("world12: daily service presenter belongs to another manager");*active=clock.last_manager;}
        const auto result=process_daily_services(candidate,reference,texts,context,services);
        // The service wrapper replaces the snapshot: reacquire its saved clock.
        candidate.progress->phase=result.season_end?ProgressPhase::season_end:result.post_event_stock_pending?ProgressPhase::post_event_market:ProgressPhase::inspect_calendar;
        out.game_draws=result.game_draws;out.watcom_draws=result.watcom_draws;break;
    }
    case ProgressPhase::post_event_market:{
        // DayStepRest has already retaken the current national role. Prices and
        // personal portfolios still belong to the canonical parked club desks.
        std::vector<std::pair<ClubId,uint8_t>> retained;
        if(candidate.world.catalogue.scope.club_play()&&national_roles_active(candidate.world)){
            for(const auto& record:*candidate.world.national_coaches)if(record.active)retained.push_back({record.team,*record.coach});
            park_national_coaches(candidate.world);
        }
        const auto result=update_daily_stock_market(candidate,reference);
        for(const auto& [team,seat]:retained){for(auto& record:*candidate.world.national_coaches)if(record.team==team){record.coach=seat;record.active=true;}for(auto& squad:candidate.world.national_squads)if(squad.team==team)squad.coach=seat;}
        candidate.progress->phase=ProgressPhase::inspect_calendar;out.game_draws=result.game_draws;out.watcom_draws=result.watcom_draws;break;
    }
    case ProgressPhase::inspect_calendar:{
        const auto ticket=prepare_day(candidate);if(!ticket)throw FormatError("world12: season transition is required before seeking another date");
        if(!ticket->leagues.empty()||!ticket->others.empty()){clock.phase=ProgressPhase::scheduled_event;break;}
        const auto previous=candidate.date;complete_empty_day(candidate);candidate.progress->previous_date=previous;candidate.progress->entry=DayEntry::plain_day;candidate.progress->phase=ProgressPhase::daily_services;out.date_advanced=true;break;
    }
    case ProgressPhase::scheduled_event:throw FormatError("world12: scheduled event needs its competition/desk owner");
    case ProgressPhase::annual_transition:throw FormatError("world12: actual annual transition owner is required");
    case ProgressPhase::season_end:throw FormatError("world12: annual awards and closing desk are required");
    }
    candidate.validate();if(state!=before)throw FormatError("world12: progression changed during service presentation");owner_matches(owner,expected);out.phase=candidate.progress->phase;state=std::move(candidate);++owner.revision;return out;
}
ClockStepOutcome seek_next_event(CareerSnapshot& state,ManagerActionOwner& owner,ManagerActionOwner expected,const GameState& reference,const TextDb& texts,const ClockHooks& hooks){
    owner_matches(owner,expected);state.validate();if(!state.progress)throw FormatError("world12: no progression search owner");if(state.progress->phase==ProgressPhase::scheduled_event||state.progress->phase==ProgressPhase::season_end||state.progress->phase==ProgressPhase::annual_transition)throw FormatError("world12: progression is already waiting for its event owner");const auto before=state;auto candidate=state;ManagerActionOwner staged{owner.scope,0};ClockStepOutcome out;
    for(size_t n=0;n<4*367;++n){if(candidate.progress->phase==ProgressPhase::scheduled_event||candidate.progress->phase==ProgressPhase::season_end||candidate.progress->phase==ProgressPhase::annual_transition){out.phase=candidate.progress->phase;candidate.validate();if(state!=before)throw FormatError("world12: progression changed during event search");owner_matches(owner,expected);state=std::move(candidate);++owner.revision;return out;}
        const auto step=advance_clock_step(candidate,staged,staged,reference,texts,hooks);out.game_draws+=step.game_draws;out.watcom_draws+=step.watcom_draws;out.date_advanced|=step.date_advanced;if(state!=before)throw FormatError("world12: progression changed during event search");owner_matches(owner,expected);
    }
    throw FormatError("world12: bounded event search exhausted");
}
}
