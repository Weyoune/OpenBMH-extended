// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/save.hpp"
#include "game/state.hpp"
#include "formats/textd.hpp"
namespace bmh::world12 {
struct NationalAnnualPlayers {
    CareerSnapshot source_binding;
    World world;
    int32_t game_seed=0;uint32_t watcom_seed=0;
    uint64_t game_draws=0,watcom_draws=0;
    size_t visited=0,released=0;
};
// All actual retained origins/free pools, once per canonical player. No hidden
// human club, club finance, advertising, manager board or nominee replacement
// is manufactured. Target remains unpublished; accounts/nominees are retained.
NationalAnnualPlayers stage_national_annual_players(const CareerSnapshot&,uint32_t target_season,
    const GameState& reference,const TextDb&);
}
