// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/identity.hpp"
#include "ui/outgoing_transfer.hpp"
#include "ui/transfer_market.hpp"
#include <variant>
namespace bmh::ui {
struct World12MarketIdentity {
    uint8_t manager = 0;
    bmh::world12::ClubId club;
    uint64_t revision = 0, scope = 0, sequence = 0;
    bool operator==(const World12MarketIdentity&) const = default;
};
using World12MarketInput = std::variant<std::monostate, TransferMarketRequest,
                                        OutgoingTransferRequest>;
struct World12MarketRequest {
    World12MarketIdentity identity;
    World12MarketInput input;
    bool close = false;
    bool operator==(const World12MarketRequest& other) const {
        if (identity != other.identity || close != other.close ||
            input.index() != other.input.index())
            return false;
        if (const auto* value = std::get_if<TransferMarketRequest>(&input)) {
            const auto& peer = std::get<TransferMarketRequest>(other.input);
            return value->bid.manager == peer.bid.manager &&
                   transfer_market_request_json(*value) ==
                       transfer_market_request_json(peer);
        }
        if (const auto* value = std::get_if<OutgoingTransferRequest>(&input))
            return outgoing_transfer_request_json(*value) ==
                   outgoing_transfer_request_json(
                       std::get<OutgoingTransferRequest>(other.input));
        return true;
    }
};
struct World12MarketState {
    World12MarketIdentity identity;
    TransferMarketState market;
    std::optional<OutgoingTransferEditorState> offer;
    std::optional<TransferMarketPreview> preview;
    std::string notice;
};
} // namespace bmh::ui
