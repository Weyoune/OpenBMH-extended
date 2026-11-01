// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "app/world12_desktop.hpp"
namespace bmh::app::world12 {
// Actual initial, empty calendar event: CurrentMatchday, manager shuffle,
// unsolicited offer, empty/open pools coupon, then the first manager's DeskDate.
// Both RNG streams and all affected state commit under one owner revision.
DesktopEntryOutcome prepare_first_desk_event(bmh::world12::CareerSnapshot&,
    ManagerActionOwner&,ManagerActionOwner expected,const GameState& reference);
DesktopEntryOutcome advance_initial_manager(bmh::world12::CareerSnapshot&,
    ManagerActionOwner&,ManagerActionOwner expected,const GameState& reference);
}
