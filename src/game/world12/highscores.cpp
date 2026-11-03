// SPDX-License-Identifier: GPL-3.0-only
#include "game/world12/highscores.hpp"
#include "game/world12/economy.hpp"
#include <algorithm>
namespace bmh::world12 {
OwnedHighscoreProjection project_owned_highscores(const CareerSnapshot& state,const GameState& reference,
    const HighscoreTable& existing) {
    state.validate();if(!state.world.highscore_signature)throw FormatError("world12: saved highscore signature is unknown");const auto signature=*state.world.highscore_signature;const auto before=state;OwnedHighscoreProjection out;out.signature=signature;out.ranked=signature!=0;out.rows=existing;
    if(!out.ranked)return out;
    if(!state.world.settings||!state.world.economy||!state.world.markets)throw FormatError("world12: ranked highscore has unknown actual accounts/portfolios");
    std::vector<ClubId> clubs;for(const auto& manager:state.world.managers){if(!manager.career||!manager.career->season_records||!manager.career->annual_records||!manager.career->cup_titles)throw FormatError("world12: ranked highscore has unknown actual title/history backing");clubs.push_back(manager.club);}
    if(clubs.empty())throw FormatError("world12: ranked highscore has no actual manager");
    const auto& settings=*state.world.settings;auto projection=AiMatchProjection::create_for_humans(state,clubs,reference,{settings.mode,settings.difficulty,settings.foreign_rule,true});install_career_economy(projection,state.world);
    auto& local=projection.state;local.memory.add_block(layout::CareerSession_FormatError_RuntimeField7,2);local.memory.set16(layout::CareerSession_FormatError_RuntimeField7,signature);
    for(uint8_t manager=0;manager<projection.human_managers();++manager){OwnedHighscore result;result.manager=projection.global_manager(manager);result.club=projection.global_club(uint32_t(local.manager_teams[manager]));result.score=highscore_score(local,manager,HighscoreContext::OwnedManagerAssets);result.candidate=highscore_candidate(local,manager,HighscoreContext::OwnedManagerAssets);insert_highscore(out.rows,result.candidate);out.managers.push_back(std::move(result));}
    if(state!=before)throw FormatError("world12: ranked highscore owner changed during projection");return out;
}
}
