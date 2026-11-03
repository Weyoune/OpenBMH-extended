// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "ui/cup_draw.hpp"
#include "game/world12/save.hpp"
#include <map>
namespace bmh::ui {
struct World12CupDrawView {
    CupDrawProjection projection;
    std::map<uint16_t,bmh::world12::ClubId> clubs;
};
// Projects an already captured real draw notice. This never selects/shuffles a
// participant, changes a programme, or consumes either gameplay random stream.
World12CupDrawView world12_cup_draw_notice(const bmh::world12::CareerSnapshot&,
    const bmh::world12::CupDrawNotice&,const GameState& reference,const LeImage& country_rules,
    const TextDb&);
}
