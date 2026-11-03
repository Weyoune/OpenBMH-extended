// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/annual_qualification.hpp"
namespace bmh::world12 {
struct UnpublishedAnnualMoods {
    CareerSnapshot source_binding,snapshot;
    std::vector<ClubId> physical_order;
    uint64_t game_draws=0,watcom_draws=0;
    bool operator==(const UnpublishedAnnualMoods&)const=default;
};
// Source's first annual RNG phase, before European selection and accounts.
// Uses actual completed cup holders and saved physical amateur order.
UnpublishedAnnualMoods stage_annual_moods(const CareerSnapshot&,const GameState& reference);
void validate_annual_moods(const UnpublishedAnnualMoods&,const GameState& reference);
}
