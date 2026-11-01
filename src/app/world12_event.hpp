// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "app/world12_desktop.hpp"
namespace bmh::app::world12 {
DesktopEntryOutcome begin_scheduled_desks(bmh::world12::CareerSnapshot&,ManagerActionOwner&,
    ManagerActionOwner expected,const GameState& reference);
void finish_scheduled_desks(bmh::world12::CareerSnapshot&,ManagerActionOwner&,ManagerActionOwner expected);
}
