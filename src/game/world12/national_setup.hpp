// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/initialization.hpp"
namespace bmh::world12 {
// Stage the actual complete master kernels, then retain the real population and
// Source market/pool receipts without admitting any club simulation or finance.
NationalStartupReceipt initialize_national_only_players(World& selected,const World& master,
    Country primary,const InitialStrengthRanges&,const GameState& reference,bool foreign_rule,
    GameRandom&,WatcomRandom&);
NationalViewPreferences initial_national_view_preferences(const GameState&,uint8_t canonical_seat);
}
