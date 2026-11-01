// SPDX-License-Identifier: GPL-3.0-only
#include "game/world12/desktop_projection.hpp"
#include <algorithm>
namespace bmh::world12 {
AiMatchProjection project_manager_desktop(const CareerSnapshot& state,const GameState& reference,const std::vector<ClubId>& extra_clubs) {
    state.validate();
    if(!state.desktop||!state.desktop->active_seat||!state.world.settings)throw FormatError("world12: manager desktop is unavailable");
    std::vector<ClubId> clubs;for(const auto& manager:state.world.managers)clubs.push_back(manager.club);
    for(auto id:extra_clubs)if(std::find(clubs.begin(),clubs.end(),id)==clubs.end())clubs.push_back(id);
    const auto settings=*state.world.settings;auto projection=AiMatchProjection::create_for_desktop(state,clubs,reference,{settings.mode,settings.difficulty,settings.foreign_rule,true});
    auto& local=projection.state;local.season=int(state.season);
    local.memory.add_block(layout::CompetitionSelection,4);local.memory.set32(layout::CompetitionSelection,1|(state.world.catalogue.scope.international?2:0));
    for(size_t i=0;i<projection.human_managers();++i){const auto seat=projection.global_manager(uint8_t(i));
        const auto& desk=*std::find_if(state.desktop->managers.begin(),state.desktop->managers.end(),[&](const auto& d){return d.seat==seat;});
        if(seat==*state.desktop->active_seat)local.memory.set8(layout::ActiveManager,uint8_t(i));
        local.manager_records.at(i*0xb0c+0x211)=desk.prestige;
        if(state.transfers){const auto value=uint32_t(state.transfers->balance[seat]);for(size_t b=0;b<4;++b)local.manager_records.at(i*0xb0c+0x330+b)=uint8_t(value>>(8*b));local.manager_records.at(i*0xb0c+0x21d)=uint8_t(std::min<size_t>(25,state.transfers->history[seat].size()));}
        local.memory.add_block(layout::CareerSession_AnyOf_ExecutableField+uint32_t(i*2),2);
        local.memory.set16(layout::CareerSession_AnyOf_ExecutableField+uint32_t(i*2),desk.display_word);
        for(size_t r=0;r<54;++r)for(size_t b=0;b<16;++b)local.memory.set8(layout::Desktops+uint32_t(i*0x360+r*16+b),desk.layout[r][b]);
    }
    local.memory.add_block(layout::OfferSlots,16);for(uint32_t i=0;i<4;++i)local.memory.set32(layout::OfferSlots+i*4,0xffffffff);
    if(state.desktop->initial_event&&state.desktop->initial_event->offer){const auto& notice=*state.desktop->initial_event->offer;const auto& row=*state.world.transfer_market->rows[notice.row];
        uint32_t manager=0;while(projection.global_manager(uint8_t(manager))!=notice.seat)++manager;
        local.memory.set32(layout::OfferSlots+uint32_t(notice.row-40)*4,projection.local_player(row.player)*0x400+manager|(notice.warned?0x80000000u:0));
    }
    if(state.world.markets)local.stocks.assign(state.world.markets->stocks.begin(),state.world.markets->stocks.end());
    return projection;
}
}
