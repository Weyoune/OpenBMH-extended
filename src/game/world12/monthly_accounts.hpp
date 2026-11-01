// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/economy.hpp"
#include "formats/textd.hpp"
namespace bmh::world12 {
// These are ordered parts of month_change, not a calendar advancement.
EconomyPhaseOutcome expire_monthly_sponsors(CareerSnapshot&,const GameState&,const TextDb&);
EconomyPhaseOutcome settle_monthly_accounts(CareerSnapshot&,const GameState&);
EconomyPhaseOutcome settle_monthly_investments(CareerSnapshot&,const GameState&);
}
