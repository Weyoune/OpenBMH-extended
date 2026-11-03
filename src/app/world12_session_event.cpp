// SPDX-License-Identifier: GPL-3.0-only
#include "app/world12_session.hpp"
#include "ui/desktop.hpp"
#include <algorithm>
namespace bmh::app::world12 {
using namespace bmh::world12;
void Session::advance_to_next_desktop(const ui::DeskRequest& request,const std::filesystem::path& data,
    const ClockHooks& clock,const HumanRoundHooks& matches) {
    if(request.kind!=ui::DeskRequestKind::advance_turn)throw FormatError("world12: not a complete turn request");
    validate_file_owner(request.manager_slot,request.revision);
    if(!state_.desktop->initial_event||state_.desktop->initial_event->offer||state_.desktop->autosave_pending||
       size_t(state_.desktop->initial_event->manager_position)+1!=state_.desktop->initial_event->manager_order.size())
        throw FormatError("world12: another desk, offer or autosave still owns the current event");
    if(state_.prepared_event&&!matches.decline_unpresented_events&&(!matches.live_event||!matches.after_minute)) {
        for(const auto& group:state_.prepared_event->groups)for(const auto& match:group.matches)
            if(std::any_of(state_.world.managers.begin(),state_.world.managers.end(),[&](const auto& manager){
                return manager.club==match.fixture.home||manager.club==match.fixture.away;
            }))throw FormatError("world12: complete human turn requires live and intervention presenters");
    }
    const auto before=state_;const auto before_owner=owner_;const auto before_menu=preparation_menu_;auto staged=*this;
    if(staged.state_.prepared_event) {
        staged.finish_scheduled_desks(request);
        staged.book_scheduled_attendance(staged.revision(),data);
        staged.play_prepared_matches(staged.revision(),matches);
    }else if(staged.state_.progress&&staged.state_.progress->phase==ProgressPhase::scheduled_event){finish_national_nomination_desks(staged.state_,staged.owner_,{staged.owner_.scope,request.revision},staged.resources_.reference);}else staged.finish_initial_day(request);
    for(size_t n=0;n<32;++n) {
        // Indoor Source passes own another event on the same date. Ordinary
        // English fixtures already committed once; no daily service repeats.
        const bool same_date_pass=staged.state_.world.indoor&&staged.state_.world.indoor->date_pass&&staged.state_.progress&&staged.state_.progress->phase==ProgressPhase::scheduled_event;
        if(!same_date_pass)staged.seek_saved_event(staged.revision(),data,clock);
        const auto ticket=prepare_day(staged.state_);
        if(!ticket||staged.state_.progress->phase!=ProgressPhase::scheduled_event)
            throw FormatError("world12: closing season needs its awards and annual desktop owner");
        if(ticket->leagues.empty()&&ticket->cups.empty()&&ticket->others.size()==1&&
           ticket->others.front().domain==CalendarDomain::shared&&ticket->others.front().source_entry==0x4006) {
            const auto nominated=staged.prepare_national_day(staged.revision());
            if(nominated.coach_desk_required){begin_national_nomination_desks(staged.state_,staged.owner_,staged.owner_,staged.resources_.reference);staged.state_.validate();if(state_!=before||owner_.scope!=before_owner.scope||owner_.revision!=before_owner.revision||preparation_menu_!=before_menu)throw FormatError("world12: national turn owner changed");state_=std::move(staged.state_);++owner_.revision;return;}
            staged.finish_uncoached_national_day(staged.revision());continue;
        }
        staged.prepare_scheduled_weather(staged.revision());staged.begin_scheduled_desks(staged.revision());
        if(!staged.state_.desktop->active_seat){staged.book_scheduled_attendance(staged.revision(),data);staged.play_prepared_matches(staged.revision(),matches);continue;}
        staged.state_.validate();
        if(state_!=before||owner_.scope!=before_owner.scope||owner_.revision!=before_owner.revision||preparation_menu_!=before_menu)
            throw FormatError("world12: complete turn changed its source owner during presentation");
        state_=std::move(staged.state_);++owner_.revision;return;
    }
    throw FormatError("world12: complete turn exceeded its bounded calendar search");
}
}
