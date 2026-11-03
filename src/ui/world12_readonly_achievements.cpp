// SPDX-License-Identifier: GPL-3.0-only
#include "ui/world12_readonly.hpp"
#include "game/world12/desktop_projection.hpp"
#include "game/messages.hpp"
#include "ui/manager.hpp"
#include <algorithm>
namespace bmh::ui {
using namespace bmh::world12;
AchievementsState world12_achievements(const CareerSnapshot& state,const World12ReadonlyIdentity& identity,const GameState& reference,const LeImage& source,const Image& underlay){
    state.validate();const auto found=std::find_if(state.world.managers.begin(),state.world.managers.end(),[&](const auto& manager){return manager.seat==identity.manager;});
    if(found==state.world.managers.end()||found->club!=identity.club||!found->career||!found->career->season_records||!found->career->first_recorded_season)throw FormatError("world12: achievements require actual saved manager history and its year origin");
    const auto actual_league=state.world.clubs.at(identity.club).league;
    const auto current_index=std::min<uint32_t>(state.season,49)*3;
    if(state.league_rounds[actual_league.value]&&((*found->career->season_records)[current_index]==0||(*found->career->season_records)[current_index]==0xfe))throw FormatError("world12: played round has no genuine saved position history; older missing history is unknown");
    const auto country=state.world.clubs.at(identity.club).country;const auto primary=state.world.catalogue.starting_league(country,0);if(!state.league_rounds[primary.value])throw FormatError("world12: achievements are available after the actual country's first league round");
    auto p=project_manager_desktop(state,reference);auto& game=p.state;const auto local=game.memory.u8(layout::ActiveManager);game.league_rounds.resize(9);game.league_rounds[0]=state.league_rounds[primary.value];
    DeskRequest owner;owner.kind=DeskRequestKind::open_menu;owner.menu_kind=4;owner.manager_slot=local;owner.revision=identity.revision;auto out=achievements_from_game(game,source,owner,underlay);out.owner.manager_slot=identity.manager;out.manager_slot=identity.manager;out.manager_team_name=state.world.clubs.at(identity.club).name;
    for(int season=0;season<out.season_count;++season){auto& row=out.seasons[size_t(season)];const auto raw=row.league_result;if(!raw||raw>=0xfe)continue;const auto band=country==Country::england?(raw-1)/20:(raw-1)/18;const auto place=country==Country::england?(raw-1)%20+1:(raw-1)%18+1;
        const auto local_league=country==Country::germany&&band==3?4:band;if(local_league>=5)throw FormatError("world12: saved achievement position has no actual country tier");const auto league=state.world.catalogue.starting_league(country,uint8_t(local_league));if(place>state.world.catalogue.league(league).clubs)throw FormatError("world12: saved achievement rank exceeds its actual league");row.league_text=format_number(game,place,2)+source.cstring(layout::Achievements_FormatError_ExecutableField)+" "+utf8_to_original(state.world.catalogue.league(league).name);
    }
    return out;
}
}
