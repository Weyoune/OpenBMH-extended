// SPDX-License-Identifier: GPL-3.0-only
#include "app/world12_session.hpp"
#include "ui/desktop.hpp"
#include <algorithm>
namespace bmh::app::world12 {
using namespace bmh::world12;
bool Session::advance_to_next_desktop(const ui::DeskRequest& request,const std::filesystem::path& data,
    const ClockHooks& clock,const HumanRoundHooks& matches,const AnnualTransitionHooks* annual,
    const std::filesystem::path& score_file) {
    if(request.kind!=ui::DeskRequestKind::advance_turn)throw FormatError("world12: not a complete turn request");
    validate_file_owner(request.manager_slot,request.revision);
    if(!state_.desktop->initial_event||state_.desktop->initial_event->offer||state_.desktop->autosave_pending||
       size_t(state_.desktop->initial_event->manager_position)+1!=state_.desktop->initial_event->manager_order.size())
        throw FormatError("world12: another desk, offer or autosave still owns the current event");
    if(state_.prepared_event&&!matches.decline_unpresented_events&&(!matches.live_event||!matches.after_minute)) {
        const auto coaches=bmh::world12::national_desktop_actors(state_);
        for(const auto& group:state_.prepared_event->groups)for(const auto& match:group.matches)
            if(std::any_of(state_.world.managers.begin(),state_.world.managers.end(),[&](const auto& manager){
                return manager.club==match.fixture.home||manager.club==match.fixture.away;
            })||std::any_of(coaches.begin(),coaches.end(),[&](const auto& coach){return coach.team==match.fixture.home||coach.team==match.fixture.away;}))throw FormatError("world12: complete human turn requires live and intervention presenters");
    }
    const auto before=state_;const auto before_owner=owner_;const auto before_menu=preparation_menu_;auto staged=*this;
    const auto publish=[&](bool complete){
        staged.state_.validate();
        if(state_!=before||owner_.scope!=before_owner.scope||owner_.revision!=before_owner.revision||preparation_menu_!=before_menu)
            throw FormatError("world12: complete turn changed its source owner during presentation");
        state_=std::move(staged.state_);options_=staged.options_;++owner_.revision;return complete;
    };
    if(staged.state_.prepared_event) {
        staged.finish_scheduled_desks(request);
        staged.book_scheduled_attendance(staged.revision(),data);
        staged.play_prepared_matches(staged.revision(),matches);
    }else if(staged.state_.progress&&staged.state_.progress->phase==ProgressPhase::season_end)
        staged.finish_annual_closing(request);
    else if(staged.state_.season&&staged.state_.calendar&&!staged.state_.calendar->next_day&&!staged.state_.progress)
        staged.finish_annual_opening(request);
    else if(staged.state_.progress&&staged.state_.progress->phase==ProgressPhase::scheduled_event)
        finish_national_nomination_desks(staged.state_,staged.owner_,{staged.owner_.scope,request.revision},staged.resources_.reference);
    else staged.finish_initial_day(request);
    for(size_t n=0;n<32;++n) {
        if(staged.state_.progress&&staged.state_.progress->phase==ProgressPhase::annual_transition){
            if(!annual)throw FormatError("world12: annual publication needs its actual presenters");
            const auto outcome=staged.publish_annual_year(staged.revision(),score_file,*annual);
            if(outcome.career_complete)return publish(true);
            if(!staged.state_.world.catalogue.scope.club_play())continue;
            staged.begin_annual_opening(staged.revision());return publish(false);
        }
        // Indoor Source passes own another event on the same date. Ordinary
        // English fixtures already committed once; no daily service repeats.
        const bool same_date_pass=staged.state_.world.indoor&&staged.state_.world.indoor->date_pass&&staged.state_.progress&&staged.state_.progress->phase==ProgressPhase::scheduled_event;
        if(!same_date_pass)staged.seek_saved_event(staged.revision(),data,clock);
        if(staged.state_.progress&&staged.state_.progress->phase==ProgressPhase::season_end){
            if(!staged.state_.world.catalogue.scope.club_play()){
                if(!annual)throw FormatError("world12: national annual publication needs its actual presenters");
                const auto outcome=staged.publish_annual_year(staged.revision(),score_file,*annual);
                if(outcome.career_complete)return publish(true);continue;
            }
            staged.begin_annual_closing(staged.revision());return publish(false);
        }
        const auto ticket=prepare_day(staged.state_);
        if(!ticket||staged.state_.progress->phase!=ProgressPhase::scheduled_event)
            throw FormatError("world12: closing season needs its awards and annual desktop owner");
        if(ticket->leagues.empty()&&ticket->cups.empty()&&ticket->others.size()==1&&
           ticket->others.front().domain==CalendarDomain::shared&&ticket->others.front().source_entry==0x4006) {
            const auto nominated=staged.prepare_national_day(staged.revision());
            if(nominated.coach_desk_required){begin_national_nomination_desks(staged.state_,staged.owner_,staged.owner_,staged.resources_.reference);return publish(false);}
            staged.finish_uncoached_national_day(staged.revision());continue;
        }
        staged.prepare_scheduled_weather(staged.revision());staged.begin_scheduled_desks(staged.revision());
        if(!staged.state_.desktop->active_seat){staged.book_scheduled_attendance(staged.revision(),data);staged.play_prepared_matches(staged.revision(),matches);continue;}
        return publish(false);
    }
    throw FormatError("world12: complete turn exceeded its bounded calendar search");
}
}
