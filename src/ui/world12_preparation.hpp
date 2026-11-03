// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "ui/preparation.hpp"
#include "game/world12/save.hpp"
namespace bmh::ui {
PreparationState world12_preparation(const std::filesystem::path&,
    const bmh::world12::CareerSnapshot&,const GameState&,const LeImage&,SetupOptions,uint64_t revision,const LeImage* english_rules=nullptr);
}
