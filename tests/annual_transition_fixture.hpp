// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "annual_accounts_fixture.hpp"
#include "game/world12/inactive_national_draw.hpp"
#include "game/calendar.hpp"
namespace bmh::test::world12 {
inline LeImage annual_transition_rules(bool english) {
    const auto cup=bmh::test::world12::country_rules(english);
    auto source=LeImage::from_bytes(layout::CalendarDateSetup,Bytes(cup.base()+cup.size()-layout::CalendarDateSetup));source.patch(cup.base(),cup.bytes().data(),cup.size());
    const uint8_t setup[]={0xc6,0x44,0x24,0x04,2,0xc6,0x44,0x24,0x0c,7,0xc7,0x04,0x24,0xea,7,0,0};source.patch(layout::CalendarDateSetup,setup,sizeof setup);
    const auto word=[&](uint16_t day,uint16_t value){const uint8_t bytes[]={uint8_t(value),uint8_t(value>>8)};source.patch(layout::CompetitionCalendar+day*2,bytes,2);};
    word(0,0x4001);for(uint16_t round=0;round<(english?38:34);++round)word(uint16_t(7+round*8),15);return source;
}
inline CareerSnapshot annual_transition_closing(const GameState& reference,const LeImage& source,uint8_t duration) {
    auto state=bmh::test::world12::completed_account_closing(reference,{false,true,false},true);
    state.world.settings->duration=duration;state.world.highscore_signature=0;
    state.desktop->manager_order={3};state.desktop->matchday_number=61;state.desktop->autosave_month=7;
    state.progress->phase=ProgressPhase::annual_transition;
    // Authored already-owned field. It is a scratch receipt, not played national
    // results; its subsequent exact draw is owned by the real carry kernel.
    initialize_inactive_national_draw(state.world,{0,1,2,3},source);state.validate();return state;
}
}
