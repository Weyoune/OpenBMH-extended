// SPDX-License-Identifier: GPL-3.0-only
#include "game/world12/monthly_services.hpp"
namespace bmh::world12 {
MonthlyServicesOutcome process_monthly_services(CareerSnapshot& state,const GameState& reference,const TextDb& texts,
    const MonthlyMarketPolicy& market,const LoanReturnHooks& loans,const MonthlyContractHooks& contracts){
    state.validate();const auto before=state;auto candidate=state;MonthlyServicesOutcome out;
    expire_monthly_sponsors(candidate,reference,texts);
    const auto listed=renew_monthly_market(candidate,reference,texts,market);
    out.watcom_draws+=listed.watcom_draws;out.listings=listed.added;
    const auto returned=return_monthly_loans(candidate,reference,texts,loans);
    out.game_draws+=returned.game_draws;out.watcom_draws+=returned.watcom_draws;out.loan_returns=returned.returned;
    for(const auto& manager:before.world.managers){const auto roster=settle_monthly_roster(candidate,manager.seat,reference,texts,contracts);
        out.game_draws+=roster.game_draws;out.watcom_draws+=roster.watcom_draws;out.released+=roster.released;++out.managers;}
    settle_monthly_investments(candidate,reference);candidate.validate();
    if(state!=before)throw FormatError("world12: monthly services owner changed during processing");
    state=std::move(candidate);return out;
}
}
