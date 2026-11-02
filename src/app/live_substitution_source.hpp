// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <algorithm>
#include <functional>
#include "game/state.hpp"
#include "game/lineup.hpp"
#include "ui/lineup.hpp"
namespace bmh::app {
// Shared Source row eligibility/mutation. The session owns current-match,
// manager/revision and paused-scope checks; this kernel draws no randomness.
inline bool apply_live_substitution_source(GameState& state,const ui::LineupRequest& request,
    int minute,int max_substitutions,bool foreign_rule,int foreign_limit_count,
    const std::function<bool(int)>& is_foreign,std::string& error) {
    error.clear();const int active=request.manager_slot,team=request.runtime_club;
    if(request.kind!=ui::LineupRequestKind::swap_players||active<0||active>3||
       size_t(active)>=state.manager_teams.size()||team<0||state.manager_teams[size_t(active)]!=team||
       size_t(team)>=state.team_flags.size()||state.manager_of(team)!=active||minute<0||minute>125||
       max_substitutions<0||max_substitutions>7||foreign_limit_count<0||foreign_limit_count>11||
       (foreign_rule&&!is_foreign)) {error="Der Eingriff hat keinen gültigen Kaderbesitzer.";return false;}
    const auto range = squad_range(team);
    const bool first_field = request.first_row >= 0 && request.first_row < 11;
    const bool second_field = request.second_row >= 0 && request.second_row < 11;
    const bool same_zone_exchange = first_field == second_field;
    if (request.first_row < 0 || request.second_row < 0 ||
        size_t(request.first_row) >= range.count || size_t(request.second_row) >= range.count ||
        request.first_row == request.second_row ||
        range.first + range.count > state.squad_slots.size()) {
        error = "Die Spielerzeilen sind für den Live-Tausch ungültig.";
        return false;
    }
    const uint16_t outgoing = state.squad_slots[range.first + size_t(request.first_row)];
    const uint16_t incoming = state.squad_slots[range.first + size_t(request.second_row)];
    const uint16_t same_zone_role = first_field ? 0x8000 : 0x4000;
    if (outgoing != request.first_slot || incoming != request.second_slot ||
        (outgoing & 0x0fff) != request.first_player ||
        (incoming & 0x0fff) != request.second_player ||
        (same_zone_exchange ? ((outgoing & 0xc000) != same_zone_role || (incoming & 0xc000) != same_zone_role) :
            (first_field ? (!(outgoing & 0x8000) || !(incoming & 0x4000)) :
                           (!(incoming & 0x8000) || !(outgoing & 0x4000)))) ||
        request.first_player < 0 || request.second_player < 0) {
        error = "Die Kaderauswahl hat sich inzwischen geändert.";
        return false;
    }
    if (size_t(request.first_player) >= state.players.size() || size_t(request.second_player) >= state.players.size()) {
        error = "Die Spielerkennung liegt außerhalb des Kaders.";
        return false;
    }
    const Player& first_player = state.players.at(size_t(request.first_player));
    const Player& second_player = state.players.at(size_t(request.second_player));
    const auto injury_status = [](const Player& p) { return int(p.w364 >> 16 & 0x1f); };
    // PitchScreen checks both selected players before it distinguishes same-zone
    // tactical exchanges from field/bench substitutions.
    if (((first_player.w35c >> 27) == 0 && injury_status(first_player) != 0) ||
        ((second_player.w35c >> 27) == 0 && injury_status(second_player) != 0)) {
        error = "source_notice:0xaf13c";
        return false;
    }
    if (!same_zone_exchange) {
        const int field_row = first_field ? request.first_row : request.second_row;
        const int bench_row = first_field ? request.second_row : request.first_row;
        const int field_player = first_field ? request.first_player : request.second_player;
        const int bench_player = first_field ? request.second_player : request.first_player;
        const Player& field = state.players.at(size_t(field_player));
        const Player& bench = state.players.at(size_t(bench_player));
        const int field_status = int(field.w364 >> 16 & 0x1f);
        const int bench_status = int(bench.w364 >> 16 & 0x1f);
        if (field_status != 0 && (field.w35c >> 27) == 0) {
            error = "source_notice:0xaf140";
            return false;
        }
        if (bench_status != 0) {
            error = "source_notice:0xaf144";
            return false;
        }
        const int used = int(state.team_flags.at(size_t(team)) >> 5 & 7);
        if (used >= max_substitutions) {
            error = "source_notice:0xaf150";
            return false;
        }
        const bool foreign_limit=foreign_rule;
        if (foreign_limit) {
            int foreign_on_field = 0;
            for (size_t row = 0; row < range.count; ++row) {
                const uint16_t slot = state.squad_slots.at(range.first + row);
                if (!(slot & 0x8000) || (slot & 0x0fff) == 0x0fff) continue;
                if (is_foreign(slot & 0x0fff)) ++foreign_on_field;
            }
            foreign_on_field -= is_foreign(field_player);
            foreign_on_field += is_foreign(bench_player);
            if (foreign_on_field > foreign_limit_count) {
                error = "source_notice:0xaf148";
                return false;
            }
        }
        if (bench.w364 & 0x1000) {
            error = "source_notice:0xaf14c";
            return false;
        }
    }

    // Source updates the squad before leaving the child menu; BuildSideRecords3 belongs
    // to the single parent-menu completion callback.
    GameState staged_state = state;
    if (same_zone_exchange) {
        const size_t first = range.first + size_t(request.first_row);
        const size_t second = range.first + size_t(request.second_row);
        const size_t board = size_t(active) * 0x840;
        if (board + (size_t(std::max(request.first_row, request.second_row)) + 1) * 0x58 >
            staged_state.tactics.size()) {
            error = "Die taktischen Spielerdaten sind unvollständig.";
            return false;
        }
        auto* a = staged_state.tactics.data() + board + size_t(request.first_row) * 0x58;
        auto* b = staged_state.tactics.data() + board + size_t(request.second_row) * 0x58;
        const uint8_t ax = a[0x41], ay = a[0x42], bx = b[0x41], by = b[0x42];
        staged_state.squad_slots[first] = uint16_t((incoming & 0x1fff) | (outgoing & 0xc000));
        staged_state.squad_slots[second] = uint16_t((outgoing & 0x1fff) | (incoming & 0xc000));
        std::swap_ranges(a, a + 0x58, b);
        a[0x41] = ax; a[0x42] = ay; b[0x41] = bx; b[0x42] = by;
        // GatedByParamLive (the desk-card remap) is gated by param_2==0; a live
        // marked exchange changes the selected rows but leaves player_cards alone.
    } else {
        const int field_row = first_field ? request.first_row : request.second_row;
        const int bench_row = first_field ? request.second_row : request.first_row;
        const auto staged_range = squad_range(team);
        const size_t field_at = staged_range.first + size_t(field_row);
        const size_t bench_at = staged_range.first + size_t(bench_row);
        const uint16_t field_slot = staged_state.squad_slots.at(field_at);
        const uint16_t bench_slot = staged_state.squad_slots.at(bench_at);
        const int used = int(staged_state.team_flags.at(size_t(team)) >> 5 & 7);
        if (!(field_slot & 0x8000) || !(bench_slot & 0x4000) ||
            (bench_slot & 0x0fff) == 0x0fff || used >= max_substitutions ||
            (staged_state.players.at(bench_slot & 0x0fff).w364 & 0x1000)) {
            error = "Die Auswechslung erfüllt die Source-Bedingungen nicht.";
            return false;
        }
        const size_t first = std::min(field_at, bench_at), second = std::max(field_at, bench_at);
        const size_t board = size_t(active) * 0x840;
        if (board + (size_t(bench_row) + 1) * 0x58 > staged_state.tactics.size()) {
            error = "Die taktischen Spielerdaten sind unvollständig.";
            return false;
        }
        auto* a = staged_state.tactics.data() + board + size_t(field_row) * 0x58;
        auto* b = staged_state.tactics.data() + board + size_t(bench_row) * 0x58;
        const uint8_t fx = a[0x41], fy = a[0x42], bx = b[0x41], by = b[0x42];
        b[0] = uint8_t(minute); // EndPhase passes minute + 1 as PitchScreen param_2; source stores param_2 - 1.
        staged_state.squad_slots[first] = bench_slot;
        staged_state.squad_slots[second] = field_slot;
        std::swap_ranges(a, a + 0x58, b);
        a[0x41] = fx; a[0x42] = fy; b[0x41] = bx; b[0x42] = by;
        staged_state.squad_slots[bench_at] &= 0x1fff;
        staged_state.squad_slots[field_at] &= 0x1fff;
        staged_state.players.at(bench_slot & 0x0fff).w364 |= 0x1000;
        staged_state.squad_slots[field_at] |= 0x8000;
        staged_state.squad_slots[bench_at] |= 0x4000;
        staged_state.team_flags.at(size_t(team)) = uint16_t(
            (staged_state.team_flags.at(size_t(team)) & 0xff1f) | uint16_t(used + 1) << 5);
    }
    state=std::move(staged_state);return true;
}
}
