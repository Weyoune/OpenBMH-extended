// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/fixtures.hpp"
#include "game/world12/calendar.hpp"
#include "game/world12/cups.hpp"
#include <optional>
#include "formats/fbm.hpp"
namespace bmh::world12 {
struct CalendarCursor {SeasonCalendar schedule;uint16_t next_day=0;uint64_t revision=0;bool operator==(const CalendarCursor&)const=default;};
struct ManagerDesktop {
    uint8_t seat=0,prestige=0;
    std::array<std::array<uint8_t,16>,54> layout{};
    bool operator==(const ManagerDesktop&)const=default;
};
struct DesktopRuntime {
    std::vector<ManagerDesktop> managers;
    std::optional<uint8_t> active_seat;
    bool autosave_pending=false;
    uint16_t autosave_month=0,autosave_months=0;
    bool operator==(const DesktopRuntime&)const=default;
};
struct CareerSnapshot {
    World world;
    SeasonResults results;
    uint32_t date=0,season=0,watcom_seed=1;
    int32_t game_seed=0;
    std::array<uint8_t,kLeagueSlots> league_rounds{};
    std::optional<CalendarCursor> calendar;
    std::vector<KnockoutStage> cups;
    std::optional<DesktopRuntime> desktop;
    // Typed active career snapshots will extend this record as their owning
    // progression transactions are integrated. No legacy FBM stream is guessed.
    void validate()const;
    bool operator==(const CareerSnapshot&)const=default;
};
Savegame make_save(const CareerSnapshot&);
CareerSnapshot read_save(const Savegame&);
}
