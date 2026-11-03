// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/match_projection.hpp"
#include "game/world12/save.hpp"
#include "game/messages.hpp"
namespace bmh::world12 {
// Initial BSS/default records only, before any day or desk action. Missing source
// fields are an error; older development snapshots are not silently upgraded.
void initialize_career_economy(CareerSnapshot&,const GameState& initial_reference);
ManagerEconomy manager_economy_from_legacy(const GameState&,uint8_t manager);
void install_career_economy(AiMatchProjection&,const World&);
void capture_career_economy(World&,const AiMatchProjection&);
// Shared notices without substituting parked club financial/portfolio state.
void install_career_mailbox(AiMatchProjection&,const World&);
void install_career_pools(AiMatchProjection&,const World&);
void capture_career_pools(World&,const AiMatchProjection&);
void capture_career_mailbox(World&,const AiMatchProjection&);
// Prices/history and every manager's paid totals/holdings use canonical seats.
void capture_career_portfolios(World&,const AiMatchProjection&);
void install_career_portfolios(AiMatchProjection&,const World&);
void install_career_loan_ledger(AiMatchProjection&,const World&);
void capture_career_loan_ledger(World&,const AiMatchProjection&);
void append_economy_message(CareerEconomy&,const EconomyMessage&);
Bytes career_mailbox_bytes(const CareerEconomy&);
MailboxEditFailure edit_career_message(CareerEconomy&,int global_actor,int row,
    const std::array<uint8_t,0xa0>& expected_global,MailboxEditKind);

struct EconomyPhaseOutcome {uint64_t game_draws=0,watcom_draws=0;size_t managers=0;};
// Ordered economic service phases, not the complete daily/calendar transaction.
enum class DailyEconomyKind {after_event,plain_day};
EconomyPhaseOutcome update_daily_stock_market(CareerSnapshot&,const GameState& reference);
EconomyPhaseOutcome daily_finance(CareerSnapshot&,const GameState& reference,DailyEconomyKind);
EconomyPhaseOutcome settle_weekly_managers(CareerSnapshot&,const GameState& reference);
}
