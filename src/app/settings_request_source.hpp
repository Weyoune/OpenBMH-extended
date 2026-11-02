// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/state.hpp"
#include "game/messages.hpp"
#include "ui/settings.hpp"
#include "ui/manager.hpp"
#include <limits>
namespace bmh::app {
// Original byte edits shared by the native and typed career owners. The caller
// authenticates its window and stages publication; this kernel never does I/O.
inline bool apply_settings_request_source(GameState& g,const LeImage& rules,
    std::optional<ui::SettingsSoundCapabilities> sound,bool scoped_settings,
    const ui::SettingsWriteRequest& request,uint64_t& revision,std::string& error) {
    auto& mem = g.memory;
    if (request.manager_slot < 0 || request.manager_slot > 3 ||
        size_t(request.manager_slot) >= g.manager_teams.size() || !mem.has(::bmh::layout::ActiveManager) ||
        int(mem.u8(::bmh::layout::ActiveManager)) != request.manager_slot || request.revision != revision ||
        revision == std::numeric_limits<uint64_t>::max()) {
        error = "Diese Einstellungsaktion gehört zu einem veralteten Manager oder Stand.";
        return false;
    }
    if (request.row >= 20 || request.column >= 9 || !mem.has(::bmh::layout::LeaguePresentationOptions + 179) ||
        !mem.has(::bmh::layout::GameMode) || !mem.has(::bmh::layout::CompetitionSelection)) {
        error = "Der Einstellungszustand ist unvollständig.";
        return false;
    }
    std::array<uint8_t, 180> raw{};
    for (size_t i = 0; i < raw.size(); ++i) {
        const uint32_t address = ::bmh::layout::LeaguePresentationOptions + uint32_t(i);
        if (!mem.has(address)) { error = "Der Einstellungszustand ist unvollständig."; return false; }
        raw[i] = mem.u8(address);
    }
    if (raw != request.expected_raw) {
        error = "Die Einstellungen wurden inzwischen geändert.";
        return false;
    }
    const size_t index = size_t(request.row) * 9 + request.column;
    const uint32_t address = ::bmh::layout::LeaguePresentationOptions + uint32_t(index);
    ui::SettingsAvailabilityInputs inputs{mem.u8(::bmh::layout::GameMode), mem.u32(::bmh::layout::CompetitionSelection),
        uint8_t(sound && !sound->music),
        uint8_t(sound && !sound->effects)};
    auto availability = ui::project_settings_availability(raw, inputs, rules);
    // The host supplies the portable sound driver's capabilities. Headless
    // callers without that provider retain the unknown Source latch boundary.
    if (!sound) { availability[5] = raw[5]; availability[6] = raw[6]; }
    const bool sound_cell = request.row == 0 && (request.column == 5 || request.column == 6);
    if (sound_cell && !sound) {
        error = "Diese Einstellung ist noch nicht verfügbar.";
        return false;
    }
    if (((request.row == 8 && request.column == 7) && !scoped_settings) ||
        (request.column == 7 && request.row >= 14 && request.row <= 19)) {
        error = "Diese Einstellung ist noch nicht verfügbar.";
        return false;
    }
    if ((request.operation != ui::SettingsEdit::toggle_binary &&
         request.operation != ui::SettingsEdit::decrement && request.operation != ui::SettingsEdit::increment) ||
        (request.column < 7 && request.operation != ui::SettingsEdit::toggle_binary) ||
        (request.column >= 7 && request.operation == ui::SettingsEdit::toggle_binary)) {
        error = "Die Aktion passt nicht zum Einstellungsfeld.";
        return false;
    }
    const uint8_t shown = availability[index];
    const uint8_t raw_value = sound_cell && sound ? uint8_t(raw[index] & 0x7f) : raw[index];
    if (request.column < 7 && ((shown & 0x80) || raw_value >= 2)) {
        error = "Dieses Einstellungsfeld ist im aktuellen Modus gesperrt.";
        return false;
    }
    const uint8_t old = raw_value;
    uint8_t next = old;
    if (request.operation == ui::SettingsEdit::toggle_binary) {
        // Inventory of actual current consumers: direct match-engine buttons,
        // matchday/season flags and the live-scene switch. Other cells are not
        // made writable merely because the Original UI displayed a cell.
        // Tie the write whitelist to the concrete consumer ranges. League
        // remapping reads rows 0..4, columns 0..3; matchday display kinds 1/2
        // map to columns 0..2 and rows 0..4 or 17..19.
        const bool consumed = (sound_cell && sound.has_value()) ||
            (request.row == 0 && request.column == 4) || // A5: match/Sportschau sound
            ((request.row <= 4 && request.column <= 3) ||
            ((request.row <= 4 || request.row >= 17) && request.column <= 2)) ||
            ((request.row == 5 || (request.row >= 9 && request.row <= 12)) && request.column == 0) ||
            (request.row == 1 && request.column == 4) || // B5 live-scene option
            (request.row == 2 && request.column == 4) || // C5: Reality player numbers (RealityMatch_FormatError_ExecutableField)
            (request.row == 5 && request.column == 5) || // F6: Source manager-match desk gate (MatchdaySetup_MatchdayNeedsManagerDesks_RuntimeField)
            (request.row == 6 && request.column == 5) || // G6: transfer confirmation wait (OutcomeDelayDisabled)
            (request.row == 16 && request.column == 6) || // Q7: own-club Sportschau preference (MatchdaySetup_ConsiderSportschauFixture_RuntimeField)
            (request.row == 13 && request.column == 6) || // N7: Source kickoff competition list (SportschauAwards_MatchdayIntroView_RuntimeField)
            (request.row == 1 && request.column == 6) || // B7: SportschauIntro intro gate (Controller_FormatError_RuntimeField)
            (request.row == 2 && request.column == 6) || // C7: SportschauIntro intro-animation gate (Controller_FormatError_RuntimeField2)
            (request.row == 3 && request.column == 6) || // D7: selected report progression (ReplayProgressionMode)
            (request.row == 4 && request.column == 6) || // E7: Source-owned Sportschau scenes (ReplayChanceMode)
            (request.row == 5 && request.column == 6) || // F7: SportschauIntro fixture-report gate (Controller_FormatError_RuntimeField3)
            (request.row == 6 && request.column == 6) || // G7: MatchdayIntro player/award rating (SportschauAwards_FinishSportschauAwards_RuntimeField)
            (request.row == 7 && request.column == 6) || // H7: SportschauTeamInfo competition totals (SportschauAwards_SportschauTeamInfoView_RuntimeField)
            (request.row == 2 && request.column == 5) || // C6 drawer-handle behavior (CareerSession_DispatchDesktopLayout_RuntimeField)
            (request.row == 3 && request.column == 5) || // D6 native desktop drag raster (CareerSession_DispatchDesktopLayout_RuntimeField2)
            (request.row == 4 && request.column == 5) || // E6 always-visible shortcut bar (DeskState_FormatError_RuntimeField3)
            (request.row == 7 && request.column == 5) || // H6 transfer option
            (request.row == 8 && (request.column == 5 || request.column == 6)) || // I6/I7
            (request.row >= 10 && request.row <= 13 && request.column == 4) || // K5:N5 match buttons
            (request.row >= 10 && request.row <= 12 && request.column == 6) || // K7:M7 presentation
            (request.row == 9 && request.column == 6) || // J7 season-entrant visibility (SeasonEntrants_SeasonEntrantsFromGame_TextPointer)
            (request.row == 15 && request.column == 4) || // P5
            (request.row == 16 && request.column == 4) || // Q5
            (request.row == 17 && request.column == 4);   // R5: secondary leagues (CalendarState_FormatError_RuntimeField2)
        if (!consumed || (old != 0 && old != 1)) {
            error = "Diese Einstellung ist noch nicht verfügbar.";
            return false;
        }
        if (request.row == 16 && request.column == 4 &&
            g.season_day() >= 110 && g.season_day() <= 230) {
            error = ui::original_to_utf8(message_text(g, ::bmh::layout::CareerSession_DispatchSettings_TextPointer));
            return false;
        }
        // SettingsSourceRefusal only permits re-enabling R5 while CareerSessionRuntimeField4 is zero.
        // LeagueRoundCounters is the saved league_rounds block; CareerSessionRuntimeField4 is its sixth byte.
        // Turning the leagues off remains possible after they have started.
        if (request.row == 17 && request.column == 4 && old != 0 &&
            (g.league_rounds.size() <= 5 || g.league_rounds[5] != 0)) {
            error = ui::original_to_utf8(message_text(g, ::bmh::layout::CareerSession_DispatchSettings_TextPointer2));
            return false;
        }
        next = old ^ 1;
    } else {
        // Column8: existing presentation speeds B8:F8, H8 process budget,
        // J8 postponed limit and K8 autosave. Column9 only exposes values
        // already read by Main's Source live presenters or CupDrawScreen.
        const bool consumed = (request.column == 7 &&
             ((request.row <= 6) || request.row == 7 || request.row == 9 || request.row == 10 ||
             (scoped_settings && request.row == 8))) ||
             // A9 match minutes; B9:C9 chance result; D9:E9 bookings; F9:G9 rolling ball;
             // I9 cup draw; K9 Reality report; M9 preview/shootout; P9 shot.
             (request.column == 8 && ((request.row >= 0 && request.row <= 6) ||
                 request.row == 7 || request.row == 8 || request.row == 10 || request.row == 12 || request.row == 15));
        if (!consumed || old >= 200) {
            error = "Diese Einstellung ist noch nicht verfügbar.";
            return false;
        }
        if (request.column == 8) {
            // SettingsSourceRefusal's ninth column is stored doubled for display. Its
            // Source edit step is two raw units; decrement is enabled only
            // above 1 and increment only below 0xc6 (198). Preserve odd raw
            // values exactly, as the original does.
            if (request.operation == ui::SettingsEdit::decrement) {
                if (old <= 1) { error = "Der Zahlenwert ist bereits am Originalminimum."; return false; }
                next = uint8_t(old - 2);
            } else {
                if (old >= 198) { error = "Der Zahlenwert ist bereits am Originalmaximum."; return false; }
                next = uint8_t(old + 2);
            }
        } else if (request.column == 7) {
            if (request.operation == ui::SettingsEdit::decrement) {
                if (old == 0) { error = "Der Zahlenwert ist bereits am Originalminimum."; return false; }
                next = uint8_t(old - 1);
            } else {
                const uint8_t limit = rules.u8(::bmh::layout::CareerSession_DispatchSettings_RuntimeField + request.row);
                if (old >= limit) { error = "Der Zahlenwert ist bereits am Originalmaximum."; return false; }
                next = uint8_t(old + 1);
            }
        }
    }
    if (next == old) { error = "Die Einstellung hat sich nicht geändert."; return false; }
    mem.set8(address, next);
    ++revision;
    return true;
}

}
