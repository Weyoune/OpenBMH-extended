// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/national_campaign.hpp"
namespace bmh::world12 {
struct NationalFinalFacts {
    uint32_t season=0;uint8_t competition=0;
    ClubId winner,runner_up;
    std::array<ClubId,4> places;
    bool operator==(const NationalFinalFacts&)const=default;
};
// Read-only Source final facts from stored played scores, never stored award
// labels alone. Requires the installed Source4/6 fixture tables for groups;
// structural World/codec validation deliberately has no asset dependency. Four-nation quota1 deliberately leaves its runner unknown.
NationalFinalFacts national_final_facts(const NationalCampaign&);
}
