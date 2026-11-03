// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/annual_qualification.hpp"
namespace bmh::world12 {
// Build the next retained row from complete actual closing owners, without
// changing World, date, season or RNG. Only final annual publication installs it.
AnnualTitleArchive stage_annual_title_archive(const CareerSnapshot&);
// Explicit migration contract for a supplied Original history. Preserve every
// label byte; do not resolve labels by current club name or overwrite known IDs.
void retain_unresolved_source_titles(CareerSnapshot&,const bmh::TitleHistoryArchive&);
}
