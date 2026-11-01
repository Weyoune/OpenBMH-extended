// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "ui/desk_state.hpp"
#include "game/world12/save.hpp"
namespace bmh::ui {
// Read-only initial desktop. Local rendering indices never replace global IDs.
DeskState world12_initial_desktop(const std::filesystem::path&,const bmh::world12::CareerSnapshot&,
    const GameState& reference,const LeImage& rules,SetupOptions,uint64_t revision);
}
