// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "app/world12_annual_transition.hpp"
#include "game/world12/national_annual.hpp"
namespace bmh::app::world12 {
// National-only July closure has no hidden club closing/opening desks. Actual
// completed national facts, population, account and saved Source setup owners
// stage atomically; continuing output owns inspect_calendar on first Wednesday.
AnnualTransitionOutcome publish_national_annual_transition(bmh::world12::CareerSnapshot&,
    ManagerActionOwner&,ManagerActionOwner expected,const GameState& reference,
    const LeImage& german,const LeImage& english,const TextDb&,const AnnualTransitionHooks&);
}
