// SPDX-License-Identifier: GPL-3.0-only
#include "ui/world12_readonly.hpp"
#include "ui/manager.hpp"
#include <algorithm>
namespace bmh::ui {
using namespace bmh::world12;
IndoorStandingsState world12_indoor_standings(const CareerSnapshot& state,const World12ReadonlyIdentity& identity,const GameState& reference,const LeImage& source,const TextDb& texts){
    state.validate();if(!state.world.indoor||!state.world.catalogue.scope.germany)throw FormatError("world12: indoor standings require their actual German season owner");
    const auto& indoor=*state.world.indoor;std::vector<ClubId> ids;
    if(indoor.selection_slots.empty()){for(auto league:{LeagueId{0},LeagueId{1},LeagueId{2}})for(auto club:state.world.membership[league.value].clubs)if(ids.size()<40)ids.push_back(club);}else ids.assign(indoor.selection_slots.begin(),indoor.selection_slots.begin()+40);
    for(auto club:indoor.previous_holders)if(std::find(ids.begin(),ids.end(),club)==ids.end())ids.push_back(club);
    for(const auto& manager:state.world.managers)if(std::find(ids.begin(),ids.end(),manager.club)==ids.end())ids.push_back(manager.club);
    const auto local=[&](ClubId club){const auto found=std::find(ids.begin(),ids.end(),club);if(found==ids.end())throw FormatError("world12: indoor standings lost a canonical club");return uint16_t(found-ids.begin());};
    GameState game=reference;game.manager_teams.clear();game.manager_records.assign(state.world.managers.size()*0xb0c,0);game.club_names={0};game.club_ids.resize(ids.size());
    if(state.world.economy&&state.world.economy->presentation_options){game.memory.add_block(layout::LeaguePresentationOptions,180);for(uint32_t i=0;i<180;++i)game.memory.set8(layout::LeaguePresentationOptions+i,(*state.world.economy->presentation_options)[i]);}
    for(size_t i=0;i<ids.size();++i){const auto name=utf8_to_original(state.world.clubs.at(ids[i]).name);if(game.club_names.size()+name.size()+1>65535)throw FormatError("world12: indoor actual name pool exceeds Source");game.club_ids[i]=uint16_t(game.club_names.size());game.club_names.insert(game.club_names.end(),name.begin(),name.end());game.club_names.push_back(0);}
    for(auto address:{layout::ActiveManager,layout::ManagerCount,layout::DeskPosition,layout::Cup_AdvanceCup_RuntimeField4,layout::Cup_AdvanceCup_RuntimeField3})game.memory.add_block(address,1);game.memory.set8(layout::ManagerCount,uint8_t(state.world.managers.size()));game.memory.set8(layout::DeskPosition,0);
    int actor=-1;for(size_t i=0;i<state.world.managers.size();++i){const auto& manager=state.world.managers[i];game.manager_teams.push_back(local(manager.club));const auto name=utf8_to_original(manager.name);std::copy_n(name.begin(),std::min<size_t>(20,name.size()),game.manager_records.begin()+i*0xb0c+0xa8);if(manager.seat==identity.manager){actor=int(i);game.memory.set8(layout::ActiveManager,uint8_t(i));}}
    if(actor<0)throw FormatError("world12: indoor standings require a real canonical viewer");
    game.indoor=state.prepared_event&&std::any_of(state.prepared_event->groups.begin(),state.prepared_event->groups.end(),[](const auto& group){return group.indoor;});
    game.memory.set8(layout::Cup_AdvanceCup_RuntimeField4,indoor.tournaments.empty()?0:indoor.tournaments.back().ordinal);game.memory.set8(layout::Cup_AdvanceCup_RuntimeField3,indoor.tournaments.empty()?0:indoor.tournaments.back().strength);
    game.memory.add_block(layout::Cup_AdvanceCup_ExecutableField,80);for(uint32_t i=0;i<40;++i)game.memory.set16(layout::Cup_AdvanceCup_ExecutableField+i*2,indoor.points[i]);game.memory.add_block(layout::Cup_AdvanceCup_RuntimeField6,6);for(uint32_t i=0;i<3;++i)game.memory.set16(layout::Cup_AdvanceCup_RuntimeField6+i*2,local(indoor.previous_holders[i]));
    DeskRequest owner;owner.kind=DeskRequestKind::open_menu;owner.menu_kind=9;owner.manager_slot=actor;owner.revision=identity.revision;auto out=indoor_standings_from_game(game,source,texts,owner);out.owner.manager_slot=identity.manager;for(auto& row:out.rows)row.world_club=ids.at(size_t(row.team));return out;
}
}
