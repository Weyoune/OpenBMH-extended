// SPDX-License-Identifier: GPL-3.0-only
#include "ui/world12_readonly.hpp"
#include "game/world12/progression.hpp"
#include "game/matchday_setup.hpp"
#include "game/messages.hpp"
#include "ui/manager.hpp"
#include <set>
#include <algorithm>
namespace bmh::ui {
using namespace bmh::world12;
CalendarState world12_calendar_state(const CareerSnapshot& state,const LeImage& source,const TextDb& texts){
    state.validate();if(!state.calendar||state.date<state.calendar->next_day)throw FormatError("world12: readonly calendar lacks its actual saved season owner");
    const auto native_start=state.progress?state.progress->season_start:state.date-state.calendar->next_day;const auto first=desk_date(native_start,source);
    if(first.month!=7||first.day<1||first.day>7||native_start<uint32_t(first.day-1))throw FormatError("world12: readonly calendar cannot resolve its actual July anchor");
    CalendarState out;out.leading_days=first.day-1;out.season_start_counter=native_start-uint32_t(out.leading_days);out.current_day=-1;
    std::set<LeagueId> visible;for(auto group:{TableGroup::germany,TableGroup::england,TableGroup::europe})for(auto league:state.world.catalogue.table_leagues(group))visible.insert(league);
    const auto label=[&](uint32_t address){return texts[text_id(address)];};
    GameState flags;flags.season=int(state.season);for(auto address:{layout::CalendarState_FormatError_RuntimeField2,layout::CalendarState_FormatError_RuntimeField,layout::CompetitionSelection})flags.memory.add_block(address,1);flags.memory.set8(layout::CompetitionSelection,(state.world.catalogue.scope.club_play()?1:0)|(state.world.catalogue.scope.international?2:0));
    for(size_t offset=0;offset<=366;++offset){const auto absolute=out.season_start_counter+uint32_t(offset);const auto date=desk_date(absolute,source);if(offset&&date.month==7&&date.day==1)break;if(offset==366)throw FormatError("world12: readonly calendar year does not close at actual July1");
        CalendarDayProjection day;day.date=date;day.day_counter=absolute;day.season_day=int(offset)-out.leading_days;day.current_day=absolute==state.date;day.owned_information=std::string{};if(day.current_day)out.current_day=int(offset);std::vector<std::string> labels;
        if(day.season_day>=0&&day.season_day<state.calendar->schedule.days){const auto relative=uint16_t(day.season_day);
            for(const auto& appointment:state.calendar->schedule.leagues_on(relative)){if(!visible.contains(appointment.league))continue;const auto& league=state.world.catalogue.league(appointment.league);labels.push_back(utf8_to_original(league.name));const auto bit=league.playable?league.country==Country::germany?(league.local_index<2?league.local_index:league.local_index<4?2:3):std::min<int>(league.local_index,3):4;day.competitions|=1u<<bit;}
            for(const auto& appointment:admitted_other_appointments(state,relative)){const auto bits=day_competitions(flags,appointment.source_entry);day.competitions|=bits;
                for(int bit=5;bit<19;++bit)if(bits&(1u<<bit)){const auto address=bit<16?layout::Calendar_Information_TextPointer2+uint32_t(bit)*4:bit==16?layout::Calendar_Information_TextPointer5:layout::Calendar_Information_TextPointer6;labels.push_back(label(address));}}
        }
        for(const auto& value:labels){auto& info=*day.owned_information;if(!info.empty())info+=source.cstring(layout::Calendar_Information_ExecutableField);if(info.size()+value.size()<30)info+=value;else info+=source.cstring(layout::Calendar_Information_TextPointer);}
        out.days.push_back(std::move(day));
    }
    if((out.days.size()!=365&&out.days.size()!=366)||out.current_day<0)throw FormatError("world12: readonly calendar current date requires its next real season owner");return out;
}
}
