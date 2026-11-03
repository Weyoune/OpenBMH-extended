// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/state.hpp"
#include "game/national_identity.hpp"
#include "ui/desktop.hpp"
#include <algorithm>
namespace bmh::app {
inline bool apply_desk_setting_source(GameState& g,int manager,bool price,int index,int value,std::string& error) {
    const size_t rec = size_t(manager) * 0xb0c;
    const bool national = is_national_team(g,g.manager_teams[size_t(manager)]);
    if (price) {
        // Uint8T4: local offsets35..86 map to admission values8..25.
        if (national || value < 8 || value > 25) { error = "invalid_ticket_price"; return false; }
        g.manager_records[rec + 0x26e] = uint8_t(value);
    } else {
        if (index < 0 || index > 4) { error = "invalid_automation_index"; return false; }
        const size_t at = size_t(manager) * 5 + size_t(index);
        if (index == 0) {
            if (g.manager_options[at] > 8 || value < 0 || value > 8) {
                error = "invalid_saved_formation_automation"; return false;
            }
            // SizeT6 uses the saved outdoor/indoor formation availability.
            const uint8_t mask = g.manager_records[rec + 0x218 + size_t(g.indoor)];
            if (value != 0 && !(mask & (1u << (value - 1)))) {
                error = "unavailable_saved_formation"; return false;
            }
            g.manager_options[at] = uint8_t(value);
        } else {
            if (index == 4 && national) { error = "national_team_training_unavailable"; return false; }
            if (value != 0 && value != 1) { error = "invalid_automation_value"; return false; }
            // Explicit bool setters and SDL toggles preserve every other bit.
            g.manager_options[at] = uint8_t((g.manager_options[at] & ~uint8_t(1)) | uint8_t(value));
        }
    }
    error.clear();
    return true;
}
template<class Projection>
bool apply_desktop_layout_source(GameState& g,const ui::DeskRequest& request,uint64_t& revision,std::string& error,Projection project) {
    constexpr size_t kItems=54,kStride=16,kBytes=kItems*kStride;
    if(request.kind!=ui::DeskRequestKind::change_layout||request.revision!=revision||revision==UINT64_MAX||request.manager_slot<0||size_t(request.manager_slot)>=g.manager_teams.size()||!g.memory.has(layout::ActiveManager)||g.memory.u8(layout::ActiveManager)!=request.manager_slot){error="stale_or_invalid_desk_layout_owner";return false;}
    if (request.item_id < 1 || request.item_id >= int(kItems)) { error="invalid_desk_layout_item"; return false; }
    const uint32_t base=::bmh::layout::DesktopsUnsigned+uint32_t(request.manager_slot)*0x360u;
    if (!g.memory.has(base+kBytes-1)) { error="desk_layout_memory_unavailable"; return false; }
    std::array<uint8_t,kBytes> actual{};
    for(size_t i=0;i<kBytes;++i) actual[i]=g.memory.u8(base+uint32_t(i));
    if(actual!=request.layout_before) { error="stale_desk_layout_before_state"; return false; }
    auto expected=request.layout_before;
    const size_t at=size_t(request.item_id)*kStride;
    auto read16=[](const auto& bytes,size_t offset){return uint16_t(bytes[offset])|(uint16_t(bytes[offset+1])<<8);};
    auto write16=[](auto& bytes,size_t offset,uint16_t value){bytes[offset]=uint8_t(value);bytes[offset+1]=uint8_t(value>>8);};
    const auto moved_x=read16(request.layout_after,at);
    const uint8_t moved_y=request.layout_after[at+2];
    if(request.menu_kind!=expected[at+12]) { error="desk_layout_item_kind_mismatch"; return false; }
    const bool source_drag_enabled=g.memory.has(::bmh::layout::CareerSession_DispatchDesktopLayout_RuntimeField)&&g.memory.u8(::bmh::layout::CareerSession_DispatchDesktopLayout_RuntimeField)==0;
    if((request.layout_operation==ui::DeskLayoutOperation::move_item||
        request.layout_operation==ui::DeskLayoutOperation::set_folder||
        request.layout_operation==ui::DeskLayoutOperation::return_folder_child||
        request.layout_operation==ui::DeskLayoutOperation::send_to_folder||
        request.layout_operation==ui::DeskLayoutOperation::restore_folder_child)&&!source_drag_enabled) {
        error="source_desk_drag_disabled";return false;
    }
    switch(request.layout_operation) {
    case ui::DeskLayoutOperation::move_item: {
        if(request.layout_target_item!=-1 || expected[at+12]==72) { error="invalid_desk_layout_move_target"; return false; }
        for(size_t i=0;i<kBytes;++i) if(i!=at&&i!=at+1&&i!=at+2&&request.layout_after[i]!=expected[i]) {
            error="desk_layout_move_changed_unowned_bytes"; return false;
        }
        const int source_x=int(moved_x&0xefff), max_x=std::max(1,319-int(expected[at+3]));
        const int max_y=std::max(1,239-int(expected[at+4]));
        const bool snap=g.memory.has(::bmh::layout::CareerSession_DispatchDesktopLayout_RuntimeField2) && g.memory.u8(::bmh::layout::CareerSession_DispatchDesktopLayout_RuntimeField2)==0;
        if((moved_x&0x1000)!=(read16(expected,at)&0x1000) || source_x<1 || source_x>max_x ||
           moved_y<1 || moved_y>max_y ||
           (snap && ((source_x%5)!=0 && source_x!=max_x || (moved_y%5)!=0 && moved_y!=max_y))) {
            error="desk_layout_move_outside_source_bounds"; return false;
        }
        write16(expected,at,moved_x); expected[at+2]=moved_y;
        break;
    }
    case ui::DeskLayoutOperation::toggle_quickbar_drawer: {
        const int id=request.item_id;
        const int kind=expected[at+12];
        if(request.layout_target_item!=id || id<1 || id>=54 || kind<50 || kind>71 ||
           kind!=request.menu_kind || (request.value!=2 && request.value!=8)) {
            error="invalid_desk_quickbar_drawer"; return false;
        }
        if(expected[at+5]==0xff) {
            int tail=0;
            size_t guard=0;
            while(expected[size_t(tail)*kStride+5]!=0) {
                if(++guard>=kItems) { error="invalid_desk_layout_chain"; return false; }
                tail=expected[size_t(tail)*kStride+5];
                if(tail<0 || tail>=int(kItems)) { error="invalid_desk_layout_links"; return false; }
            }
            expected[at+5]=0; expected[at+6]=uint8_t(tail);
            expected[size_t(tail)*kStride+5]=uint8_t(id);
            const bool opened=!(request.value==8 && id>2);
            expected[at+13]=uint8_t((expected[at+13]&0x7f)|(opened?0x80:0));
        } else {
            const uint8_t previous=expected[at+6],next=expected[at+5];
            if(previous>=kItems || (next!=0 && next>=kItems)) { error="invalid_desk_layout_links"; return false; }
            expected[size_t(previous)*kStride+5]=next;
            if(next!=0) expected[size_t(next)*kStride+6]=previous;
            expected[at+5]=expected[at+6]=0xff;
        }
        break;
    }
    case ui::DeskLayoutOperation::return_folder_child: {
        const int folder=request.layout_target_item;
        if(request.item_id<1||request.item_id>30||folder<0||folder>=3||
           expected[at+5]!=0xff||expected[at+6]!=0xff||(expected[at+1]&0x10)!=0||
           (expected[at+13]&0x7f)!=folder||expected[at+12]>=50) {
            error="invalid_desk_folder_child_return"; return false;
        }
        bool owner_visible=false;
        for(size_t item=1;item<kItems;++item) {
            // The frame bit does not hide folder contents; require a visible linked owner.
            if(expected[item*kStride+12]!=50+folder||(expected[item*kStride+1]&0x10)!=0) continue;
            int cursor=0;size_t visited=0;
            while(++visited<kItems) {
                const int next=expected[size_t(cursor)*kStride+5];
                if(next==int(item)) { owner_visible=true;break; }
                if(next<=0||next>=int(kItems)) break;
                cursor=next;
            }
            if(owner_visible) break;
        }
        if(!owner_visible) { error="desk_folder_child_owner_hidden"; return false; }
        const int max_x=std::max(1,319-int(expected[at+3])),max_y=std::max(1,239-int(expected[at+4]));
        const int source_x=int(moved_x&0xefff);
        const bool snap=g.memory.has(::bmh::layout::CareerSession_DispatchDesktopLayout_RuntimeField2)&&g.memory.u8(::bmh::layout::CareerSession_DispatchDesktopLayout_RuntimeField2)==0;
        if((moved_x&0x1000)!=(read16(expected,at)&0x1000)||source_x<1||source_x>max_x||moved_y<1||moved_y>max_y||
           (snap&&(((source_x%5)!=0&&source_x!=max_x)||((moved_y%5)!=0&&moved_y!=max_y)))) {
            error="desk_folder_child_return_outside_source_bounds";return false;
        }
        int tail=0;size_t guard=0;
        while(expected[size_t(tail)*kStride+5]!=0) {
            if(++guard>=kItems) { error="invalid_desk_layout_chain";return false; }
            tail=expected[size_t(tail)*kStride+5];
            if(tail<0||tail>=int(kItems)) { error="invalid_desk_layout_links";return false; }
        }
        expected[at]=request.layout_after[at];expected[at+1]=request.layout_after[at+1];expected[at+2]=moved_y;
        expected[at+5]=0;expected[at+6]=uint8_t(tail);
        expected[size_t(tail)*kStride+5]=uint8_t(request.item_id);
        break;
    }
    case ui::DeskLayoutOperation::send_to_folder: {
        const int folder=request.layout_target_item;
        const uint8_t previous=expected[at+6],next=expected[at+5];
        if(request.item_id<1||request.item_id>30||folder<0||folder>=3||
           expected[at+5]==0xff||expected[at+12]>=50||expected[at+12]==12||
           (expected[at+13]&0x7f)!=folder||previous>=kItems||(next!=0&&next>=kItems)||
           expected[size_t(previous)*kStride+5]!=request.item_id||
           (next!=0&&expected[size_t(next)*kStride+6]!=request.item_id)) {
            error="invalid_desk_bin_send";return false;
        }
        expected[size_t(previous)*kStride+5]=next;
        if(next!=0)expected[size_t(next)*kStride+6]=previous;
        expected[at+5]=expected[at+6]=0xff;
        break;
    }
    case ui::DeskLayoutOperation::restore_folder_child: {
        const int folder=request.layout_target_item;
        if(request.item_id<1||request.item_id>30||folder<0||folder>=3||
           expected[at+5]!=0xff||expected[at+6]!=0xff||(expected[at+1]&0x10)!=0||
           (expected[at+13]&0x7f)!=folder||expected[at+12]>=50) {
            error="invalid_desk_bin_restore";return false;
        }
        bool owner_visible=false;
        for(size_t item=1;item<kItems;++item) {
            // The frame bit does not hide folder contents; require a visible linked owner.
            if(expected[item*kStride+12]!=50+folder||(expected[item*kStride+1]&0x10)!=0) continue;
            int cursor=0;size_t visited=0;
            while(++visited<kItems) {
                const int next=expected[size_t(cursor)*kStride+5];
                if(next==int(item)) { owner_visible=true;break; }
                if(next<=0||next>=int(kItems)) break;
                cursor=next;
            }
            if(owner_visible)break;
        }
        if(!owner_visible) { error="desk_bin_restore_folder_hidden";return false; }
        int tail=0;size_t guard=0;
        while(expected[size_t(tail)*kStride+5]!=0) {
            if(++guard>=kItems) { error="invalid_desk_layout_chain";return false; }
            tail=expected[size_t(tail)*kStride+5];
            if(tail<0||tail>=int(kItems)) { error="invalid_desk_layout_links";return false; }
        }
        expected[at+5]=0;expected[at+6]=uint8_t(tail);
        expected[size_t(tail)*kStride+5]=uint8_t(request.item_id);
        break;
    }
    case ui::DeskLayoutOperation::set_folder: {
        const int target=request.layout_target_item;
        const int kind=expected[at+12];
        if(kind>=50 || request.item_id>30 || (expected[at+1]&0x10)!=0 || target<1 || target>=int(kItems) || expected[size_t(target)*kStride+12]<50 ||
           expected[size_t(target)*kStride+12]>52) { error="invalid_desk_folder_drop"; return false; }
        const size_t target_at=size_t(target)*kStride;
        const uint8_t folder=uint8_t(expected[target_at+12]-50);
        int folder_count=0;
        for(int child=1;child<=30;++child) {
            const size_t child_at=size_t(child)*kStride;
            if(expected[child_at+5]==0xff && (expected[child_at+1]&0x10)==0 &&
               (expected[child_at+13]&0x7f)==folder) ++folder_count;
        }
        const bool already_in_folder=expected[at+5]==0xff&&(expected[at+13]&0x7f)==folder;
        if(folder_count-(already_in_folder?1:0)>=12) { error="desk_folder_source_capacity"; return false; }
        const int previous=expected[at+6], next=expected[at+5];
        if((previous!=0xff && previous>=int(kItems)) || (next!=0xff && next>=kItems)) { error="invalid_desk_layout_links"; return false; }
        if(previous!=0xff) expected[size_t(previous)*kStride+5]=uint8_t(next);
        if(next>0 && next<0xff) expected[size_t(next)*kStride+6]=uint8_t(previous);
        expected[at+13]=uint8_t((expected[at+13]&0x80)|folder);
        expected[at+5]=expected[at+6]=0xff;
        write16(expected,at,moved_x); expected[at+2]=moved_y;
        break;
    }
    case ui::DeskLayoutOperation::none:
        error="unsupported_desk_layout_operation"; return false;
    }
    if(expected!=request.layout_after) { error="desk_layout_after_state_not_source_derived"; return false; }
    GameState staged=g;
    for(size_t i=0;i<kBytes;++i) staged.memory.set8(base+uint32_t(i),expected[i]);
    try {
        const auto projected=project(staged,revision+1);
        if(projected.manager_slot!=request.manager_slot) { error="desk_layout_projection_mismatch"; return false; }
        for(size_t item=0;item<kItems;++item)
            if(!std::equal(projected.layout_records[item].begin(),projected.layout_records[item].end(),expected.begin()+item*kStride)) {
                error="desk_layout_projection_mismatch"; return false;
            }
    } catch(const std::exception&) { error="desk_layout_projection_invalid"; return false; }
    g=std::move(staged); ++revision; error.clear(); return true;

}
inline bool apply_desk_widget_source(GameState& g,const ui::DeskState& desk,const ui::DeskRequest& request,uint64_t& revision,std::string& error) {
    const int active=g.memory.has(layout::ActiveManager)?int(g.memory.u8(layout::ActiveManager)):-1;
    if(active<0||active>3||request.manager_slot!=active||request.revision!=revision||revision==UINT64_MAX||desk.manager_slot!=active||desk.revision!=revision||size_t(active)>=g.manager_teams.size()||size_t(active+1)*5>g.manager_options.size()||size_t(active+1)*0xb0c>g.manager_records.size()){error="stale_manager_or_revision";return false;}
    const bool price=request.kind==ui::DeskRequestKind::change_ticket_price;
    if(!price&&request.kind!=ui::DeskRequestKind::change_option){error="unsupported_desk_widget";return false;}
    const int expected_kind=price?55:64;
    const auto item=std::find_if(desk.items.begin(),desk.items.end(),[&](const auto& item){return item.id==request.item_id&&item.kind==expected_kind&&!item.disabled;});
    if(request.menu_kind!=expected_kind||item==desk.items.end()){error="stale_or_closed_desk_widget";return false;}
    int value=request.value;
    if(!price){
        if(request.value<0||request.value>4){error="invalid_automation_index";return false;}
        const size_t at=size_t(active)*5+size_t(request.value);
        if(request.value==0){
            if(g.manager_options[at]>8){error="invalid_saved_formation_automation";return false;}
            value=g.manager_options[at];const uint8_t mask=g.manager_records[size_t(active)*0xb0c+0x218+size_t(g.indoor)];
            do{value=(value+1)%9;}while(value!=0&&!(mask&(1u<<(value-1))));
        }else value=(g.manager_options[at]^1)&1;
    }
    if(!apply_desk_setting_source(g,active,price,request.value,value,error))return false;
    ++revision;return true;
}
}
