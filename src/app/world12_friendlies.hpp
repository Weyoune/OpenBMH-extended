// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/friendly_booking.hpp"
#include "ui/world12_friendlies.hpp"
#include "ui/friendly_match.hpp"
namespace bmh::app::world12 {
struct FriendlyMenuOwner {
    uint8_t actor=0;uint64_t sequence=0;bool national=false;
    bmh::world12::ClubId own;
    ui::World12TableSelection selection;
    std::optional<bmh::world12::ClubId> selected;
    std::optional<bmh::world12::FriendlyOffer> offer;
    std::optional<uint32_t> quoted_watcom_seed;
};
}
