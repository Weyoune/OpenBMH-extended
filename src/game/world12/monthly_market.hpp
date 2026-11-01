// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/save.hpp"
#include "game/state.hpp"
#include "formats/textd.hpp"
#include <set>
namespace bmh::world12 {
struct MonthlyMarketPolicy {
    // Actual protected squad identities from the national owner, if any.
    std::set<PlayerId> national_squad;
};
struct MonthlyMarketOutcome {uint64_t watcom_draws=0;size_t added=0;};
// Source monthly CareerFirstSteps listing renewal, without season shuffle,
// loan returns, manager billing or month/date advancement.
MonthlyMarketOutcome renew_monthly_market(CareerSnapshot&,const GameState& reference,
    const TextDb&,const MonthlyMarketPolicy&);
}
