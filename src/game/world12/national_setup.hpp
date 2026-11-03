// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/initialization.hpp"
#include "game/world12/calendar.hpp"
namespace bmh::world12 {
// Stage the actual complete master kernels, then retain the real population and
// Source market/pool receipts without admitting any club simulation or finance.
NationalStartupReceipt initialize_national_only_players(World& selected,const World& master,
    Country primary,const InitialStrengthRanges&,const GameState& reference,bool foreign_rule,
    GameRandom&,WatcomRandom&);
NationalDrawOutcome prepare_national_campaign_from_initial_field(World&,const LeImage&,Country primary,const std::vector<uint16_t>& already_drawn_four,GameRandom&,WatcomRandom&);
SeasonCalendar make_national_only_calendar(const Catalogue&,const std::vector<uint16_t>& primary_source);
NationalViewPreferences initial_national_view_preferences(const GameState&,uint8_t canonical_seat);
}
