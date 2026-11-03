// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/save.hpp"
#include "ui/season_entrants.hpp"
namespace bmh::ui {
std::vector<SeasonEntrantsState> world12_season_entrants(const bmh::world12::CareerSnapshot&,const LeImage&,const TextDb&);
}
