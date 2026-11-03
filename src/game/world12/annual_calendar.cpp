// SPDX-License-Identifier: GPL-3.0-only
#include "game/world12/annual_calendar.hpp"
#include "game/calendar.hpp"
#include "game/week.hpp"
#include <algorithm>
namespace bmh::world12 {
SeasonCalendar make_annual_calendar(const Catalogue& catalogue,const LeImage& german,const LeImage& english,uint32_t date) {
    if(weekday_code(date)!=2)throw FormatError("world12: next annual calendar requires the real first Wednesday");
    const auto source=[&](const LeImage& rules){std::vector<uint16_t> entries(366);for(uint32_t day=0;day<366;++day){const auto at=layout::CompetitionCalendar+day*2;if(!rules.contains(at+1))throw FormatError("world12: actual annual source calendar is missing");auto word=rules.u16(at);if((word&0x4f0e)==0x4000)word=0;entries[day]=word;}return entries;};
    const auto de=align_source_calendar(source(german),calendar_epoch_from_executable(german).weekday,2,366);
    const auto en=align_source_calendar(source(english),calendar_epoch_from_executable(english).weekday,2,366);
    return SeasonCalendar::from_sources(catalogue,de.entries,en.entries);
}
IndoorSeason make_annual_indoor_season(const World& world,const CareerSnapshot& completed,const LeImage& source) {
    completed.validate();if(!world.catalogue.scope.germany||world.indoor||!world.initial_competitions||!completed.world.indoor)
        throw FormatError("world12: annual indoor reset lacks actual old/new ownership");
    const auto& old=*completed.world.indoor;IndoorSeason next;next.rules=indoor_programme_rules(source);
    if(next.rules!=old.rules)throw FormatError("world12: annual indoor Source programme changed");
    const auto title=[&](const auto& rows){const auto found=std::find_if(rows.begin(),rows.end(),[](const auto& row){return row.first==Country::germany;});if(found==rows.end())throw FormatError("world12: annual indoor reigning title is unknown");return found->second;};
    next.previous_holders={title(world.initial_competitions->champions),title(world.initial_competitions->domestic_holders),old.masters_winner.valid()?old.masters_winner:old.previous_holders[2]};
    std::vector<ClubId> slots;for(uint32_t league:{0u,1u,2u,3u,4u,10u,5u,11u,12u})slots.insert(slots.end(),world.membership[league].clubs.begin(),world.membership[league].clubs.end());
    if(slots.size()!=164)throw FormatError("world12: annual indoor actual source namespace is incomplete");
    for(auto id:next.previous_holders){if(!world.clubs.contains(id))throw FormatError("world12: annual indoor previous holder is not retained");if(std::find(slots.begin(),slots.end(),id)==slots.end())slots.push_back(id);}
    for(size_t i=0;i<4;++i) {
        if(old.selection_slots.empty()){next.initial_scratch_slots[i]=old.initial_scratch_slots[i];continue;}
        const auto old_slot=std::find(old.selection_slots.begin(),old.selection_slots.end(),old.masters_scratch[i]);
        if(old_slot==old.selection_slots.end())throw FormatError("world12: annual indoor scratch has unknown old physical provenance");const auto raw=size_t(old_slot-old.selection_slots.begin());
        size_t retained=raw;
        if(raw>=164){const auto id=old.masters_scratch[i];auto current=std::find(slots.begin(),slots.end(),id);if(current==slots.end()){if(!world.clubs.contains(id))throw FormatError("world12: annual indoor scratch reference was lost");slots.push_back(id);retained=slots.size()-1;}else retained=size_t(current-slots.begin());}
        if(retained>=slots.size())throw FormatError("world12: annual indoor scratch slot lies outside real retained clubs");
        next.initial_scratch_slots[i]=uint16_t(retained);next.masters_scratch[i]=slots[retained];
    }
    return next;
}
}
