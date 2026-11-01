// SPDX-License-Identifier: GPL-3.0-only
#include "app/world12_ai_day.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <type_traits>
namespace bmh::app::world12 {
using namespace bmh::world12;
static_assert(std::is_nothrow_move_assignable_v<CareerSnapshot>);
static AiDayOutcome play_league_day_impl(CareerSnapshot& state,AiDayOwner& owner,
    const AiDayIdentity& identity,const DayTicket& ticket,const GameState& rules,
    const std::vector<AiDayLeagueConditions>& conditions,const HumanProjectionPolicy* human_policy,const HumanRoundHooks* hooks) {
    if(!owner.scope || identity.scope!=owner.scope || identity.revision!=owner.revision ||
        owner.revision==std::numeric_limits<uint64_t>::max())
        throw std::invalid_argument("world12: stale AI day owner");
    state.validate();
    const auto expected=prepare_day(state);
    if(!expected || *expected!=ticket || conditions.size()!=ticket.leagues.size())
        throw std::invalid_argument("world12: stale or incomplete AI day request");
    if(!ticket.others.empty() || !ticket.cups.empty())
        throw std::invalid_argument("world12: non-league day requires its competition owner");
    if(state.progress&&state.progress->phase!=ProgressPhase::scheduled_event)
        throw std::invalid_argument("world12: league day cannot skip pending calendar services");
    // Reject the entire batch before scoring when a human owns any participant.
    // Unrelated managers remain valid; an AI fixture must never replace them.
    for(size_t i=0;i<ticket.leagues.size();++i) {
        const auto& plan=ticket.leagues[i];const auto& input=conditions[i];
        if(input.appointment!=plan.appointment || input.matches.size()!=plan.matches.size())
            throw std::invalid_argument("world12: AI day conditions belong to another round");
        for(const auto& match:input.matches)if(match.date!=state.date)
            throw std::invalid_argument("world12: AI day conditions use another career date");
        for(const auto& fixture:plan.matches)for(const auto& manager:state.world.managers)
            if(!human_policy&&(manager.club==fixture.home || manager.club==fixture.away))
                throw std::invalid_argument("world12: human match requires the human match owner");
    }
    const auto before=state;const auto old_owner=owner;
    auto candidate=before;GameRandom game(before.game_seed);WatcomRandom random(before.watcom_seed);
    AiDayOutcome outcome;std::vector<CompletedLeagueRound> scores;
    for(size_t i=0;i<ticket.leagues.size();++i) {
        const auto& appointment=ticket.leagues[i].appointment;
        const bool human=std::any_of(candidate.world.managers.begin(),candidate.world.managers.end(),[&](const auto& manager){return candidate.world.clubs.at(manager.club).league==appointment.league;});
        auto round=human?simulate_and_settle_human_league_round(candidate,rules,appointment.league,appointment.round,conditions[i].matches,*human_policy,game,random,*hooks):
            simulate_and_settle_ai_league_round(candidate.world,rules,appointment.league,appointment.round,conditions[i].matches,game,random);
        CompletedLeagueRound completed{appointment.league,appointment.round,{}};
        for(const auto& match:round.matches)completed.results.push_back(match.score);
        scores.push_back(std::move(completed));outcome.leagues.push_back(std::move(round));
    }
    candidate.game_seed=game.seed();candidate.watcom_seed=random.seed();
    const auto clock=candidate.progress;candidate.progress.reset();
    commit_league_day(candidate,ticket,scores);
    if(clock){candidate.progress=clock;candidate.progress->previous_date=before.date;
        candidate.progress->entry=DayEntry::after_event;candidate.progress->phase=ProgressPhase::daily_services;}
    candidate.validate();
    // Hooks/frontends may have changed the actual session while scoring. Never
    // replace that external state or rewind its independently consumed RNG.
    if(state!=before || owner!=old_owner)
        throw std::invalid_argument("world12: career or owner changed during AI day");
    outcome.game_draws=game.calls();outcome.watcom_draws=random.calls();
    state=std::move(candidate);++owner.revision;
    return outcome;
}
AiDayOutcome play_ai_league_day(CareerSnapshot& state,AiDayOwner& owner,const AiDayIdentity& identity,const DayTicket& ticket,
    const GameState& rules,const std::vector<AiDayLeagueConditions>& conditions) {
    return play_league_day_impl(state,owner,identity,ticket,rules,conditions,nullptr,nullptr);
}
AiDayOutcome play_league_day(CareerSnapshot& state,AiDayOwner& owner,const AiDayIdentity& identity,const DayTicket& ticket,
    const GameState& rules,const std::vector<AiDayLeagueConditions>& conditions,HumanProjectionPolicy policy,const HumanRoundHooks& hooks) {
    return play_league_day_impl(state,owner,identity,ticket,rules,conditions,&policy,&hooks);
}

}
