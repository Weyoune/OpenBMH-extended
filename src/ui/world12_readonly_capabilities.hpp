// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/save.hpp"
#include "ui/desk_state.hpp"
namespace bmh::ui {
// Presentation capabilities only: never copy these masks back into saved MEN.
inline void world12_readonly_capabilities(const bmh::world12::CareerSnapshot& snapshot,DeskState& desk){
    if(desk.manager_slot<0||size_t(desk.manager_slot)>=desk.managers.size())throw FormatError("world12: capability view has no rendered actor");
    const auto team=desk.managers[size_t(desk.manager_slot)].world_club;
    if(snapshot.world.clubs.at(team).kind!=bmh::world12::ClubKind::national_team)return;
    const auto disabled=[&](int kind){return kind==0||kind==1||kind==2||kind==4||kind==20||kind==25||(kind==9&&!snapshot.world.catalogue.scope.germany)||(kind==26&&!snapshot.world.catalogue.scope.club_play()&&(!snapshot.world.highscore_signature||*snapshot.world.highscore_signature!=0));};
    for(auto& item:desk.items)if(disabled(item.kind))item.disabled=true;
    for(auto& row:desk.layout_records)if(disabled(row[12]))row[1]|=0x10;
}
}
