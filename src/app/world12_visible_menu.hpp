// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "ui/desk_state.hpp"
#include <algorithm>
namespace bmh::app::world12 {
inline bool visible_world12_menu(const ui::DeskState& desk,int id,int kind) {
    for(const auto& item:desk.items)if(item.id==id&&item.kind==kind&&!item.disabled)return true;
    if(id<1||id>30)return false;
    const auto& row=desk.layout_records[size_t(id)];const size_t folder=row[13]&0x7f;
    if(row[12]!=kind||folder>=3||row[5]!=0xff||row[6]!=0xff||(row[1]&0x10))return false;
    if(std::find(desk.folder_icon_slots[folder].begin(),desk.folder_icon_slots[folder].end(),uint8_t(id))==desk.folder_icon_slots[folder].end())return false;
    return std::any_of(desk.items.begin(),desk.items.end(),[&](const auto& item){return item.kind==50+int(folder)&&!item.disabled;});
}
}
