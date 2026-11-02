// SPDX-License-Identifier: GPL-3.0-only
#include "app/world12_session.hpp"
#include "app/bank_request_source.hpp"
#include "game/world12/desktop_projection.hpp"
#include "game/world12/economy.hpp"
#include "formats/textd.hpp"
#include <limits>
namespace bmh::app::world12 {
void Session::apply_bank(const std::filesystem::path& data,const ui::BankRequest& request) {
    if(preparation_menu_!=14||owner_.revision==std::numeric_limits<uint64_t>::max())
        throw FormatError("world12: bank command has no current menu owner");
    if(!state_.world.economy||!state_.desktop||!state_.desktop->active_seat||
       !state_.world.economy->managers[*state_.desktop->active_seat].depreciation)
        throw FormatError("world12: old career lacks its depreciation account; start a current fresh career");
    const auto before=state_;const auto revision=owner_.revision;auto candidate=state_;
    auto projection=bmh::world12::project_manager_desktop(candidate,resources_.reference);auto& local=projection.state;
    const auto texts=load_textd(ui::asset_path(data,"TEXTD.BLK"));local.texts=&texts;
    GameRandom game(candidate.game_seed);WatcomRandom random(candidate.watcom_seed);
    BankSourcePresentation presentation{bank_tip_,{}, {}};auto scratch=revision;std::string error;
    if(!apply_bank_request_source(local,resources_.german,game,random,request,scratch,presentation,error))
        throw FormatError(error.empty()?"world12: bank request rejected":error);
    projection.commit_match_state(candidate.world);bmh::world12::capture_career_economy(candidate.world,projection);
    bmh::world12::capture_career_portfolios(candidate.world,projection);
    candidate.game_seed=game.seed();candidate.watcom_seed=random.seed();candidate.validate();
    if(state_!=before||owner_.revision!=revision||preparation_menu_!=14)
        throw FormatError("world12: bank owner changed during calculation");
    state_=std::move(candidate);++owner_.revision;bank_tip_=presentation.last_tip;
    bank_property_notice_=std::move(presentation.property_notice);bank_credit_notice_=std::move(presentation.credit_notice);
}
}
