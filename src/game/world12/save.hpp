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
    uint16_t display_word=0;
    std::array<std::array<uint8_t,16>,54> layout{};
    bool operator==(const ManagerDesktop&)const=default;
};
struct PendingDeskOffer {
    uint8_t seat=0,row=40;
    bool salary_only=false,warned=false;
    bool operator==(const PendingDeskOffer&)const=default;
};
struct InitialDeskEvent {
    uint16_t matchday_number=0;
    uint8_t manager_position=0;
    std::vector<uint8_t> manager_order;
    // The initial event has no fixtures. Its Toto coupon is open and empty.
    bool pools_open=true;
    std::optional<PendingDeskOffer> offer;
    bool operator==(const InitialDeskEvent&)const=default;
};
struct DesktopRuntime {
    std::vector<ManagerDesktop> managers;
    // The next shuffle starts from the preceding event's order, not seat order.
    // Empty means an older snapshot did not preserve this owner information.
    std::vector<uint8_t> manager_order;
    std::optional<uint16_t> matchday_number;
    std::optional<uint8_t> active_seat;
    bool autosave_pending=false;
    uint16_t autosave_month=0,autosave_months=0;
    std::optional<InitialDeskEvent> initial_event;
    bool operator==(const DesktopRuntime&)const=default;
};
struct TransferHistoryRow {
    PlayerId player;ClubId other;uint32_t source_word=0;
    bool operator==(const TransferHistoryRow&)const=default;
};
struct LoanContract {
    PlayerId player;uint8_t manager=0;
    std::array<uint8_t,0x58> board{};
    PlayerId mark,marked_by;
    std::optional<uint8_t> source_slot={};
    bool operator==(const LoanContract&)const=default;
};
struct TransferRuntime {
    std::array<int32_t,4> balance{};
    std::array<std::vector<TransferHistoryRow>,4> history;
    std::vector<LoanContract> loans;
    bool operator==(const TransferRuntime&)const=default;
};
enum class ProgressPhase:uint8_t { daily_services,post_event_market,inspect_calendar,scheduled_event,season_end };
enum class DayEntry:uint8_t { after_event,plain_day };
struct CareerProgress {
    ProgressPhase phase=ProgressPhase::daily_services;
    DayEntry entry=DayEntry::after_event;
    uint32_t previous_date=0,season_start=0;
    uint16_t epoch_year=0;
    uint8_t epoch_weekday=0,last_manager=0;
    bool operator==(const CareerProgress&)const=default;
};
struct PreparedFixture {
    Fixture fixture;
    int8_t temperature=0;
    uint8_t rain=0,referee=0;
    Country referee_country=Country::germany;
    bool operator==(const PreparedFixture&)const=default;
};
struct PreparedGroup {
    std::optional<LeagueId> league;
    std::optional<DomesticCup> cup;
    uint8_t source_entry=0,round=0,leg=0,flags=2;
    uint64_t revision=0;
    std::vector<PreparedFixture> matches;
    bool operator==(const PreparedGroup&)const=default;
};
struct PreparedEvent {
    uint16_t day=0;
    uint64_t calendar_revision=0;
    std::array<uint8_t,50> referee_properties{};
    std::vector<PreparedGroup> groups;
    bool operator==(const PreparedEvent&)const=default;
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
    std::optional<TransferRuntime> transfers;
    std::optional<CareerProgress> progress;
    std::optional<PreparedEvent> prepared_event;
    // Typed active career snapshots will extend this record as their owning
    // progression transactions are integrated. No legacy FBM stream is guessed.
    void validate()const;
    bool operator==(const CareerSnapshot&)const=default;
};
Savegame make_save(const CareerSnapshot&);
CareerSnapshot read_save(const Savegame&);
}
