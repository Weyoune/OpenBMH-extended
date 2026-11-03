// SPDX-License-Identifier: GPL-3.0-only
#include "app/world12_cups.hpp"
#include <algorithm>
#include <stdexcept>
#include <type_traits>
#include <limits>
#include <set>

namespace bmh::app::world12 {
using namespace bmh::world12;
static_assert(std::is_nothrow_move_assignable_v<CareerSnapshot>);
std::vector<CupDrawNotice> install_domestic_cup_fields(CareerSnapshot& state,
    const CountryCupRanking& german,const CountryCupRanking& english,const CupSources& sources) {
    state.validate();
    if(!state.cups.empty() || (state.calendar && state.calendar->next_day!=0) ||
        std::any_of(state.league_rounds.begin(),state.league_rounds.end(),[](auto round){return round!=0;}))
        throw std::invalid_argument("world12: initial cup fields require an unstarted season");
    auto candidate=state;GameRandom game(state.game_seed);
    if(candidate.world.catalogue.scope.germany)candidate.cups=initial_domestic_cup_fields(candidate.world,Country::germany,german,sources.german,game);
    if(candidate.world.catalogue.scope.england) {
        auto english_fields=initial_domestic_cup_fields(candidate.world,Country::england,english,sources.english,game);
        candidate.cups.insert(candidate.cups.end(),english_fields.begin(),english_fields.end());
    }
    candidate.game_seed=game.seed();candidate.validate();
    std::vector<CupDrawNotice> notices;
    for(const auto& cup:candidate.cups)if(auto notice=cup_draw_notice(candidate.world,cup))notices.push_back(std::move(*notice));
    state=std::move(candidate);return notices;
}
DayPresentation commit_day_with_domestic_draws(CareerSnapshot& state,const DayTicket& ticket,
    const std::vector<CompletedLeagueRound>& leagues,const std::vector<CupScores>& scores,const CupSources& sources,const std::vector<EuropeanResults>& european,const std::optional<IndoorResults>& indoor,const GameState* indoor_reference,const std::optional<RegionalPlayoffResults>& regional,const std::optional<NationalResults>& national,const std::vector<FriendlyResults>& friendlies) {
    const auto expected=prepare_day(state);
    if(!expected || *expected!=ticket || scores.size()!=ticket.cups.size())
        throw std::invalid_argument("world12: stale or incomplete cup day score input");
    auto candidate=state;WatcomRandom random(state.watcom_seed);
    std::vector<CupLegResult> completed;
    for(size_t i=0;i<scores.size();++i) {
        const auto& score=scores[i];const auto& plan=ticket.cups[i];
        if(score.cup!=plan.cup || score.source_entry!=plan.source_entry || score.revision!=plan.revision)
            throw std::invalid_argument("world12: cup day score input belongs to another stage");
        const auto cup=std::find_if(candidate.cups.rbegin(),candidate.cups.rend(),[&](const auto& stage){return stage.cup==score.cup;});
        const auto& source=cup_country(score.cup)==Country::germany?sources.german:sources.english;
        completed.push_back(draw_after_cup_leg(candidate.world,*cup,score.scores,source,random));
    }
    candidate.watcom_seed=random.seed();
    auto notices=commit_day(candidate,ticket,leagues,completed,european,&sources.german,indoor,indoor_reference,regional,national,state.world.national_campaigns&&state.world.national_campaigns->primary_country==Country::england?&sources.english:&sources.german,friendlies);
    notices.watcom_draws+=random.calls();candidate.validate();state=std::move(candidate);return notices;
}

CupDayOutcome play_domestic_cup_day(CareerSnapshot& state,ManagerActionOwner& owner,
    ManagerActionOwner expected,const DayTicket& ticket,const GameState& reference,
    const CupSources& sources,const std::vector<CupDayConditions>& conditions,
    HumanProjectionPolicy policy,const HumanRoundHooks& hooks) {
    state.validate();
    if(!owner.scope||owner.scope!=expected.scope||owner.revision!=expected.revision||
       owner.revision==std::numeric_limits<uint64_t>::max())throw FormatError("world12: stale domestic match-day owner");
    const auto scheduled=prepare_day(state);
    if(state.prepared_event)throw FormatError("world12: stored match preparation requires its event-consumption owner");
    if(!scheduled||*scheduled!=ticket||!ticket.leagues.empty()||ticket.cups.empty()||conditions.size()!=ticket.cups.size()||
       (state.progress&&state.progress->phase!=ProgressPhase::scheduled_event))
        throw FormatError("world12: incomplete or non-domestic-only match date");
    for(const auto& other:ticket.others)if(!domestic_cups_on(other))
        throw FormatError("world12: a shared/national appointment needs its separate owner");
    std::set<ClubId> participants;
    for(size_t i=0;i<ticket.cups.size();++i) {
        const auto& plan=ticket.cups[i];const auto& input=conditions[i];
        if(input.cup!=plan.cup||input.source_entry!=plan.source_entry||input.revision!=plan.revision||input.matches.size()!=plan.matches.size())
            throw FormatError("world12: cup match conditions belong to another stage");
        for(const auto& match:input.matches)if(match.date!=state.date)throw FormatError("world12: cup match conditions use another date");
        for(const auto& match:plan.matches)for(auto club:{match.home,match.away})
            if(!participants.insert(club).second)throw FormatError("world12: simultaneous cup fixtures repeat a participant");
    }
    const auto before=state;auto candidate=state;GameRandom game(state.game_seed);WatcomRandom random(state.watcom_seed);
    CupDayOutcome out;std::vector<CupScores> scores;
    for(size_t i=0;i<ticket.cups.size();++i) {
        const auto& plan=ticket.cups[i];
        const auto stage=std::find_if(candidate.cups.rbegin(),candidate.cups.rend(),[&](const auto& cup){return cup.cup==plan.cup;});
        const auto& rules=cup_country(plan.cup)==Country::germany?sources.german:sources.english;
        auto result=simulate_and_settle_domestic_cup_round(candidate,reference,*stage,rules,conditions[i].matches,policy,game,random,hooks);
        scores.push_back({plan.cup,plan.source_entry,plan.revision,result.scores});out.cups.push_back(std::move(result));
    }
    candidate.game_seed=game.seed();candidate.watcom_seed=random.seed();
    const auto clock=candidate.progress;candidate.progress.reset();
    out.presentation=commit_day_with_domestic_draws(candidate,ticket,{},scores,sources);
    if(clock){candidate.progress=clock;candidate.progress->previous_date=before.date;
        candidate.progress->entry=DayEntry::after_event;candidate.progress->phase=ProgressPhase::daily_services;}
    candidate.validate();
    if(state!=before||owner.scope!=expected.scope||owner.revision!=expected.revision)
        throw FormatError("world12: domestic match-day state changed during calculation");
    out.game_draws=game.calls();out.watcom_draws=random.calls()+out.presentation.watcom_draws;
    state=std::move(candidate);++owner.revision;return out;
}
}
