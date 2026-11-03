// SPDX-License-Identifier: GPL-3.0-only
#include "app/world12_ai_day.hpp"
#include <algorithm>
#include "game/world12/rescheduling.hpp"
#include <limits>
#include <stdexcept>
#include <type_traits>
namespace bmh::app::world12 {
using namespace bmh::world12;
static_assert(std::is_nothrow_move_assignable_v<CareerSnapshot>);
std::vector<AiLeagueRoundOutcome> simulate_scheduled_leagues(CareerSnapshot& state,const GameState& rules,
    const std::vector<AiDayLeagueConditions>& conditions,const HumanProjectionPolicy* policy,
    GameRandom& game,WatcomRandom& random,const HumanRoundHooks* hooks) {
    std::vector<AiLeagueRoundOutcome> results(conditions.size());std::vector<size_t> human_indices;
    std::vector<HumanLeagueRound> human_rounds;
    for(size_t i=0;i<conditions.size();++i){const auto& input=conditions[i];
        const bool human=std::any_of(state.world.managers.begin(),state.world.managers.end(),[&](const auto& manager){const auto fixtures=selected_league_fixtures(state.world,input.appointment.league,input.appointment.round,input.match_indices);return std::any_of(fixtures.begin(),fixtures.end(),[&](const auto& f){return f.home==manager.club||f.away==manager.club;});});
        if(human){if(!policy||!hooks)throw FormatError("world12: human league lacks its presenter owner");human_indices.push_back(i);human_rounds.push_back({input.appointment.league,input.appointment.round,input.matches,input.match_indices,input.ordinary});}
        if(policy)validate_league_conditions(input.matches);
    }
    if(!human_rounds.empty()){
        auto batch=simulate_and_settle_human_league_rounds(state,rules,human_rounds,*policy,game,random,*hooks);
        for(size_t n=0;n<human_indices.size();++n)results[human_indices[n]]=std::move(batch.leagues[n]);
    }
    for(size_t i=0;i<conditions.size();++i)if(std::find(human_indices.begin(),human_indices.end(),i)==human_indices.end()){
        const auto& input=conditions[i];const auto fixtures=selected_league_fixtures(state.world,input.appointment.league,input.appointment.round,input.match_indices);const bool observed=std::any_of(fixtures.begin(),fixtures.end(),[&](const auto& f){return state.world.clubs.at(f.home).presentation_category.value_or(0)>0||state.world.clubs.at(f.away).presentation_category.value_or(0)>0;});
        if(observed&&policy&&hooks)results[i]=simulate_observed_ai_league_fixtures(state,rules,input.appointment.league,input.appointment.round,input.match_indices,input.matches,*policy,game,random,*hooks);
        else results[i]=simulate_and_settle_ai_league_fixtures(state.world,rules,input.appointment.league,input.appointment.round,input.match_indices,input.matches,game,random);
    }
    return results;
}
static AiDayOutcome play_league_day_impl(CareerSnapshot& state,AiDayOwner& owner,
    const AiDayIdentity& identity,const DayTicket& ticket,const GameState& rules,
    const std::vector<AiDayLeagueConditions>& conditions,const HumanProjectionPolicy* human_policy,const HumanRoundHooks* hooks) {
    if(!owner.scope || identity.scope!=owner.scope || identity.revision!=owner.revision ||
        owner.revision==std::numeric_limits<uint64_t>::max())
        throw std::invalid_argument("world12: stale AI day owner");
    state.validate();
    if(state.prepared_event)throw FormatError("world12: stored match preparation requires its event-consumption owner");
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
        if(input.appointment!=plan.appointment || input.matches.size()!=plan.matches.size()||input.match_indices!=plan.match_indices||input.ordinary!=plan.ordinary)
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
    outcome.leagues=simulate_scheduled_leagues(candidate,rules,conditions,human_policy,game,random,hooks);
    for(size_t i=0;i<ticket.leagues.size();++i){const auto& appointment=ticket.leagues[i].appointment;
        CompletedLeagueRound completed{appointment.league,appointment.round,{},ticket.leagues[i].match_indices,ticket.leagues[i].ordinary};
        for(const auto& match:outcome.leagues[i].matches)completed.results.push_back(match.score);
        scores.push_back(std::move(completed));
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
