// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/manager_creation.hpp"
#include "game/world12/save.hpp"
namespace bmh::world12 {
struct ManagerJoiningOutcome {uint8_t seat=0;uint64_t watcom_draws=0;};
// Takes over the actual selected club. Existing memberships, results, players'
// age/ratings and other humans are retained; new personal state uses shared kernels.
ManagerJoiningOutcome add_running_manager(CareerSnapshot&,const InitialManagerChoice&,
    const GameState& reference,const LeImage& country_rules);
}
