// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "ui/world12_table.hpp"
namespace bmh::app::world12 {
struct RuntimeResources;
// Immutable Original-family projection, including snapshots outside a live desk.
// It grants no window/action authority; Session owns publication and requests.
ui::World12TableState project_league_family(const bmh::world12::CareerSnapshot&,
    const RuntimeResources&,const ui::World12TableIdentity&,const ui::World12TableSelection&);
}
