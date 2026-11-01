// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "app/world12_manager.hpp"
#include "game/world12/save.hpp"
#include "game/pending_offer.hpp"
namespace bmh::app::world12 {
PendingOfferResult resolve_initial_offer(bmh::world12::CareerSnapshot&,ManagerActionOwner&,
    ManagerActionOwner expected,const GameState& reference,const TextDb&,const PendingOfferHooks&);
}
