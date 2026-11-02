// SPDX-License-Identifier: GPL-3.0-only
#include "ui/world12_preparation.hpp"
#include "ui/world12_desktop.hpp"
#include "game/world12/desktop_projection.hpp"
#include "formats/textd.hpp"
#include <algorithm>
namespace bmh::ui {
PreparationState world12_preparation(const std::filesystem::path& data,
    const bmh::world12::CareerSnapshot& state,const GameState& reference,const LeImage& rules,SetupOptions options,uint64_t revision) {
    auto projection=bmh::world12::project_manager_desktop(state,reference);auto& local=projection.state;
    const auto texts=load_textd(asset_path(data,"TEXTD.BLK"));local.texts=&texts;
    PreparationState result;result.desktop=world12_initial_desktop(data,state,reference,rules,options,revision);
    result.lineup=lineup_from_game(local,rules,revision);result.lineup.source="world12";result.lineup.entry_prepared=true;
    result.tactics=tactics_from_game(local,rules,revision);result.tactics.source="world12";
    if(options.mode>=2){result.training=training_from_game(local,rules,revision);result.training->source="world12";}
    result.pitch=pitch_extras_from_game(local,rules,revision);result.pitch->source="world12";
    result.pitch->badge=result.desktop.badge;result.pitch->badge_atlas=result.desktop.badge_atlas;
    result.bank=bank_from_game(local,rules,revision);result.bank->source="world12";
    result.bank->badge_sprite=result.desktop.badge;result.bank->badge_atlas=result.desktop.badge_atlas;
    const auto actor=*state.desktop->active_seat;
    const auto& manager=*std::find_if(state.world.managers.begin(),state.world.managers.end(),[&](const auto& m){return m.seat==actor;});
    if(manager.career&&manager.career->camp_focus) {
        result.training_camp=training_camp_from_game(local,rules,result.desktop.manager_slot,revision);
        result.training_camp->source="world12";result.training_camp->badge=result.desktop.badge;result.training_camp->badge_atlas=result.desktop.badge_atlas;
    }
    for(const auto& player:result.lineup.players)if(player.player_id>=0){
        auto card=player_card_from_game(local,rules,player.row,revision);card.lineup=result.lineup;
        card.badge=result.desktop.badge;card.badge_atlas=result.desktop.badge_atlas;result.player_cards.push_back(std::move(card));
    }
    validate_preparation_context(result);return result;
}
}
