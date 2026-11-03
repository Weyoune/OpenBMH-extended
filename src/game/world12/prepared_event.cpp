// SPDX-License-Identifier: GPL-3.0-only
#include "game/world12/prepared_event.hpp"
#include "game/world12/progression.hpp"
#include <algorithm>
#include <map>
#include <set>
namespace bmh::world12 {
uint8_t prepared_cup_flags(const KnockoutStage& stage) {
    auto flags=stage.flags;
    if((flags&5)==5&&stage.next_leg()==1)flags&=0xf7;
    if((stage.cup==DomesticCup::german_cup||stage.cup==DomesticCup::fa_cup)&&stage.next_fixtures().size()==1)flags&=0xfd;
    return flags;
}
void validate_prepared_event(const CareerSnapshot& state) {
    if(!state.prepared_event)return;
    const auto& event=*state.prepared_event;
    if(!state.progress||state.progress->phase!=ProgressPhase::scheduled_event||!state.desktop||
       state.desktop->manager_order.empty()||!state.calendar||event.day!=state.calendar->next_day||
       event.calendar_revision!=state.calendar->revision||event.groups.size()>32)
        throw FormatError("world12: prepared match environment has no current calendar owner");
    if(uint8_t(event.desk_phase)>2||
       (event.desk_phase==PreparedEvent::DeskPhase::desks&&(!state.desktop->initial_event||event.gates_booked))||
       (event.desk_phase!=PreparedEvent::DeskPhase::desks&&(state.desktop->initial_event||state.desktop->active_seat||state.desktop->autosave_pending)))
        throw FormatError("world12: prepared match desk owner/phase mismatch");
    if(event.pools_matches) {
        if(event.desk_phase==PreparedEvent::DeskPhase::unstarted||event.pools_matches->size()>11)
            throw FormatError("world12: invalid prepared pools phase/count");
        size_t next=0;
        for(const auto& fixture:*event.pools_matches) {
            size_t position=0;bool found=false;
            for(const auto& group:event.groups)for(const auto& match:group.matches) {
                if(!found&&position>=next&&match.fixture==fixture){next=position+1;found=true;}
                ++position;
            }
            if(!found)throw FormatError("world12: pools selection is outside its ordered appointment");
        }
    }
    const auto ticket=prepare_day(state);
    if(!ticket||ticket->leagues.size()+ticket->cups.size()+ticket->european.size()+ticket->indoor.size()+(ticket->regional_playoff?1u:0u)+ticket->national.size()+ticket->friendlies.size()!=event.groups.size()||event.groups.empty())
        throw FormatError("world12: prepared match groups do not cover the calendar ticket");
    for(const auto& appointment:ticket->others)if(!domestic_cups_on(appointment)&&!european_cups_on(appointment)&&!german_indoor_appointment(appointment)&&!regional_playoff_appointment(appointment)&&!national_appointment(appointment,state.season)&&!friendly_appointment(appointment))
        throw FormatError("world12: prepared weather cannot own a shared/national appointment");
    std::map<Country,std::set<uint8_t>> chosen;
    for(size_t i=0;i<event.groups.size();++i) {
        const auto& group=event.groups[i];std::vector<Fixture> fixtures;
        if(i<ticket->leagues.size()) {
            const auto& planned=ticket->leagues[i];
            if(group.friendly||group.national||group.regional_playoff||group.league!=planned.appointment.league||group.cup||group.european||group.indoor||group.source_entry||group.leg||group.revision||
               group.flags!=2||group.round!=planned.appointment.round)
                throw FormatError("world12: prepared league environment belongs to another round");
            fixtures=planned.matches;
        }else if(i<ticket->leagues.size()+ticket->cups.size()) {
            const auto& planned=ticket->cups[i-ticket->leagues.size()];
            const auto stage=std::find_if(state.cups.rbegin(),state.cups.rend(),[&](const auto& cup){return cup.cup==planned.cup;});
            if(group.friendly||group.national||group.regional_playoff||group.league||group.european||group.indoor||group.cup!=planned.cup||group.source_entry!=planned.source_entry||group.leg!=planned.leg||
               group.revision!=planned.revision||stage==state.cups.rend()||group.flags!=prepared_cup_flags(*stage)||
               group.round!=stage->rounds.size())
                throw FormatError("world12: prepared cup environment belongs to another leg");
            fixtures=planned.matches;
        }
        else if(i<ticket->leagues.size()+ticket->cups.size()+ticket->european.size()) {
            const auto& planned=ticket->european[i-ticket->leagues.size()-ticket->cups.size()];
            if(group.friendly||group.national||group.regional_playoff||group.league||group.cup||group.indoor||group.european!=planned.competition||group.source_entry!=planned.source_entry||group.leg!=planned.leg||group.round!=planned.round||group.flags!=planned.flags||group.revision!=planned.revision)
                throw FormatError("world12: prepared European environment belongs to another stage/leg");
            fixtures=planned.matches;
        }
        else if(i<ticket->leagues.size()+ticket->cups.size()+ticket->european.size()+ticket->indoor.size()) {
            const auto& planned=ticket->indoor[i-ticket->leagues.size()-ticket->cups.size()-ticket->european.size()];
            if(group.friendly||group.national||group.regional_playoff||group.league||group.cup||group.european||!group.indoor||group.indoor_tournament!=planned.tournament||group.source_entry!=planned.source_entry||group.round!=planned.played+1||group.leg!=1||group.flags!=planned.flags||group.revision!=planned.revision)throw FormatError("world12: prepared indoor environment belongs to another tournament/pass");fixtures=planned.matches;
        }
        else if(i<ticket->leagues.size()+ticket->cups.size()+ticket->european.size()+ticket->indoor.size()+(ticket->regional_playoff?1u:0u)) {
            if(!ticket->regional_playoff)throw FormatError("world12: unexpected regional prepared group");const auto& planned=*ticket->regional_playoff;
            if(group.friendly||group.national||!group.regional_playoff||group.league||group.cup||group.european||group.indoor||group.source_entry!=35||group.round!=planned.played+1||group.leg!=1||group.flags!=planned.flags||group.revision!=planned.revision)
                throw FormatError("world12: prepared regional environment belongs to another round/date");fixtures=planned.matches;
        }
        else if(i<ticket->leagues.size()+ticket->cups.size()+ticket->european.size()+ticket->indoor.size()+(ticket->regional_playoff?1u:0u)+ticket->national.size()) {
            const auto& planned=ticket->national.at(i-ticket->leagues.size()-ticket->cups.size()-ticket->european.size()-ticket->indoor.size()-(ticket->regional_playoff?1u:0u));
            if(group.friendly||group.national!=planned.competition||group.league||group.cup||group.european||group.indoor||group.regional_playoff||group.source_entry!=planned.source_entry||group.round!=planned.round||group.leg!=planned.leg||group.flags!=planned.flags||group.revision!=planned.revision)throw FormatError("world12: prepared national group belongs to another programme/stage");
            fixtures=planned.matches;
        }
        else {const auto& planned=ticket->friendlies.at(i-ticket->leagues.size()-ticket->cups.size()-ticket->european.size()-ticket->indoor.size()-(ticket->regional_playoff?1u:0u)-ticket->national.size());if(!group.friendly||group.friendly_national!=planned.national||group.national||group.league||group.cup||group.european||group.indoor||group.regional_playoff||group.source_entry!=24||group.round!=1||group.leg!=1||group.flags!=planned.flags||group.revision!=planned.revision)throw FormatError("world12: prepared friendly belongs to another booked day");fixtures=planned.matches;}
        if(group.matches.size()!=fixtures.size()||group.matches.empty()||group.matches.size()>32)
            throw FormatError("world12: incomplete prepared fixture environment");
        for(size_t m=0;m<fixtures.size();++m) {
            const auto& match=group.matches[m];
            if(match.fixture!=fixtures[m]||match.rain>7||match.referee>=50)
                throw FormatError("world12: invalid prepared fixture/weather identity");
            if(match.spectators.has_value()!=event.gates_booked||(match.spectators&&*match.spectators>2147483647u))
                throw FormatError("world12: incomplete or invalid booked attendance");
            if(match.kickoff_disposition&&(!event.gates_booked||group.indoor||*match.kickoff_disposition>2||(*match.kickoff_disposition&&*match.spectators!=0)))
                throw FormatError("world12: invalid pre-attendance kickoff receipt");
            const auto& home=state.world.clubs.at(match.fixture.home);const auto& away=state.world.clubs.at(match.fixture.away);
            const bool needs_referee=home.kind==ClubKind::club&&away.kind==ClubKind::club&&!(group.flags&0x80);
            const auto referee_country=group.indoor?Country::germany:home.country;
            if(match.referee_country!=referee_country||
               (needs_referee&&(!match.referee||!chosen[referee_country].insert(match.referee).second)))
                throw FormatError("world12: invalid prepared country referee assignment");
        }
    }
}
}
