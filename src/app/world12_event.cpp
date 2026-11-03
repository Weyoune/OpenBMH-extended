// SPDX-License-Identifier: GPL-3.0-only
#include "app/world12_event.hpp"
#include "game/world12/progression.hpp"
#include "app/world12_first_event.hpp"
#include "game/world12/pools.hpp"
#include "game/world12/national_coach.hpp"
#include "game/world12/match_projection.hpp"
#include "game/matchday_setup.hpp"
#include "game/lineup.hpp"
#include <algorithm>
#include <limits>
namespace bmh::app::world12 {
using namespace bmh::world12;
namespace {
void owned(const CareerSnapshot& state,const ManagerActionOwner& owner,const ManagerActionOwner& expected) {
    state.validate();
    if(!owner.scope||owner.scope!=expected.scope||owner.revision!=expected.revision||owner.revision==std::numeric_limits<uint64_t>::max()||
       !state.prepared_event||!state.progress||state.progress->phase!=ProgressPhase::scheduled_event)
        throw FormatError("world12: missing or stale scheduled desk owner");
}
std::vector<ClubId> participants(const World& world,Fixture fixture) {
    std::vector<ClubId> clubs;for(const auto& manager:world.managers)clubs.push_back(manager.club);
    for(auto id:{fixture.home,fixture.away})if(std::find(clubs.begin(),clubs.end(),id)==clubs.end())clubs.push_back(id);
    return clubs;
}
}
DesktopEntryOutcome begin_scheduled_desks(CareerSnapshot& state,ManagerActionOwner& owner,
    ManagerActionOwner expected,const GameState& reference) {
    owned(state,owner,expected);
    if(state.prepared_event->desk_phase!=PreparedEvent::DeskPhase::unstarted||state.prepared_event->gates_booked||
       !state.world.settings)
        throw FormatError("world12: scheduled desks are already prepared or lack their settings");
    const auto before=state;auto candidate=state;const auto settings=*state.world.settings;
    const HumanProjectionPolicy policy{settings.mode,settings.difficulty,settings.foreign_rule,true};
    const bool national=!prepare_day(candidate)->national.empty();
    if(national){
        std::vector<ClubId> teams;for(const auto& group:candidate.prepared_event->groups)for(const auto& match:group.matches)for(auto id:{match.fixture.home,match.fixture.away})if(std::find(teams.begin(),teams.end(),id)==teams.end())teams.push_back(id);
        for(const auto& record:*candidate.world.national_coaches)if(record.active&&std::find(teams.begin(),teams.end(),record.team)==teams.end())teams.push_back(record.team);
        auto p=AiMatchProjection::create_for_nationals(candidate,teams,reference,policy);auto& local=p.state;
        for(const auto& group:candidate.prepared_event->groups){if(!group.national)throw FormatError("world12: a national coach window cannot present parked club desks");for(const auto& match:group.matches){const int home=int(p.local_club(match.fixture.home)),away=int(p.local_club(match.fixture.away));prepare_team(local,home,match_competition_class(uint8_t(*group.national+1)),0);prepare_team(local,away,match_competition_class(uint8_t(*group.national+1)),1);auto_lineup(local,home,100);auto_lineup(local,away,100);}}
        cycle_manager_options(local);local.memory.add_block(layout::CurrentMatchday,4);
        GameRandom game(candidate.game_seed);WatcomRandom random(candidate.watcom_seed);draw_matchday_number(local,random);const auto shuffled=shuffle_manager_order(local,game);p.commit_match_state(candidate.world);
        std::vector<uint8_t> actors;for(auto index:shuffled)actors.push_back(p.global_manager(uint8_t(index)));
        size_t index=0;for(auto& seat:candidate.desktop->manager_order)if(std::find(actors.begin(),actors.end(),seat)!=actors.end())seat=actors.at(index++);
        candidate.desktop->matchday_number=uint16_t(local.memory.u32(layout::CurrentMatchday));candidate.game_seed=game.seed();candidate.watcom_seed=random.seed();
        DesktopEntryOutcome out{game.calls(),random.calls(),false};
        if(actors.empty())candidate.prepared_event->desk_phase=PreparedEvent::DeskPhase::complete;
        else {InitialDeskEvent event;event.matchday_number=*candidate.desktop->matchday_number;event.manager_order=actors;candidate.desktop->initial_event=event;candidate.prepared_event->desk_phase=PreparedEvent::DeskPhase::desks;prepare_scheduled_pools(candidate,reference,random);candidate.watcom_seed=random.seed();ManagerActionOwner staged{owner.scope,0};const auto entered=enter_initial_manager_desktop(candidate,staged,staged,actors.front(),reference);out.game_draws+=entered.game_draws;out.watcom_draws=random.calls()+entered.watcom_draws;out.autosave_due=entered.autosave_due;}
        if(actors.empty())candidate.prepared_event->pools_matches.emplace();
        candidate.validate();if(state!=before||owner.scope!=expected.scope||owner.revision!=expected.revision)throw FormatError("world12: national desk owner changed");state=std::move(candidate);++owner.revision;return out;
    }
    for(const auto& group:candidate.prepared_event->groups)for(const auto& match:group.matches) {
        auto p=group.european?AiMatchProjection::create_for_european(candidate,participants(candidate.world,match.fixture),reference,policy):AiMatchProjection::create_for_humans(candidate,participants(candidate.world,match.fixture),reference,policy);
        const auto home=int(p.local_club(match.fixture.home)),away=int(p.local_club(match.fixture.away));
        p.state.indoor=group.indoor;
        const uint8_t type=group.regional_playoff?14:group.indoor?12:group.league?0:group.european?uint8_t(*group.european)+1:group.cup==DomesticCup::league_cup?12:
            group.cup==DomesticCup::german_supercup||group.cup==DomesticCup::english_supercup?9:1;
        prepare_team(p.state,home,match_competition_class(type),0);prepare_team(p.state,away,match_competition_class(type),1);
        auto_lineup(p.state,home,100);auto_lineup(p.state,away,100);
        p.commit_match_state(candidate.world);
    }
    // Projection commit replaces World storage. Keep its physical visit order
    // outside that storage before the first automatic lineup is committed.
    std::vector<ClubId> ordered_clubs;
    for(const auto& membership:candidate.world.membership)
        ordered_clubs.insert(ordered_clubs.end(),membership.clubs.begin(),membership.clubs.end());
    for(const auto club:ordered_clubs)if(candidate.world.clubs.at(club).match_flags&1) {
            auto p=AiMatchProjection::create_for_humans(candidate,participants(candidate.world,{club,club}),reference,policy);
            p.state.indoor=std::any_of(candidate.prepared_event->groups.begin(),candidate.prepared_event->groups.end(),[](const auto& group){return group.indoor;});auto_lineup(p.state,int(p.local_club(club)),100);p.commit_match_state(candidate.world);
        }
    std::vector<ClubId> humans;for(const auto& manager:candidate.world.managers)humans.push_back(manager.club);
    auto p=AiMatchProjection::create_for_humans(candidate,humans,reference,policy);auto& local=p.state;
    local.indoor=std::any_of(candidate.prepared_event->groups.begin(),candidate.prepared_event->groups.end(),[](const auto& group){return group.indoor;});
    cycle_manager_options(local);
    local.memory.add_block(layout::CurrentMatchday,4);local.memory.add_block(layout::NextDeskManager,humans.size()*2);
    for(size_t i=0;i<candidate.desktop->manager_order.size();++i){size_t n=0;
        while(n<p.human_managers()&&p.global_manager(uint8_t(n))!=candidate.desktop->manager_order[i])++n;
        if(n==p.human_managers())throw FormatError("world12: scheduled order cannot resolve global manager");
        local.memory.set16(layout::NextDeskManager+uint32_t(i*2),uint16_t(n));}
    GameRandom game(candidate.game_seed);WatcomRandom random(candidate.watcom_seed);
    draw_matchday_number(local,random);const auto order=shuffle_manager_order(local,game);
    p.commit_match_state(candidate.world);candidate.desktop->manager_order.clear();
    for(auto seat:order)candidate.desktop->manager_order.push_back(p.global_manager(uint8_t(seat)));
    candidate.desktop->matchday_number=uint16_t(local.memory.u32(layout::CurrentMatchday));
    InitialDeskEvent event;event.matchday_number=*candidate.desktop->matchday_number;event.manager_order=candidate.desktop->manager_order;
    candidate.desktop->initial_event=std::move(event);candidate.prepared_event->desk_phase=PreparedEvent::DeskPhase::desks;
    candidate.desktop->initial_event->offer=prepare_manager_desk_offer(candidate,candidate.desktop->manager_order.front(),reference,random);
    prepare_scheduled_pools(candidate,reference,random);
    candidate.game_seed=game.seed();candidate.watcom_seed=random.seed();ManagerActionOwner staged{owner.scope,0};
    const auto entered=enter_initial_manager_desktop(candidate,staged,staged,candidate.desktop->manager_order.front(),reference);
    candidate.validate();if(state!=before||owner.scope!=expected.scope||owner.revision!=expected.revision)
        throw FormatError("world12: scheduled desk preparation changed its source state");
    DesktopEntryOutcome out{game.calls()+entered.game_draws,random.calls()+entered.watcom_draws,entered.autosave_due};
    state=std::move(candidate);++owner.revision;return out;
}
void finish_scheduled_desks(CareerSnapshot& state,ManagerActionOwner& owner,ManagerActionOwner expected) {
    owned(state,owner,expected);
    if(state.prepared_event->desk_phase!=PreparedEvent::DeskPhase::desks||!state.desktop->initial_event||
       !state.desktop->active_seat||state.desktop->autosave_pending||state.desktop->initial_event->offer||
       size_t(state.desktop->initial_event->manager_position)+1!=state.desktop->initial_event->manager_order.size())
        throw FormatError("world12: scheduled manager loop has unfinished decisions");
    auto candidate=state;candidate.progress->last_manager=*candidate.desktop->active_seat;
    candidate.desktop->initial_event.reset();candidate.desktop->active_seat.reset();
    candidate.prepared_event->desk_phase=PreparedEvent::DeskPhase::complete;candidate.validate();
    state=std::move(candidate);++owner.revision;
}
}
