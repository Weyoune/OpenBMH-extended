// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/world.hpp"
#include "game/club_info.hpp"
#include <algorithm>
#include <set>
namespace bmh::world12 {
inline uint8_t effective_club_category(const World& world,ClubId id){const auto& c=world.clubs.at(id);return c.presentation_category.value_or(std::any_of(world.managers.begin(),world.managers.end(),[&](const auto& m){return m.club==id;})?2:0);}
// Bound only the clubs the Original four-match rule can observe. No world ID
// is packed into its positional byte tables, and no squad is manufactured.
inline uint8_t change_club_category(World& world,ClubId target,int requested){
    world.validate();if(!world.clubs.contains(target)||requested<0||requested>2)throw FormatError("world12: invalid club presentation request");
    std::set<ClubId> needed{target};for(const auto& m:world.managers)needed.insert(m.club);for(const auto& [id,c]:world.clubs)if(effective_club_category(world,id)==2)needed.insert(id);
    if(needed.size()>0x147)throw FormatError("world12: presentation policy exceeds its bounded table");
    std::vector<ClubId> ids(needed.begin(),needed.end());GameState g;g.team_morale.assign(0x147,25);g.manager_records.resize(5*0xb0c);
    const auto local=[&](ClubId id){return int(std::find(ids.begin(),ids.end(),id)-ids.begin());};
    for(size_t i=0;i<ids.size();++i)g.team_morale[i]=uint8_t(world.clubs.at(ids[i]).morale|(effective_club_category(world,ids[i])<<6));
    for(const auto& m:world.managers)g.manager_teams.push_back(local(m.club));g.memory.add_block(layout::ManagerCount,1);g.memory.set8(layout::ManagerCount,uint8_t(g.manager_teams.size()));
    const auto result=set_club_info_category(g,local(target),requested);auto candidate=world;
    for(size_t i=0;i<ids.size();++i){const auto category=uint8_t(g.team_morale[i]>>6);if(ids[i]==target||category!=effective_club_category(world,ids[i]))candidate.clubs.at(ids[i]).presentation_category=category;}
    candidate.validate();world=std::move(candidate);return result;
}
}
