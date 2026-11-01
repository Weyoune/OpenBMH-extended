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
struct SeasonTicketReceipt {uint8_t seat=0;uint16_t sold=0;uint32_t income=0;};
struct InitialManagerServicesOutcome {
    std::vector<SeasonTicketReceipt> tickets;
    uint64_t watcom_draws=0;
};
// Through initial ticket sales and the date-zero manager/player reset boundary.
// Fixture shuffling, market creation and daily progression follow afterwards.
InitialManagerServicesOutcome initialize_first_season_manager_services(bmh::world12::CareerSnapshot&,
    ManagerActionOwner&,ManagerActionOwner expected,const bmh::GameState& reference);
struct InitialMarketOutcome {uint64_t watcom_draws=0;size_t listings=0;};
// Source first-step membership shuffle and initial transfer list, atomically.
InitialMarketOutcome initialize_first_season_market(bmh::world12::CareerSnapshot&,
    ManagerActionOwner&,ManagerActionOwner expected,const bmh::GameState* initial_reference=nullptr);
}
