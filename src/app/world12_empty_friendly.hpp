// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "app/world12_clock.hpp"
namespace bmh::app::world12 {
ClockStepOutcome consume_empty_friendly_day(bmh::world12::CareerSnapshot&,ManagerActionOwner&,ManagerActionOwner expected,const GameState& actual_reference);
}
