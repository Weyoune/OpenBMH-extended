// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "ui/training.hpp"
#include "ui/tactics.hpp"
#include "game/state.hpp"
#include "game/week.hpp"
#include "game/schedule.hpp"
#include "game/national_identity.hpp"
#include <algorithm>
#include <limits>
namespace bmh::app {
// Shared Original menu commands. Session/modal authorization stays with the
// caller; these functions validate the projected active manager and revision.
inline bool apply_tactics_request_source(GameState& g,const LeImage& exe,const ui::TacticsRequest& request,uint64_t& revision,std::string& error) {
    error.clear();
    const int active = int(g.memory.u8(::bmh::layout::ActiveManager));
    if (request.manager_slot != active || request.manager_slot < 0 || request.manager_slot > 3 ||
        size_t(request.manager_slot) >= g.manager_teams.size() ||
        request.runtime_club != g.manager_teams[size_t(request.manager_slot)] ||
        request.revision != revision) {
        error = "Die Taktikanfrage ist veraltet."; return false;
    }
    if (request.kind != ui::TacticsRequestKind::set_tactic || request.index < 0 || request.index >= 8 ||
        request.desired_value < 0 || request.desired_value > exe.u8(::bmh::layout::CareerSession_DispatchLiveTactic_RuntimeField + request.index)) {
        error = "Ungültige Taktikeinstellung."; return false;
    }
    const bool live_option = (g.memory.has(::bmh::layout::CareerSession_DispatchLiveTactic_RuntimeField3) ? g.memory.u8(::bmh::layout::CareerSession_DispatchLiveTactic_RuntimeField3) : exe.u8(::bmh::layout::CareerSession_DispatchLiveTactic_RuntimeField3)) != 0;
    if (live_option && exe.u8(::bmh::layout::CareerSession_DispatchLiveTactic_RuntimeField2 + request.index) == 0) {
        error = "Diese Taktikeinstellung ist nicht verfügbar."; return false;
    }
    if ((request.index == 0 && request.desired_assistant_attitude != std::optional<uint8_t>(0)) ||
        (request.index != 0 && request.desired_assistant_attitude)) {
        error = "Ungültige Assistenteneinstellung."; return false;
    }
    // RerateTeam: only +d2..+d9 and (index 0) option byte +2.
    // The tactic board, player order and both RNG streams stay untouched.
    const size_t rec = size_t(request.manager_slot) * 0xb0c;
    const size_t option = size_t(request.manager_slot) * 5 + 2;
    if (g.manager_records.size() < rec + 0xb0c || g.manager_options.size() <= option) {
        error = "Die Taktikdaten sind unvollständig."; return false;
    }
    g.manager_records[rec + 0xd2 + size_t(request.index)] = uint8_t(request.desired_value);
    if (request.index == 0) g.manager_options[option] = 0;
    ++revision;
    return true;
}

inline bool apply_training_request_source(GameState& g,const LeImage& exe,const ui::TrainingRequest& request,uint64_t& revision,std::string& error) {
    const int active = int(g.memory.u8(::bmh::layout::ActiveManager));
    if (request.manager_slot != active || request.manager_slot < 0 || request.manager_slot > 3 ||
        size_t(request.manager_slot) >= g.manager_teams.size() ||
        request.runtime_club != g.manager_teams[size_t(request.manager_slot)] || request.revision != revision) {
        error = "stale_manager_or_revision";
        return false;
    }

    try {
        const ui::TrainingState current = ui::training_from_game(g, exe, revision);
        if (current.manager_slot != request.manager_slot || current.runtime_club != request.runtime_club) {
            error = "stale_manager_or_revision";
            return false;
        }
        const size_t slot = size_t(request.manager_slot);
        const size_t rec = slot * 0xb0c;
        const size_t option_at = slot * 5 + 4;
        if (g.manager_records.size() < rec + 0xb0c || g.manager_options.size() <= option_at) {
            error = "invalid_training_state";
            return false;
        }

        auto valid_plan = [](const std::array<uint8_t, 4>& types, const std::array<uint8_t, 9>& areas) {
            int type_sum = 0, fitness = 0, skills = 0;
            for (uint8_t value : types) { if (value < 1 || value > 3) return false; type_sum += value; }
            for (size_t i = 0; i < areas.size(); ++i) {
                const uint8_t value = areas[i];
                if (value < 1 || value > 3) return false;
                if (i < 2) fitness += value; else skills += value;
            }
            return type_sum <= 8 && fitness <= 4 && skills <= 18;
        };
        if (!valid_plan(request.desired_types, request.desired_areas)) {
            error = "invalid_training_plan";
            return false;
        }

        ui::TrainingRequestKind kind = request.kind;
        auto types = current.types;
        auto areas = current.areas;
        uint8_t automation = current.automation;
        bool run_automatic = false;
        // NeedsInitialization initializes an uninitialized screen via Initialize;
        // the four subordinate toggles exist only while the global flag is on.
        // NeedsInitialization2 calls that same deterministic routine after an enabled
        // automation toggle. Manual field edits only write their field and clear
        // the matching assistant flag; they do not run training or draw RNG.
        switch (kind) {
        case ui::TrainingRequestKind::initialize:
            if (!current.needs_initialization() || request.index != -1 || request.value != -1 ||
                request.player_id != -1 || request.desired_types != types || request.desired_areas != areas ||
                request.desired_automation != 0x9f) {
                error = "invalid_training_initialization";
                return false;
            }
            automation = 0x9f;
            run_automatic = true;
            break;
        case ui::TrainingRequestKind::set_type:
            if (current.needs_initialization() || request.index < 0 || request.index >= int(types.size()) ||
                request.value < 1 || request.value > 3 || request.player_id != -1 ||
                request.value == types[size_t(request.index)]) {
                error = "invalid_training_type";
                return false;
            }
            types[size_t(request.index)] = uint8_t(request.value);
            automation = uint8_t(automation & ~uint8_t(1u << 1));
            break;
        case ui::TrainingRequestKind::set_area:
            if (current.needs_initialization() || request.index < 0 || request.index >= int(areas.size()) ||
                request.value < 1 || request.value > 3 || request.player_id != -1 ||
                request.value == areas[size_t(request.index)]) {
                error = "invalid_training_area";
                return false;
            }
            areas[size_t(request.index)] = uint8_t(request.value);
            automation = uint8_t(automation & ~uint8_t(1u << 2));
            break;
        case ui::TrainingRequestKind::set_player_intensity:
        case ui::TrainingRequestKind::set_player_focus: {
            if (current.needs_initialization() || request.index < 0 || request.index >= current.squad_capacity ||
                request.player_id < 0 || request.player_id > 0x0ffe) {
                error = "invalid_training_player";
                return false;
            }
            const auto player = std::find_if(current.players.begin(), current.players.end(), [&](const auto& p) {
                return p.row == request.index && p.player_id == request.player_id;
            });
            const SquadRange squad = squad_range(g,request.runtime_club);
            if (player == current.players.end() || squad.count != size_t(current.squad_capacity) ||
                squad.first + size_t(request.index) >= g.squad_slots.size() ||
                (g.squad_slots[squad.first + size_t(request.index)] & 0x0fff) != request.player_id) {
                error = "stale_training_player";
                return false;
            }
            if (kind == ui::TrainingRequestKind::set_player_intensity) {
                if (request.value < 1 || request.value > 3 || request.value == player->intensity) {
                    error = "invalid_training_intensity";
                    return false;
                }
                automation = uint8_t(automation & ~uint8_t(1u << 3));
            } else if (request.value < 0 || request.value > 7 || request.value != (player->focus + 1) % 8) {
                error = "invalid_training_focus";
                return false;
            }
            break;
        }
        case ui::TrainingRequestKind::set_automation: {
            if (current.needs_initialization() || request.index < 0 || request.index > 4 ||
                (request.value != 0 && request.value != 1) || request.player_id != -1) {
                error = "invalid_training_automation";
                return false;
            }
            if (request.index != 0 && !(current.automation & 1)) {
                error = "training_automation_group_hidden";
                return false;
            }
            const uint8_t mask = uint8_t(1u << request.index);
            automation = uint8_t(automation ^ mask);
            if (((automation & mask) != 0) != (request.value != 0)) {
                error = "stale_training_automation";
                return false;
            }
            run_automatic = (automation & 1) != 0;
            break;
        }
        case ui::TrainingRequestKind::none:
        default:
            error = "unsupported_training_request";
            return false;
        }

        if (request.desired_types != types || request.desired_areas != areas ||
            request.desired_automation != automation) {
            error = "stale_training_projection";
            return false;
        }

        GameState staged = g;
        std::copy(types.begin(), types.end(), staged.manager_records.begin() + rec + 0xc5);
        std::copy(areas.begin(), areas.end(), staged.manager_records.begin() + rec + 0xc9);
        staged.manager_options[option_at] = automation;
        if (kind == ui::TrainingRequestKind::set_player_intensity ||
            kind == ui::TrainingRequestKind::set_player_focus) {
            const size_t board = slot * 0x840 + size_t(request.index) * 0x58 + 0x0a;
            if (board >= staged.tactics.size()) {
                error = "invalid_training_board";
                return false;
            }
            if (kind == ui::TrainingRequestKind::set_player_intensity)
                staged.tactics[board] = uint8_t((staged.tactics[board] & 0xfc) | request.value);
            else
                staged.tactics[board] = uint8_t((staged.tactics[board] & 0x03) | (request.value << 2));
        }
        if (run_automatic) auto_training(staged, request.manager_slot);
        g = std::move(staged);
        ++revision;
        error.clear();
        return true;
    } catch (const std::exception&) {
        error = "invalid_training_state";
        return false;
    }
}
}
