// SPDX-License-Identifier: GPL-3.0-only
#include "app/world12_session.hpp"
#include "app/world12_annual_awards.hpp"
#include "game/world12/annual_coach.hpp"
#include "game/calendar.hpp"
#include "ui/desktop.hpp"
#include <limits>
namespace bmh::app::world12 {
DesktopEntryOutcome Session::begin_annual_closing(uint64_t revision) {
    if(preparation_menu_||revision!=owner_.revision||!owner_.scope||revision==std::numeric_limits<uint64_t>::max()||!resources_.texts)
        throw FormatError("world12: annual closing desk has a stale/open session owner");
    const auto before=state_;const auto previous=owner_;auto candidate=state_;auto staged=owner_;
    if(!bmh::world12::close_annual_national_roles(candidate.world))throw FormatError("world12: annual national role history is unknown");
    // Awards and the first real closing desk publish together. No paid ledger
    // can be replayed when resuming an already-owned closing event.
    if(!candidate.world.annual_awards_completed||uint64_t(*candidate.world.annual_awards_completed)!=uint64_t(candidate.season)+1)
        publish_annual_awards(candidate,staged,staged,resources_.reference,*resources_.texts);
    const auto entry=prepare_annual_closing_desk_event(candidate,staged,staged,resources_.reference);
    if(state_!=before||owner_.scope!=previous.scope||owner_.revision!=previous.revision)
        throw FormatError("world12: session changed during annual closing presentation");
    state_=std::move(candidate);++owner_.revision;return entry;
}
void Session::finish_annual_closing(const ui::DeskRequest& request) {
    if(request.kind!=ui::DeskRequestKind::advance_turn)throw FormatError("world12: not an annual closing desk exit");
    validate_file_owner(request.manager_slot,request.revision);
    finish_annual_closing_event(state_,owner_,{owner_.scope,request.revision});
}
AnnualTransitionOutcome Session::publish_annual_year(uint64_t revision,const std::filesystem::path& scores,const AnnualTransitionHooks& hooks) {
    if(preparation_menu_||!resources_.texts)throw FormatError("world12: annual publication has an open menu or missing Source text owner");
    auto result=publish_annual_transition(state_,owner_,{owner_.scope,revision},resources_.reference,
        resources_.german,resources_.english,*resources_.texts,scores,hooks);
    options_.duration=state_.world.settings->duration;return result;
}
DesktopEntryOutcome Session::begin_annual_opening(uint64_t revision) {
    if(preparation_menu_)throw FormatError("world12: an open menu owns the annual opening");
    return prepare_annual_desk_event(state_,owner_,{owner_.scope,revision},resources_.reference);
}
void Session::finish_annual_opening(const ui::DeskRequest& request) {
    if(request.kind!=ui::DeskRequestKind::advance_turn)throw FormatError("world12: not an annual opening desk exit");
    validate_file_owner(request.manager_slot,request.revision);
    finish_annual_opening_event(state_,owner_,{owner_.scope,request.revision},resources_.reference,
        calendar_epoch_from_executable(resources_.german));
}
}
