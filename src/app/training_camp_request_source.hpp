// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "ui/training_camp.hpp"
#include "game/desk_actions.hpp"
#include <limits>
namespace bmh::app {
// Caller owns the open menu and rollback. Focus, prices, booking limits and
// effects are validated/calculated by the same Original command/Core path.
inline bool apply_training_camp_request_source(GameState& g,WatcomRandom& random,
    const ui::TrainingCampRequest& request,uint64_t& revision,std::string& error) {
    error.clear();
    if(revision==std::numeric_limits<uint64_t>::max()){error="revision_exhausted";return false;}
    if (request.manager_slot < 0 || size_t(request.manager_slot) >= g.manager_teams.size() ||
        request.manager_slot != int(g.memory.u8(::bmh::layout::ActiveManager)) ||
        request.runtime_club != g.manager_teams[size_t(request.manager_slot)] || request.revision != revision) {
        error = "stale_manager_or_revision";
        return false;
    }
    const size_t rec = size_t(request.manager_slot) * 0xb0c;
    if (request.kind == ui::TrainingCampRequestKind::set_focus) {
        const std::array<uint8_t, 4> current{
            g.manager_records.at(rec + 0xc1), g.manager_records.at(rec + 0xc2),
            g.manager_records.at(rec + 0xc3), g.manager_records.at(rec + 0xc4)};
        if (request.focus == current) {
            error = "focus_unchanged";
            return false;
        }
        if (!set_training_camp_focus(g, request.manager_slot, request.focus)) {
            error = "invalid_focus_request";
            return false;
        }
        ++revision;
        return true;
    }
    if (request.kind == ui::TrainingCampRequestKind::book) {
        if (request.resort < 0 || request.resort >= kTrainingCampResorts) {
            error = "invalid_resort";
            return false;
        }
        const std::array<uint8_t, 4> current{
            g.manager_records.at(rec + 0xc1), g.manager_records.at(rec + 0xc2),
            g.manager_records.at(rec + 0xc3), g.manager_records.at(rec + 0xc4)};
        if (request.focus != current) {
            error = "stale_focus";
            return false;
        }
        switch (training_camp_booking_block(g, request.manager_slot)) {
        case TrainingCampBookingBlock::booked_this_week: error = "booked_this_week"; return false;
        case TrainingCampBookingBlock::visit_limit: error = "visit_limit"; return false;
        case TrainingCampBookingBlock::invalid_request: error = "invalid_request"; return false;
        case TrainingCampBookingBlock::none: break;
        }
        if (!book_training_camp(g, request.manager_slot, request.resort, random)) {
            error = "insufficient_funds";
            return false;
        }
        ++revision;
        return true;
    }
    error = "unsupported_training_camp_request";
    return false;
}
}
