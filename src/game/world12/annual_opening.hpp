// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/monthly_market.hpp"
namespace bmh::world12 {
struct AnnualOpeningStage {
    CareerSnapshot snapshot;
    uint64_t game_draws=0,watcom_draws=0;
    size_t days=0,training_passes=0,new_listings=0;
};
// Source SeasonEndSteps after actual history/highscore/length handling. Input is
// the unpublished new-year World/results on1July with no old calendar/progress
// cursor. Daily interest (without stock updates), real human training and the
// new transfer list finish before the first Wednesday calendar/opening desk.
AnnualOpeningStage stage_annual_opening_services(const CareerSnapshot& new_year,
    const GameState& reference,const TextDb&,const MonthlyMarketPolicy&);
}
