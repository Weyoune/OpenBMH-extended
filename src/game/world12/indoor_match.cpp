// SPDX-License-Identifier: GPL-3.0-only
#include "game/world12/indoor_match.hpp"
#include "game/world12/save.hpp"
#include "game/world12/economy.hpp"
#include "game/cup.hpp"
#include "game/lineup.hpp"
#include "game/matchday_setup.hpp"
#include "game/driver.hpp"
#include "game/matchday.hpp"
#include "game/week.hpp"
#include <algorithm>
#include <set>
namespace bmh::world12 {
IndoorMatchOutcome simulate_and_settle_indoor_pass(CareerSnapshot& state,const GameState& reference,
    const std::vector<IndoorGroup>& planned,const std::vector<std::vector<AiLeagueConditions>>& conditions,
    HumanProjectionPolicy policy,GameRandom& game,WatcomRandom& random,const HumanRoundHooks& hooks){
    state.validate();if(!state.world.indoor||planned!=prepare_indoor_groups(*state.world.indoor)||planned.empty()||conditions.size()!=planned.size())throw FormatError("world12: stale/incomplete indoor match pass");
    std::vector<ClubId> participants;std::set<ClubId> unique;
    for(size_t i=0;i<planned.size();++i){if(conditions[i].size()!=planned[i].matches.size())throw FormatError("world12: incomplete indoor match conditions");for(size_t m=0;m<planned[i].matches.size();++m){const auto& c=conditions[i][m];if(c.date!=state.date||c.month<1||c.month>12||c.referee>=c.referee_properties.size()||c.presentation_buttons.size()<180||c.spectators!=10000)throw FormatError("world12: indoor Source date/referee/spectator conditions are invalid");for(auto club:{planned[i].matches[m].home,planned[i].matches[m].away})if(!unique.insert(club).second)throw FormatError("world12: repeated indoor pass participant");else participants.push_back(club);}}
    const bool human=std::any_of(state.world.managers.begin(),state.world.managers.end(),[&](const auto& manager){return unique.contains(manager.club);});const auto before=state;auto candidate=state;auto staged_game=game;auto staged_random=random;const auto old_game=game.seed();const auto old_random=random.seed();const auto old_game_calls=game.calls(),old_random_calls=random.calls();
    auto projection=human?AiMatchProjection::create_for_humans(candidate,participants,reference,policy):AiMatchProjection::create(candidate.world,participants,reference);if(candidate.world.economy)install_career_economy(projection,candidate.world);auto& local=projection.state;local.indoor=true;local.date=state.date;local.lineups_frozen=false;local.pools_open=false;local.chance_rain=conditions.front().front().rain;local.chance_temperature=conditions.front().front().temperature;
    const auto& tournament=candidate.world.indoor->tournaments.back();
    for(size_t i=0;i<5;++i){const auto& r=candidate.world.indoor->rules[i];const auto at=cup_entry_address(r.entry);local.memory.add_block(at,0x28);local.memory.set8(at+0x14,r.flags);local.memory.set8(at+0x15,r.previous);local.memory.set8(at+0x16,r.next);local.memory.set8(at+0x17,r.parallel);local.memory.set16(at+0x18,11);local.memory.set8(at+0x1a,r.teams);local.memory.set8(at+0x1b,r.rounds);local.memory.set8(at+0x1d,tournament.played[i]);}
    std::vector<MatchGroup> groups;
    for(size_t i=0;i<planned.size();++i){const auto& plan=planned[i];MatchGroup group;group.desc.assign(0xd0,0);group.desc[0]=12;group.desc[1]=plan.flags;group.desc[2]=uint8_t(plan.matches.size());const auto at=cup_entry_address(plan.source_entry);for(size_t b=0;b<4;++b)group.desc[0xcc+b]=uint8_t(at>>(b*8));
        for(size_t m=0;m<plan.matches.size();++m){const auto& c=conditions[i][m];const auto f=plan.matches[m];const auto home=int(projection.local_club(f.home)),away=int(projection.local_club(f.away));group.fixtures.push_back(home);group.fixtures.push_back(away);group.desc[0x10+m]=c.referee;group.desc[0x30+m]=uint8_t(c.temperature);group.desc[0x50+m]=uint8_t(c.rain);
            if(!c.teams_prepared){prepare_team(local,home,match_competition_class(12),0);prepare_team(local,away,match_competition_class(12),1);}for(auto team:{home,away}){auto_lineup(local,team,local.manager_of(team)>=0?0xe4:100);if(local.manager_of(team)>=0)prepare_lineup_entry(local,team);}}
        groups.push_back(std::move(group));}
    MatchdayContext context;context.month=conditions.front().front().month;context.weekday=weekday_code(candidate.date);std::vector<uint8_t> referees=conditions.front().front().referee_properties;context.referees=&referees;MinuteReport report;
    // The common Source kickoff supplies Hall attendance and the real short-
    // squad forfeit path; its engine then plays the original two20minute halves.
    kick_off(local,groups,context,staged_random,staged_game,report);if(report.unsupported)throw FormatError(report.reason);
    MatchEngine engine(local,groups,staged_game,staged_random,referees);engine.indoor=true;engine.month=context.month;engine.buttons=conditions.front().front().presentation_buttons;engine.leave_screens=!human||hooks.decline_unpresented_events;
    if(hooks.live_event)engine.live_event=[&](const auto& event){hooks.live_event(projection,engine,event);};
    if(hooks.before_matchday)hooks.before_matchday(projection,engine);
    play_matchday(engine,local,groups,engine.buttons,staged_game,staged_random,report,hooks.after_minute?std::function<void(int)>([&](int minute){hooks.after_minute(projection,engine,minute);}):std::function<void(int)>{});if(report.unsupported)throw FormatError(report.reason);
    IndoorMatchOutcome out;
    for(auto& group:groups){for(size_t m=0;m<group.end_minute.size();++m)if(!(group.kickoff_void_mask&(1u<<m))&&group.end_minute[m]!=40)throw FormatError("world12: indoor Source match did not reach40minutes");process_group(local,group,staged_game,staged_random,report);if(report.unsupported)throw FormatError(report.reason);CupRoundOutcome result;result.scores=group.cup_results;result.end_minutes=group.end_minute;out.groups.push_back(std::move(result));}
    projection.commit_match_state(candidate.world);if(candidate.world.economy)capture_career_economy(candidate.world,projection);projection.commit_desktop_preferences(candidate);candidate.game_seed=staged_game.seed();candidate.watcom_seed=staged_random.seed();candidate.validate();
    if(state!=before||game.seed()!=old_game||random.seed()!=old_random||game.calls()!=old_game_calls||random.calls()!=old_random_calls)throw FormatError("world12: indoor match owner or random stream changed");
    out.game_draws=staged_game.calls()-old_game_calls;out.watcom_draws=staged_random.calls()-old_random_calls;state=std::move(candidate);game=staged_game;random=staged_random;return out;
}
}
