// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "app/world12_cups.hpp"
#include "formats/textd.hpp"
namespace bmh::app::world12 {
struct AttendanceOutcome {
    std::vector<uint32_t> spectators;
    uint64_t game_draws=0,watcom_draws=0;
};
// Books real gates/catering and their histories once, after saved weather and
// the pre-match manager decisions. Publishing spectator counts marks the receipt.
AttendanceOutcome book_event_attendance(bmh::world12::CareerSnapshot&,ManagerActionOwner&,
    ManagerActionOwner expected,const GameState& reference,const TextDb&,const CupSources&);
}
