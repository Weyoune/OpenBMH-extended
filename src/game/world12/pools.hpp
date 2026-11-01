// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/save.hpp"
#include "game/state.hpp"
namespace bmh::world12 {
// Source BuildPoolsCoupon selection, rebuilt for each manager entry. Betting
// and financial settlement are separate owners; this cannot place a wager.
void prepare_scheduled_pools(CareerSnapshot&,const GameState&,WatcomRandom&);
}
