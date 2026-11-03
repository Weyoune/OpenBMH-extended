// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/save.hpp"
#include "formats/le.hpp"
namespace bmh::world12 {
// Actual source appointments retain their weekdays at the new first-Wednesday
// epoch. Source's old runtime friendly/replay cells are cleared, never treated
// as newly scheduled fixtures. No random draws or fabricated appointments.
SeasonCalendar make_annual_calendar(const Catalogue&,const LeImage& german,
    const LeImage& english,uint32_t first_wednesday);
// Retain actual previous Masters and Source scratch lineage. The old completed
// owner remains in the outer receipt; this creates only the undrawn next owner.
IndoorSeason make_annual_indoor_season(const World& next_world,const CareerSnapshot& completed,
    const LeImage& german);
}
