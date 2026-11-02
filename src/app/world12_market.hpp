// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/desk_actions.hpp"
#include "game/human_buyer_sale.hpp"
#include "game/world12/match_projection.hpp"
#include "ui/world12_market.hpp"
namespace bmh {
struct TransferDecisionEvent;
struct TransferOfferListView;
} // namespace bmh
namespace bmh::app::world12 {
struct RuntimeResources;
struct MarketPresenters {
    HumanBuyerSaleHooks sale;
    std::function<void(const GameState&, const TransferDecisionEvent&)> notice;
    std::function<std::optional<int>(const GameState&,
                                     const TransferOfferListView&)>
        select;
};
struct MarketDraft {
    ui::World12MarketIdentity identity;
    bmh::world12::AiMatchProjection projection;
    std::optional<bmh::world12::AiMatchProjection> offer_projection;
    std::optional<TransferOfferEditorState> offer;
    std::optional<ui::TransferMarketPreview> preview;
    std::string notice;
};
MarketDraft prepare_market_draft(const bmh::world12::CareerSnapshot&,
                                 const RuntimeResources&,
                                 ui::World12MarketIdentity,
                                 std::optional<uint8_t> listing = {});
ui::World12MarketState project_market(const bmh::world12::CareerSnapshot&,
                                      const RuntimeResources&,
                                      const MarketDraft&);
} // namespace bmh::app::world12
