// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/save.hpp"
#include "game/world12/standings.hpp"
#include "ui/league_table.hpp"
namespace bmh::ui {
struct World12TableSelection {
    bmh::world12::LeagueId league;
    bmh::world12::TableGroup group=bmh::world12::TableGroup::germany;
    uint16_t word=0x11;
    int first_round=0,end_round=1,aggregation=7;
    uint8_t normal_mode=1,child=0,pairings_preferences=0;
    uint16_t strengths_word=0x1ff;
    LeaguePairingsOwner pairings;
    std::optional<StrengthsAgeScratch> age;
    // Presentation-only schedule for an unpublished in-progress match overlay.
    std::optional<bmh::world12::CalendarCursor> display_calendar;
    StrengthsAgeBasis age_basis=StrengthsAgeBasis::entry_or_league_change_bit10;
    bool operator==(const World12TableSelection&)const=default;
};
World12TableState world12_table_state(const bmh::world12::CareerSnapshot&,const World12TableIdentity&,
    const World12TableSelection&,const LeImage&,const TextDb&);
World12TableSelection initial_world12_table_selection(const bmh::world12::CareerSnapshot&,uint8_t seat,uint16_t word,uint8_t normal);
void normalize_world12_table_range(World12TableSelection&,const bmh::world12::CareerSnapshot&);
void apply_world12_table_control(World12TableSelection&,int control,const bmh::world12::Catalogue&);
}
