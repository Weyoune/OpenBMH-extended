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
        // The original edits every unlocked switch: the page only stores the value and each
        // consumer reads it where it needs it. A switch without a consumer in OpenBMH yet is still
        // stored (and saved) and never refused. Locks (bit 7 of the entry-time availability) and
        // values other than 0/1 are refused above and below.
        if (old != 0 && old != 1) {
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
        // The numeric columns are stored the same way; values from 200 up are blank cells.
        if (old >= 200) {
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
