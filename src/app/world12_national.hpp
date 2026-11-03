// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "app/world12_manager.hpp"
#include "app/world12_desktop.hpp"
#include "game/world12/national.hpp"
#include "game/world12/progression.hpp"
namespace bmh::app::world12 {
struct NationalAppointmentOutcome {
    std::vector<bmh::world12::NationalNominationOutcome> nominations;
    uint64_t game_draws=0,watcom_draws=0;
    bool coach_desk_required=false;
};
// The exact scheduled nomination, including every represented home country.
// Selection commits before a coached desk can be shown; the date remains owned.
NationalAppointmentOutcome prepare_national_appointment(bmh::world12::CareerSnapshot&,
    ManagerActionOwner&,ManagerActionOwner expected,const GameState&);
// No human coach: perform the real empty setup, option cycle, persistent-order
// shuffle and event closure. Daily economy remains the next saved phase.
NationalAppointmentOutcome finish_uncoached_national_appointment(bmh::world12::CareerSnapshot&,
    ManagerActionOwner&,ManagerActionOwner expected,const GameState&);
DesktopEntryOutcome begin_national_nomination_desks(bmh::world12::CareerSnapshot&,ManagerActionOwner&,ManagerActionOwner expected,const GameState&);
NationalAppointmentOutcome finish_national_nomination_desks(bmh::world12::CareerSnapshot&,ManagerActionOwner&,ManagerActionOwner expected,const GameState&);
}
