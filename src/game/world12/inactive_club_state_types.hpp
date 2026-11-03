// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/identity.hpp"
#include <array>
#include <cstdint>
#include <vector>
namespace bmh::world12 {
// Numerical setup fields only. These are not played club competitions, title
// awards, completed standings or selectable manager clubs.
struct InactiveDomesticField {
    uint8_t source_entry=0;
    std::vector<ClubId> teams,seeds;
    bool operator==(const InactiveDomesticField&)const=default;
};
struct InactiveClubState {
    uint32_t season=0;
    Country primary=Country::germany;
    uint16_t primary_cup_runner_token=0;
    ClubId primary_cup_seed;
    std::array<ClubId,3> holder_seeds;
    std::array<std::vector<ClubId>,3> qualified,fields;
    std::array<std::vector<ClubId>,27> reserve_pools;
    std::vector<InactiveDomesticField> domestic_fields;
    bool operator==(const InactiveClubState&)const=default;
};
}
