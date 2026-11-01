// SPDX-License-Identifier: GPL-3.0-only
#include "app/world12_event.hpp"
#include "app/world12_first_event.hpp"
#include "game/world12/pools.hpp"
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
    for(const auto& group:candidate.prepared_event->groups)for(const auto& match:group.matches) {
        auto p=AiMatchProjection::create_for_humans(candidate,participants(candidate.world,match.fixture),reference,policy);
        const auto home=int(p.local_club(match.fixture.home)),away=int(p.local_club(match.fixture.away));
        const uint8_t type=group.league?0:group.cup==DomesticCup::league_cup?12:
            group.cup==DomesticCup::german_supercup||group.cup==DomesticCup::english_supercup?9:1;
        prepare_team(p.state,home,match_competition_class(type),0);prepare_team(p.state,away,match_competition_class(type),1);
        auto_lineup(p.state,home,100);auto_lineup(p.state,away,100);
        p.commit_match_state(candidate.world);
    }
    for(const auto& membership:candidate.world.membership)for(const auto club:membership.clubs)
        if(candidate.world.clubs.at(club).match_flags&1) {
            auto p=AiMatchProjection::create_for_humans(candidate,participants(candidate.world,{club,club}),reference,policy);
            auto_lineup(p.state,int(p.local_club(club)),100);p.commit_match_state(candidate.world);
        }
    std::vector<ClubId> humans;for(const auto& manager:candidate.world.managers)humans.push_back(manager.club);
    auto p=AiMatchProjection::create_for_humans(candidate,humans,reference,policy);auto& local=p.state;
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
