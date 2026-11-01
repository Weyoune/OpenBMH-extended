// SPDX-License-Identifier: GPL-3.0-only
#include "game/world12/weekly_services.hpp"
#include <stdexcept>
namespace bmh::world12 {
WeeklyServiceOutcome process_weekly_services(CareerSnapshot& state,const GameState& reference,
    const TextDb& texts,const WeeklyIncidentContext& context,const WeeklyIncidentHooks& incidents,
    const MarketAuctionHooks& auctions,MarketBidPolicy policy) {
    state.validate();const auto before=state;auto candidate=state;WeeklyServiceOutcome result;
    result.incidents=begin_weekly_managers(candidate,reference,texts,context,incidents);
    result.auctions=process_market_auctions(candidate,reference,texts,auctions);
    result.bids=generate_weekly_market_bids(candidate,reference,policy);
    result.players=update_weekly_players(candidate,reference,texts);
    result.settlement=settle_weekly_managers(candidate,reference);
    result.training=train_manager_squads(candidate,reference);
    result.game_draws=result.incidents.game_draws+result.auctions.game_draws+result.bids.game_draws+
        result.players.game_draws+result.settlement.game_draws+result.training.game_draws;
    result.watcom_draws=result.incidents.watcom_draws+result.auctions.watcom_draws+result.bids.watcom_draws+
        result.players.watcom_draws+result.settlement.watcom_draws+result.training.watcom_draws;
    candidate.validate();if(state!=before)throw std::invalid_argument("world12: weekly service owner changed during processing");
    state=std::move(candidate);return result;
}
}
