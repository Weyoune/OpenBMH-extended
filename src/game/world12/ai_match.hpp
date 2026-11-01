// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/match_projection.hpp"
#include "game/world12/fixtures.hpp"
#include "game/random.hpp"
#include "game/match_engine.hpp"
namespace bmh::world12 {
struct AiLeagueConditions {
    int month=0,temperature=0,rain=0;
    uint32_t spectators=0,date=0;
    uint8_t referee=0;
    std::vector<uint8_t> referee_properties;
    std::vector<uint8_t> presentation_buttons;
    // The event owner has already switched competition cards and squad status.
    // Repeating prepare_team after a manager edited the lineup loses that state.
    bool teams_prepared=false;
};
struct AiLeagueOutcome {
    Fixture fixture;
    LeagueResult score;
    uint8_t end_minute=0;
    uint64_t game_draws=0,watcom_draws=0;
    bool settled=false;
};
struct AiLeagueRoundOutcome {
    std::vector<AiLeagueOutcome> matches;
    uint64_t game_draws=0,watcom_draws=0;
};
struct HumanRoundHooks {
    // Trusted synchronous presenters; group storage must stay stable while
    // MatchEngine retains references. Gameplay changes use existing Core actions.
    std::function<void(AiMatchProjection&,bmh::MatchEngine&,const bmh::LiveMatchEvent&)> live_event;
    std::function<void(AiMatchProjection&,bmh::MatchEngine&,int)> after_minute;
    bool decline_unpresented_events=false;
};
// Actual Original scoring phases and AI events; no generated/test score input.
// Conditions/formation tables come from the session owner. After-match economy,
// development, standings and human tactics are deliberately outside this API.
// World match mutations and both RNGs publish atomically on successful completion.
AiLeagueOutcome simulate_ai_league_match(World&,const GameState& rules_template,
    Fixture,const AiLeagueConditions&,GameRandom&,WatcomRandom&);
// Adds the Original AI post-match records, morale and player-development path.
// Does not settle human finances, advance calendar/results or publish a GUI turn.
AiLeagueOutcome simulate_and_settle_ai_league_match(World&,const GameState& rules_template,
    Fixture,const AiLeagueConditions&,GameRandom&,WatcomRandom&);
// One complete league round on the shared Original minute clock. Match RNG
// belongs to the whole group; individual outcome draw counts are zero.
AiLeagueRoundOutcome simulate_and_settle_ai_league_round(World&,const GameState& rules_template,
    LeagueId,int round,const std::vector<AiLeagueConditions>&,GameRandom&,WatcomRandom&);
// Calculation on the day owner's staged snapshot. Includes real human rating,
// development, account/bonus/statistics state. Does not advance results/date.
// Missing human event presenters are an explicit error unless the caller
// deliberately chooses the existing headless decline behavior for a probe.
AiLeagueRoundOutcome simulate_and_settle_human_league_round(CareerSnapshot&,const GameState& rules_template,
    LeagueId,int round,const std::vector<AiLeagueConditions>&,HumanProjectionPolicy,
    GameRandom&,WatcomRandom&,const HumanRoundHooks& hooks={});
}
