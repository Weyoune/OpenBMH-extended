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
    const auto ticket=prepare_day(state);
    if(!ticket||ticket->leagues.size()+ticket->cups.size()!=event.groups.size()||event.groups.empty())
        throw FormatError("world12: prepared match groups do not cover the calendar ticket");
    for(const auto& appointment:ticket->others)if(!domestic_cups_on(appointment))
        throw FormatError("world12: prepared weather cannot own a shared/national appointment");
    std::map<Country,std::set<uint8_t>> chosen;
    for(size_t i=0;i<event.groups.size();++i) {
        const auto& group=event.groups[i];std::vector<Fixture> fixtures;
        if(i<ticket->leagues.size()) {
            const auto& planned=ticket->leagues[i];
            if(group.league!=planned.appointment.league||group.cup||group.source_entry||group.leg||group.revision||
               group.flags!=2||group.round!=planned.appointment.round)
                throw FormatError("world12: prepared league environment belongs to another round");
            fixtures=planned.matches;
        }else {
            const auto& planned=ticket->cups[i-ticket->leagues.size()];
            const auto stage=std::find_if(state.cups.rbegin(),state.cups.rend(),[&](const auto& cup){return cup.cup==planned.cup;});
            if(group.league||group.cup!=planned.cup||group.source_entry!=planned.source_entry||group.leg!=planned.leg||
               group.revision!=planned.revision||stage==state.cups.rend()||group.flags!=prepared_cup_flags(*stage)||
               group.round!=stage->rounds.size())
                throw FormatError("world12: prepared cup environment belongs to another leg");
            fixtures=planned.matches;
        }
        if(group.matches.size()!=fixtures.size()||group.matches.empty()||group.matches.size()>32)
            throw FormatError("world12: incomplete prepared fixture environment");
        for(size_t m=0;m<fixtures.size();++m) {
            const auto& match=group.matches[m];
            if(match.fixture!=fixtures[m]||match.rain>7||match.referee>=50)
                throw FormatError("world12: invalid prepared fixture/weather identity");
            if(match.spectators.has_value()!=event.gates_booked||(match.spectators&&*match.spectators>2147483647u))
                throw FormatError("world12: incomplete or invalid booked attendance");
            const auto& home=state.world.clubs.at(match.fixture.home);const auto& away=state.world.clubs.at(match.fixture.away);
            const bool needs_referee=home.kind==ClubKind::club&&away.kind==ClubKind::club&&!(group.flags&0x80);
            if(match.referee_country!=home.country||
               (needs_referee&&(!match.referee||!chosen[home.country].insert(match.referee).second)))
                throw FormatError("world12: invalid prepared country referee assignment");
        }
    }
}
}
