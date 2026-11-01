// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "app/world12_cups.hpp"
#include "app/world12_manager.hpp"
namespace bmh::app::world12 {
struct InitialCompetitionOutcome {
    std::vector<bmh::world12::CupDrawNotice> domestic_draws;
    uint64_t game_draws=0,watcom_draws=0;
};
// Season-zero competition preparation, through the initial domestic draws.
// Tickets, seasonal player/reset services and first-day advancement follow this
// boundary. This does not pretend that the desktop is ready yet.
InitialCompetitionOutcome prepare_initial_competitions(bmh::world12::CareerSnapshot&,
    ManagerActionOwner&,ManagerActionOwner expected,const bmh::world12::SeasonCalendar&,
    const CupSources&,const bmh::GameState& reference);
}
