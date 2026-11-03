// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/save.hpp"
namespace bmh::world12 {
// Pure state predicate; Source final-score verification occurs in staging.
bool national_annual_ready(const CareerSnapshot&);
}
