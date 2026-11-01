// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/initialization.hpp"
#include "game/state.hpp"
namespace bmh::world12 {
struct InitialManagerChoice {
    uint8_t seat=0;ClubId club;LeagueId start_league;
    std::string name;uint8_t portrait=0,strength=1;
};
// Verified numeric facility/environment/sponsor state; no names, pointers or packed IDs.
ManagerCareerState manager_career_state_from_legacy(const GameState&,uint8_t seat,const std::vector<PlayerId>& players={},bool season_services=false);
void install_manager_career_state(GameState&,uint8_t seat,const ManagerCareerState&,const std::vector<PlayerId>& players={});
// Fresh-career confirmation, including identity-preserving placement, strength,
// facilities and the existing account/contract/lineup phase. Rules are supplied
// for the selected country; already accepted managers remain untouched.
PlayerInitializationOutcome add_initial_manager(World&,const InitialManagerChoice&,CareerSettings,
    const InitialStrengthRanges&,const GameState& reference,const LeImage& country_rules,
    GameRandom&,WatcomRandom&);
}
