// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/save.hpp"
#include "ui/season_entrants.hpp"
namespace bmh::ui {
std::vector<SeasonEntrantsState> world12_season_entrants(const bmh::world12::CareerSnapshot&,const LeImage&,const TextDb&);
std::vector<SeasonEntrantsState> world12_season_entrants(const bmh::world12::World&,
    const std::array<std::vector<bmh::world12::ClubId>,3>& actual_fields,const LeImage&,const TextDb&);
}
