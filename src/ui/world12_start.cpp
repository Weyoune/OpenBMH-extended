// SPDX-License-Identifier: GPL-3.0-only
#include "ui/world12_start.hpp"
#include "formats/executable_layout.hpp"
#include "ui/manager.hpp"
#include <algorithm>
namespace bmh::ui {
std::vector<SeasonEntrantsState> world12_season_entrants(const bmh::world12::CareerSnapshot& snapshot,const LeImage& rules,const TextDb& texts){
    snapshot.validate();if(!snapshot.world.catalogue.scope.club_play())return {};
    if(!snapshot.world.initial_competitions)throw FormatError("world12: entrant presentation has no actual field owner");
    return world12_season_entrants(snapshot.world,snapshot.world.initial_competitions->fields,rules,texts);
}
std::vector<SeasonEntrantsState> world12_season_entrants(const bmh::world12::World& world,
    const std::array<std::vector<bmh::world12::ClubId>,3>& actual_fields,const LeImage& rules,const TextDb& texts){
    using namespace bmh::world12;world.validate();std::vector<SeasonEntrantsState> pages;
    if(!world.catalogue.scope.club_play())return pages;
    if(!world.economy||!world.economy->presentation_options)throw FormatError("world12: entrant presentation has no saved options owner");
    const auto& options=*world.economy->presentation_options;
    if(options[9*9+6]!=0)return pages;
    for(const auto country:{Country::germany,Country::england}){
        if(std::none_of(world.managers.begin(),world.managers.end(),[&](const auto& manager){return world.clubs.at(manager.club).country==country;}))continue;
        for(size_t competition=0;competition<3;++competition){
            SeasonEntrantsState page;page.competition=int(competition);page.heading=country==Country::germany?original_to_utf8(texts[1249]):"ENGLISCHE VEREINE";
            const auto kind=uint32_t(competition+2);const auto address=(rules.u8(layout::SeasonEntrants_FormatError_TextPointer+kind)?layout::CupDraw_FormatError_ExecutableField6:layout::SeasonEntrants_FormatError_TextPointer2)+uint32_t(rules.u8(layout::SeasonEntrants_FormatError_RuntimeField+kind))*4;
            page.footer=original_to_utf8(texts[(address-layout::TextPointerTable)/4]);
            page.decorated_background=rules.u8(layout::PlainBackground)==0;page.gradient_text=rules.u8(layout::TextGradientDisabled)==0&&rules.u8(layout::TextGradientMode)==2;
            std::vector<ClubId> clubs;for(auto id:actual_fields[competition])if(world.clubs.at(id).country==country)clubs.push_back(id);
            if(clubs.size()>=8)throw FormatError("world12: entrant count exceeds Original placement slots");
            if(!clubs.empty()){
                const auto first=rules.u8(layout::SeasonEntrants_FormatError_ExecutableField3+uint32_t(clubs.size()));
                if(size_t(first)+clubs.size()>8)throw FormatError("world12: entrant placement exceeds its Original table");
                for(size_t i=0;i<clubs.size();++i){const auto& club=world.clubs.at(clubs[i]);const auto& crest=world.crests.at(club.crest);const auto slot=uint32_t(first+i);
                    page.teams.push_back({int(i),crest.cell,original_to_utf8(club.name),rules.u8(layout::SeasonEntrants_FormatError_ExecutableField+slot),rules.u8(layout::SeasonEntrants_FormatError_ExecutableField2+slot),crest.path});
                }
            }
            pages.push_back(std::move(page));
        }
    }
    return pages;
}
}
