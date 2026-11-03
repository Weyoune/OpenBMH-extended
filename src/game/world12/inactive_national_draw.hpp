// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/national_campaign.hpp"
namespace bmh::world12 {
struct InactiveNationalDraw {
    uint32_t season=0;
    uint16_t native_national_first=0,native_first_small=0;
    std::vector<uint16_t> field_tokens;
    std::array<uint16_t,2> four_seed_tokens{},six_seed_tokens{};
    bool operator==(const InactiveNationalDraw&)const=default;
};
// Exact UNSIMULATED Native words, normalized modulo16bits by its national base.
// These may contain opaque club/scratch tokens. They are never global ClubIds,
// played national results, winners, or admission to a real competition.
void initialize_inactive_national_draw(World&,const std::vector<uint16_t>& already_drawn_four,const LeImage&);
NationalDrawOutcome advance_inactive_national_draw(World&,uint32_t target_season,GameRandom&,WatcomRandom&);
void validate_inactive_national_draw(const World&);
}
