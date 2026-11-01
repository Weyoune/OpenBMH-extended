// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/monthly_accounts.hpp"
#include "game/world12/monthly_market.hpp"
#include "game/world12/monthly_rosters.hpp"
namespace bmh::world12 {
struct MonthlyServicesOutcome {
    uint64_t game_draws=0,watcom_draws=0;
    size_t listings=0,loan_returns=0,released=0,managers=0;
};
MonthlyServicesOutcome process_monthly_services(CareerSnapshot&,const GameState&,const TextDb&,
    const MonthlyMarketPolicy&,const LoanReturnHooks&,const MonthlyContractHooks&);
}
