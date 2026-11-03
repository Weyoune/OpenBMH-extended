// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/save.hpp"
namespace bmh::world12 {
// A fixture keeps its original league/round/result cell when its date changes.
std::vector<FixtureReschedule> reschedulable_fixtures(const CareerSnapshot&,uint8_t seat);
std::vector<uint16_t> reschedule_dates(const CareerSnapshot&,uint8_t seat,const FixtureReschedule&);
void move_fixture(CareerSnapshot&,uint8_t seat,const FixtureReschedule&,uint16_t target_day);
void undo_fixture_move(CareerSnapshot&,uint8_t seat,const FixtureReschedule&);
void validate_fixture_moves(const CareerSnapshot&);
bool replay_on(const CareerSnapshot&,uint16_t day);
std::vector<Fixture> selected_league_fixtures(const World&,LeagueId,int round,const std::vector<uint8_t>& indices);
int visible_league_round(const CareerSnapshot&,LeagueId);
}
