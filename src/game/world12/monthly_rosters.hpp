// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/loan_returns.hpp"
namespace bmh::world12 {
struct MonthlyContractHooks {
    // A successful renewal must actually update the staged Source contract.
    std::function<std::optional<bool>(GameState&,GameRandom&,WatcomRandom&,
        uint8_t global_manager,PlayerId,int row,const AiMatchProjection&)> renew;
};
struct MonthlyRosterOutcome {uint64_t game_draws=0,watcom_draws=0;size_t released=0;};
// Accounts followed by the exact monthly board pass for one global manager.
MonthlyRosterOutcome settle_monthly_roster(CareerSnapshot&,uint8_t seat,
    const GameState&,const TextDb&,const MonthlyContractHooks&);
}
