// SPDX-License-Identifier: GPL-3.0-only
#include "ui/world12_desktop.hpp"
#include "game/world12/desktop_projection.hpp"
#include "formats/textd.hpp"
#include <algorithm>
namespace bmh::ui {
DeskState world12_initial_desktop(const std::filesystem::path& data,const bmh::world12::CareerSnapshot& state,
    const GameState& reference,const LeImage& rules,SetupOptions options,uint64_t revision) {
    using namespace bmh::world12;state.validate();
    if(!state.desktop||!state.desktop->initial_event||!state.desktop->active_seat||!state.world.settings||!options.country_selection)
        throw FormatError("world12: initial desktop event is unavailable");
    auto projection=project_manager_desktop(state,reference);auto& local=projection.state;
    const auto texts=load_textd(asset_path(data,"TEXTD.BLK"));local.texts=&texts;
    auto desk=desk_from_game(local,rules,revision,nullptr,33,nullptr,&options);
    for(size_t i=0;i<desk.managers.size();++i){const auto seat=projection.global_manager(uint8_t(i));const auto& manager=*std::find_if(state.world.managers.begin(),state.world.managers.end(),[&](const auto& m){return m.seat==seat;});const auto& club=state.world.clubs.at(manager.club);
        desk.managers[i].world_club=manager.club;desk.managers[i].badge=state.world.crests.at(club.crest).cell;
    }
    const auto id=desk.managers.at(size_t(desk.manager_slot)).world_club;
    const auto& crest=state.world.crests.at(state.world.clubs.at(id).crest);desk.badge=crest.cell;desk.badge_atlas=crest.path;
    desk.rank_curves[0].league=desk.league;desk.source="world12";validate_desk_state(desk);return desk;
}
}
