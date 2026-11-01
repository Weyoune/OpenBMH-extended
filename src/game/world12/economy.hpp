// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/match_projection.hpp"
#include "game/world12/save.hpp"
namespace bmh::world12 {
// Initial BSS/default records only, before any day or desk action. Missing source
// fields are an error; older development snapshots are not silently upgraded.
void initialize_career_economy(CareerSnapshot&,const GameState& initial_reference);
void install_career_economy(AiMatchProjection&,const World&);
void capture_career_economy(World&,const AiMatchProjection&);
void append_economy_message(CareerEconomy&,const EconomyMessage&);
struct EconomyPhaseOutcome {uint64_t game_draws=0,watcom_draws=0;size_t managers=0;};
// Ordered economic service phases, not the complete daily/calendar transaction.
enum class DailyEconomyKind {after_event,plain_day};
EconomyPhaseOutcome daily_finance(CareerSnapshot&,const GameState& reference,DailyEconomyKind);
EconomyPhaseOutcome settle_weekly_managers(CareerSnapshot&,const GameState& reference);
}
