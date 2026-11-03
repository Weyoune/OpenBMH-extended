// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/world.hpp"
#include "game/state.hpp"
namespace bmh::world12 {
// One actual retained origin and/or actual free pools. It is a numeric Source
// roster, with no active league/fixtures/club-minimum assertion and no manager.
// Valid inactive source rosters must not be padded to gameplay squad minima.
struct InactiveRosterProjection {
    GameState state;
    std::optional<ClubId> club;
    std::vector<PlayerId> players;
    static InactiveRosterProjection create(const World&,std::optional<ClubId>,const GameState&,bool include_pools);
    uint16_t local_player(PlayerId)const;
    PlayerId global_player(uint16_t)const;
    void capture(World&,bool capture_pools)const;
};
std::vector<ClubId> inactive_origin_order(const World&);
const NationalStartupReceipt& inactive_population_receipt(const World&);
}
