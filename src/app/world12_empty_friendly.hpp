// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "app/world12_clock.hpp"
#include "game/world12/friendlies.hpp"
namespace bmh::app::world12 {
// A calendar entry that can leave its day without any match: a friendly date nobody booked (0x4001), or the
// national Super-Cup date (0x2000) of a country whose champion also won the cup. The caller has established
// that the day ticket holds no fixture at all.
inline bool matchless_appointment(const bmh::world12::OtherAppointment& a){return bmh::world12::friendly_appointment(a)||a.source_entry==0x2000;}
ClockStepOutcome consume_empty_friendly_day(bmh::world12::CareerSnapshot&,ManagerActionOwner&,ManagerActionOwner expected,const GameState& actual_reference);
}
