// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "app/world12_manager.hpp"
#include "game/world12/season_awards.hpp"
namespace bmh::app::world12 {
// Atomic closing-award publication only; closing desks, annual accounts and
// next-season qualification/calendar remain separate required owners.
std::vector<bmh::world12::AnnualAward> publish_annual_awards(
    bmh::world12::CareerSnapshot&,ManagerActionOwner&,ManagerActionOwner expected,
    const bmh::GameState& reference,const bmh::TextDb&);
}
