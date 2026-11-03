// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/world.hpp"
#include "game/state.hpp"
namespace bmh::world12 {
struct CareerSnapshot;
struct NationalDesktopActor {uint8_t seat=0;ClubId team;Country country=Country::germany;bool operator==(const NationalDesktopActor&)const=default;};
std::vector<NationalDesktopActor> national_desktop_actors(const CareerSnapshot&);
struct NationalSelectionChoice {Country country;ClubId team;bool operator==(const NationalSelectionChoice&)const=default;};
struct NationalCapabilities {
    uint8_t national_only_manager_limit=1;
    std::vector<NationalSelectionChoice> national_only_choices;
    std::string unavailable_reason;
};
NationalCapabilities national_capabilities(const World&);
// Validates the saved manager order; selects its first eligible coach per country.
std::vector<std::pair<ClubId,uint8_t>> select_national_coaches(const World&,const std::vector<uint8_t>& saved_order);
// Fresh Source spare-record capture only. Existing/unknown history is not rebuilt.
void initialize_national_coaches(World&,const GameState& fresh_reference);
void activate_national_coaches(World&,const std::vector<uint8_t>& saved_order);
void validate_national_coaches(const World&);
void install_national_coach_account(GameState&,int local,const NationalCoachAccount&);
NationalCoachAccount national_coach_account_from_legacy(const GameState&,int local);
}
