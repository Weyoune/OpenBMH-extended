// SPDX-License-Identifier: GPL-3.0-only
#include "ui/world12_desktop.hpp"
#include "game/world12/match_projection.hpp"
#include "formats/textd.hpp"
#include <algorithm>
namespace bmh::ui {
DeskState world12_initial_desktop(const std::filesystem::path& data,const bmh::world12::CareerSnapshot& state,
    const GameState& reference,const LeImage& rules,SetupOptions options,uint64_t revision) {
    using namespace bmh::world12;state.validate();
    if(!state.desktop||!state.desktop->initial_event||!state.desktop->active_seat||!state.world.settings||!options.country_selection)
        throw FormatError("world12: initial desktop event is unavailable");
    std::vector<ClubId> clubs;for(const auto& m:state.world.managers)clubs.push_back(m.club);
    const auto settings=*state.world.settings;
    auto projection=AiMatchProjection::create_for_humans(state,clubs,reference,{settings.mode,settings.difficulty,settings.foreign_rule,true});
    auto& local=projection.state;const auto texts=load_textd(asset_path(data,"TEXTD.BLK"));local.texts=&texts;
    local.memory.set32(layout::CompetitionSelection,1|(state.world.catalogue.scope.international?2:0));
    for(size_t i=0;i<projection.human_managers();++i){const auto seat=projection.global_manager(uint8_t(i));
        const auto& desk=*std::find_if(state.desktop->managers.begin(),state.desktop->managers.end(),[&](const auto& d){return d.seat==seat;});
        if(seat==*state.desktop->active_seat)local.memory.set8(layout::ActiveManager,uint8_t(i));
        local.manager_records.at(i*0xb0c+0x211)=desk.prestige;
        for(size_t r=0;r<54;++r)for(size_t b=0;b<16;++b)local.memory.set8(layout::Desktops+uint32_t(i*0x360+r*16+b),desk.layout[r][b]);
    }
    if(state.world.markets)local.stocks.assign(state.world.markets->stocks.begin(),state.world.markets->stocks.end());
    auto desk=desk_from_game(local,rules,revision,nullptr,33,nullptr,&options);
    for(size_t i=0;i<desk.managers.size();++i){const auto seat=projection.global_manager(uint8_t(i));const auto& manager=*std::find_if(state.world.managers.begin(),state.world.managers.end(),[&](const auto& m){return m.seat==seat;});const auto& club=state.world.clubs.at(manager.club);
        desk.managers[i].world_club=manager.club;desk.managers[i].badge=state.world.crests.at(club.crest).cell;
    }
    const auto id=desk.managers.at(size_t(desk.manager_slot)).world_club;
    const auto& crest=state.world.crests.at(state.world.clubs.at(id).crest);desk.badge=crest.cell;desk.badge_atlas=crest.path;
    desk.rank_curves[0].league=desk.league;desk.source="world12";validate_desk_state(desk);return desk;
}
}
