// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <cstdint>
#include <array>

#include "game/random.hpp"
#include "game/human_buyer_sale.hpp"
#include "game/state.hpp"

namespace bmh {
struct MinuteReport;

// Scalar input to the Original candidate score. Identity mapping, eligibility,
// foreign-skip order and sorting belong to the owning transfer operation.
struct TransferClubScoreInput {
    std::array<int,4> averages{},counts{};
    int nationality=0,region=0,position=0,player_strength=0,players=0,need=0,entry40_bids=0;
    uint16_t flags=0;
    bool large_squad=false,domestic_capacity=false;
};
int transfer_club_score(const TransferClubScoreInput&,WatcomRandom&);

// ProjectTransferOfferList rates an existing bid. source_slot is the actual absolute SquadSlots
// index supplied by its caller, not an inferred pool index or player ID.
// The caller owns list/source-slot correspondence and any career-mode decision.
struct TransferBidRatingRequest {
    int source_slot = -1;
    int list_entry = -1;
    int bid_slot = -1;
    int expected_bidder = -1;
    int expected_player = -1;
    // Only checked DispatchClubPlayerPendingOffers entries40..43 may rate parked native buyer4.
    std::optional<SourcePendingOwner> source_pending_owner = std::nullopt;
};
struct TransferBidRating {
    bool supported = false;
    uint32_t fee_pct = 0;
    uint32_t salary_pct = 0;
};

// Exact RateTransferBidSource -> ProjectTransferOfferList composition: Source32 products/unsigned division,
// proposed human board with all seven bonuses/rankFF/fitness, then one W90..100.
// Does not write bids, perform settlement, read GameMode or consume G. Any borrowed
// board is restored before returning or propagating a validation failure.
// Unsupported inputs require caller-owned staging/rollback; no zero-basis guess.
TransferBidRating rate_transfer_bid_source(GameState&, const TransferBidRatingRequest&,
                                           WatcomRandom&, MinuteReport&);
} // namespace bmh
