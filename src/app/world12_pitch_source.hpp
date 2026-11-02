// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "formats/executable_layout.hpp"
#include "game/lineup.hpp"
#include "ui/pitch.hpp"
namespace bmh::app::world12 {
inline void verify_world12_pitch_row(const GameState& local,int club,int row,int player,uint16_t slot){
    if(row<0||row>=24||local.squad_slots.at(squad_range(club).first+size_t(row))!=slot||((slot&0xfff)==0xfff?-1:int(slot&0xfff))!=player)
        throw FormatError("world12: stale pitch player/row");
}
inline void apply_world12_pitch_action(GameState& local,const ui::PitchRequest& r,bool foreign_rule) {
    if(r.indoor!=local.indoor)throw FormatError("world12: stale pitch mode");
    const auto at=size_t(r.manager_slot)*0xb0c;const auto display=layout::CareerSession_AnyOf_ExecutableField+uint32_t(r.manager_slot*2);
    const auto word=[&](size_t p){return uint16_t(local.manager_records.at(at+p)|uint16_t(local.manager_records.at(at+p+1))<<8);};
    const std::array<uint16_t,3> takers{word(0xafc),word(0xafe),word(0xb08)};const auto old=local.memory.u16(display);const auto mode=local.memory.u8(layout::GameMode);
    const bool editor=r.kind==ui::PitchRequestKind::open_set_piece_editor||r.kind==ui::PitchRequestKind::close_set_piece_editor||r.kind==ui::PitchRequestKind::set_piece_taker||r.kind==ui::PitchRequestKind::auto_set_piece_takers;
    if(editor||r.kind==ui::PitchRequestKind::select_pitch_page||r.kind==ui::PitchRequestKind::set_player_order){
        if(r.expected_display_word!=old||r.expected_set_piece_values!=takers||r.expected_set_piece_signature!=word(0xb0a)||r.expected_mode!=mode)throw FormatError("world12: stale pitch page/takers");
    }
    if(editor&&(word(0xb0a)!=0x3253||(r.kind!=ui::PitchRequestKind::open_set_piece_editor&&!(old&0x80))))throw FormatError("world12: inactive set-piece editor");
    bool accepted=false;
    switch(r.kind){
    case ui::PitchRequestKind::move_player:
        accepted=move_pitch_player_source(local,r.manager_slot,r.runtime_club,r.row,r.player_id,r.slot,r.old_depth,r.old_lane,r.new_depth,r.new_lane);break;
    case ui::PitchRequestKind::swap_players:
        verify_world12_pitch_row(local,r.runtime_club,r.row,r.player_id,r.slot);verify_world12_pitch_row(local,r.runtime_club,r.second_row,r.second_player,r.second_slot);
        accepted=swap_pitch_roster_rows_source(local,r.manager_slot,r.runtime_club,r.row,r.second_row,r.slot,r.second_slot,foreign_rule);break;
    case ui::PitchRequestKind::set_automatic_weight:
        accepted=set_pitch_automatic_weight_source(local,r.manager_slot,r.runtime_club,r.weight_index,r.old_weights,r.new_weights);break;
    case ui::PitchRequestKind::apply_formation:
        accepted=apply_pitch_formation_source(local,r.manager_slot,r.runtime_club,r.formation,r.old_weights);break;
    case ui::PitchRequestKind::save_formation:
        accepted=save_pitch_formation_source(local,r.manager_slot,r.runtime_club,r.formation,r.old_weights,r.formation_positions);break;
    case ui::PitchRequestKind::open_set_piece_editor: {
        const auto next=uint16_t((old&0xff00)|((old&0x8f)|0x80));accepted=r.new_display_word==next&&normalize_pitch_set_piece_source(local,r.manager_slot,r.runtime_club);if(accepted)local.memory.set16(display,next);break;}
    case ui::PitchRequestKind::close_set_piece_editor:
        accepted=r.new_display_word==uint16_t(old&~uint16_t(0x80));if(accepted)local.memory.set16(display,r.new_display_word);break;
    case ui::PitchRequestKind::set_piece_taker:
        accepted=r.set_piece_selector>=2&&r.set_piece_selector<=4&&r.expected_set_piece_value==takers[size_t(r.set_piece_selector-2)]&&set_pitch_set_piece_taker_source(local,r.manager_slot,r.runtime_club,r.set_piece_selector,r.row,r.player_id,r.slot,r.expected_set_piece_value);break;
    case ui::PitchRequestKind::auto_set_piece_takers:
        accepted=auto_pitch_set_piece_takers_source(local,r.manager_slot,r.runtime_club,r.expected_set_piece_values);break;
    case ui::PitchRequestKind::set_player_order:
        verify_world12_pitch_row(local,r.runtime_club,r.row,r.player_id,r.slot);
        if(mode==1||(old&0x70)!=0x20||r.row>=11||r.order_aspect<0||r.order_aspect>3||r.new_order_value<2||r.new_order_value>40||r.new_order_value==r.expected_order_value||local.tactics.at(size_t(r.manager_slot)*0x840+size_t(r.row)*0x58+2+size_t(r.order_aspect))!=r.expected_order_value)break;
        accepted=set_player_order_source(local,r.manager_slot,r.runtime_club,r.row,r.player_id,r.order_aspect,r.new_order_value);break;
    case ui::PitchRequestKind::select_pitch_page: {
        uint16_t next=old;
        switch(r.page_control){case 0:next=uint16_t((old&~uint16_t(0x80))^3);break;case 1:next=uint16_t(old^12);break;case 2:next=uint16_t((old&0x8f)|0x10);break;case 3:if(mode!=1)next=uint16_t((old&0x8f)|0x20);else throw FormatError("world12: player orders unavailable in mode I");break;default:throw FormatError("world12: no authenticated opponent for marking page");}
        // Page 2/3 clears editor bit 80, preserving the Original low-word mask.
        if(r.page_control==2||r.page_control==3)next=uint16_t(next&~uint16_t(0x80));
        accepted=r.new_display_word==next;if(accepted)local.memory.set16(display,next);break;}
    default:throw FormatError("world12: unsupported pitch command");
    }
    if(!accepted)throw FormatError("world12: rejected stale or unavailable pitch command");
}
}
