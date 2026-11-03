// SPDX-License-Identifier: GPL-3.0-only
#include "app/world12_european.hpp"
#include <algorithm>
#include <limits>
#include <set>
namespace bmh::app::world12 {
using namespace bmh::world12;
CompetitionDayOutcome play_competition_day(CareerSnapshot& state,ManagerActionOwner& owner,
    ManagerActionOwner expected,const DayTicket& ticket,const GameState& reference,
    const CupSources& sources,const std::vector<AiDayLeagueConditions>& leagues,
    const std::vector<CupDayConditions>& cups,const std::vector<EuropeanDayConditions>& european,
    HumanProjectionPolicy policy,const HumanRoundHooks& hooks,const std::vector<IndoorDayConditions>& indoor){
    state.validate();
    if(!owner.scope||owner.scope!=expected.scope||owner.revision!=expected.revision||owner.revision==std::numeric_limits<uint64_t>::max()||state.prepared_event)
        throw FormatError("world12: stale combined match-day owner or unconsumed preparation");
    const auto current=prepare_day(state);
    if(!current||*current!=ticket||leagues.size()!=ticket.leagues.size()||cups.size()!=ticket.cups.size()||european.size()!=ticket.european.size()||indoor.size()!=ticket.indoor.size()||
       (state.progress&&state.progress->phase!=ProgressPhase::scheduled_event))throw FormatError("world12: incomplete combined match-day ticket");
    for(const auto& appointment:ticket.others)if(!domestic_cups_on(appointment)&&!european_cups_on(appointment)&&!german_indoor_appointment(appointment))throw FormatError("world12: national/indoor date requires its separate result owner");
    std::set<ClubId> participants;
    const auto conditions=[&](const auto& matches,const auto& inputs){if(matches.size()!=inputs.size())throw FormatError("world12: incomplete match conditions");validate_league_conditions(inputs);for(const auto& condition:inputs)if(condition.date!=state.date)throw FormatError("world12: match conditions use another date");for(const auto& fixture:matches)for(auto club:{fixture.home,fixture.away})if(!participants.insert(club).second)throw FormatError("world12: simultaneous scheduled fixtures repeat a club");};
    for(size_t i=0;i<leagues.size();++i){if(leagues[i].appointment!=ticket.leagues[i].appointment)throw FormatError("world12: league conditions use another appointment");conditions(ticket.leagues[i].matches,leagues[i].matches);}
    for(size_t i=0;i<cups.size();++i){const auto& plan=ticket.cups[i];const auto& input=cups[i];if(plan.cup!=input.cup||plan.source_entry!=input.source_entry||plan.revision!=input.revision)throw FormatError("world12: domestic conditions use another stage");conditions(plan.matches,input.matches);}
    for(size_t i=0;i<european.size();++i){if(european[i].group!=ticket.european[i])throw FormatError("world12: European conditions use another stage/leg");conditions(ticket.european[i].matches,european[i].matches);}
    for(size_t i=0;i<indoor.size();++i){if(indoor[i].group!=ticket.indoor[i])throw FormatError("world12: indoor conditions use another tournament/pass");conditions(ticket.indoor[i].matches,indoor[i].matches);}
    const auto before=state;auto candidate=state;GameRandom game(state.game_seed);WatcomRandom random(state.watcom_seed);CompetitionDayOutcome out;std::vector<CompletedLeagueRound> league_scores;std::vector<CupScores> cup_scores;std::vector<EuropeanResults> european_scores;
    const auto pools=[&](const auto& receipt){out.pools.insert(out.pools.end(),receipt.pools.begin(),receipt.pools.end());};
    out.leagues=simulate_scheduled_leagues(candidate,reference,leagues,&policy,game,random,&hooks);
    for(size_t i=0;i<leagues.size();++i){const auto& a=leagues[i].appointment;const auto& receipt=out.leagues[i];
        CompletedLeagueRound scores{a.league,a.round,{}};for(const auto& match:receipt.matches)scores.results.push_back(match.score);league_scores.push_back(std::move(scores));pools(receipt);}
    for(const auto& input:cups){const auto stage=std::find_if(candidate.cups.rbegin(),candidate.cups.rend(),[&](const auto& stage){return stage.cup==input.cup;});const auto& source=cup_country(input.cup)==Country::germany?sources.german:sources.english;
        auto receipt=simulate_and_settle_domestic_cup_round(candidate,reference,*stage,source,input.matches,policy,game,random,hooks);cup_scores.push_back({input.cup,input.source_entry,input.revision,receipt.scores});pools(receipt);out.cups.push_back(std::move(receipt));}
    if(!european.empty()){
        std::vector<EuropeanGroup> groups;std::vector<std::vector<AiLeagueConditions>> conditions;for(const auto& input:european){groups.push_back(input.group);conditions.push_back(input.matches);}
        auto batch=simulate_and_settle_european_groups(candidate,reference,groups,sources.german,conditions,policy,game,random,hooks);
        for(size_t i=0;i<european.size();++i){const auto& group=european[i].group;auto& receipt=batch.groups[i];if(european_scores.empty()||european_scores.back().competition!=group.competition)european_scores.push_back({group.competition,group.revision,{}});european_scores.back().groups.push_back({group.source_entry,group.played,receipt.scores});pools(receipt);out.european.push_back(std::move(receipt));}
    }
    std::optional<IndoorResults> indoor_scores;
    if(!indoor.empty()){std::vector<IndoorGroup> groups;std::vector<std::vector<AiLeagueConditions>> conditions;for(const auto& input:indoor){groups.push_back(input.group);conditions.push_back(input.matches);}auto result=simulate_and_settle_indoor_pass(candidate,reference,groups,conditions,policy,game,random,hooks);indoor_scores=IndoorResults{groups.front().revision,groups.front().tournament,{}};for(size_t i=0;i<groups.size();++i){indoor_scores->groups.push_back({groups[i].source_entry,groups[i].played,result.groups[i].scores});out.indoor.push_back(std::move(result.groups[i]));}}
    candidate.game_seed=game.seed();candidate.watcom_seed=random.seed();const auto clock=candidate.progress;candidate.progress.reset();
    out.presentation=commit_day_with_domestic_draws(candidate,ticket,league_scores,cup_scores,sources,european_scores,indoor_scores,&reference);
    if(clock){candidate.progress=clock;if(candidate.date!=before.date){candidate.progress->previous_date=before.date;candidate.progress->entry=DayEntry::after_event;candidate.progress->phase=ProgressPhase::daily_services;}else candidate.progress->phase=ProgressPhase::scheduled_event;}
    candidate.validate();if(state!=before||owner.scope!=expected.scope||owner.revision!=expected.revision)throw FormatError("world12: combined match-day state changed during calculation");
    out.game_draws=game.calls()+out.presentation.game_draws;out.watcom_draws=random.calls()+out.presentation.watcom_draws;state=std::move(candidate);++owner.revision;return out;
}
}
