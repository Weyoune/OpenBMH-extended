// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/market_auctions.hpp"
#include "game/human_buyer_sale.hpp"
namespace bmh::world12 {
struct LoanReturnHooks {
    std::optional<uint8_t> active_seat;
    std::function<std::optional<bool>(const GameState&,uint8_t global_borrower,const std::string&,const AiMatchProjection&)> keep;
    // Exact Source return-offer editor owner. May perform a real shared sale
    // or loan on the staged numeric state; absent is unsupported, never No.
    std::function<int(GameState&,GameRandom&,WatcomRandom&,uint8_t global_borrower,
        int source_slot,int entry,const HumanBuyerSaleHooks&,const AiMatchProjection&)> offer;
};
struct LoanReturnOutcome {uint64_t game_draws=0,watcom_draws=0;size_t visited=0,returned=0,retained=0;};
LoanReturnOutcome return_monthly_loans(CareerSnapshot&,const GameState& reference,
    const TextDb&,const LoanReturnHooks&);
}
