// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "ui/manager.hpp"
#include "game/world12/world.hpp"
namespace bmh::ui {
SelectionContext world12_manager_selection_context(const bmh::world12::World&,uint64_t revision,bool joining=false);
}
