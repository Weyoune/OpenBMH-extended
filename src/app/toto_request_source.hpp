// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/state.hpp"
#include "game/desk_actions.hpp"
#include "ui/toto.hpp"
#include <limits>
namespace bmh::app {
inline bool apply_toto_request_source(GameState& g,const ui::TotoRequest& request,uint64_t& revision,std::string& error) {
    if(revision==UINT64_MAX){error="toto_revision_exhausted";return false;}
    const int active = int(g.memory.u8(::bmh::layout::ActiveManager));
    if (request.kind != ui::TotoRequestKind::submit || request.manager_slot != active || request.manager_slot < 0 ||
        size_t(request.manager_slot) >= g.manager_teams.size() ||
        request.runtime_club != g.manager_teams[size_t(request.manager_slot)] || request.revision != revision) {
        error = "stale_manager_or_revision"; return false;
    }
    if (g.memory.u8(::bmh::layout::CareerSession_DispatchToto_RuntimeField) != 11) { error = "toto_coupon_requires_eleven_matches"; return false; }
    if (!submit_pools_coupon(g, request.manager_slot, request.stake, request.tips)) {
        error = "invalid_toto_coupon"; return false;
    }
    ++revision;
    return true;
}
}
