// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "app/world12_manager.hpp"
#include "game/world12/save.hpp"
namespace bmh::app::world12 {
// Copies the verified Original menu layout, without interpreting it as club or
// player storage. No event, random draw or desk entry is performed here.
void initialize_desktops(bmh::world12::CareerSnapshot&,const GameState& reference);
struct DesktopEntryOutcome {
    uint64_t game_draws=0,watcom_draws=0;
    bool autosave_due=false;
};
// Initial, zero-round DeskDate or an owned scheduled desktop cursor, using the
// German numeric reference profile. The event owner prepares fixtures/order, pre-match offers
// and pools BEFORE this boundary. It does not prepare or finish a calendar day.
// Autosave is signalled to the owner; no file is written by this transaction.
DesktopEntryOutcome enter_initial_manager_desktop(bmh::world12::CareerSnapshot&,
    ManagerActionOwner&,ManagerActionOwner expected,uint8_t seat,const GameState& reference);
}
