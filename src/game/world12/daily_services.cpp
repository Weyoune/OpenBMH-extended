// SPDX-License-Identifier: GPL-3.0-only
#include "game/world12/daily_services.hpp"
#include "game/week.hpp"
#include <limits>
namespace bmh::world12 {
DailyServiceOutcome process_daily_services(CareerSnapshot& state,const GameState& reference,const TextDb& texts,
    const DailyServiceContext& context,const DailyServiceHooks& hooks){
    state.validate();const auto epoch=calendar_epoch();
    if(!state.world.catalogue.scope.club_play()||context.previous_date==std::numeric_limits<uint32_t>::max()||
       state.date!=context.previous_date+1||state.season!=context.season||!context.entry||
       context.epoch.year!=epoch.year||context.epoch.weekday!=epoch.weekday)
        throw FormatError("world12: stale or unqualified daily service clock");
    const auto before=state;auto candidate=state;DailyServiceOutcome out;
    const auto finance=daily_finance(candidate,reference,*context.entry);
    out.game_draws+=finance.game_draws;out.watcom_draws+=finance.watcom_draws;
    const auto previous_weekday=weekday_code(context.previous_date);
    out.trained=previous_weekday==2||previous_weekday==6;
    out.weekly=out.trained&&weekday_code(candidate.date)==3;
    if(out.weekly){const auto weekly=process_weekly_services(candidate,reference,texts,context.weekly_relations,hooks.incidents,hooks.auctions,hooks.bids);
        out.game_draws+=weekly.game_draws;out.watcom_draws+=weekly.watcom_draws;
    }else if(out.trained){const auto training=train_manager_squads(candidate,reference);out.game_draws+=training.game_draws;out.watcom_draws+=training.watcom_draws;}
    out.monthly=month_code(context.previous_date)!=month_code(candidate.date);
    if(out.monthly){const auto month=process_monthly_services(candidate,reference,texts,hooks.listings,hooks.loans,hooks.contracts);
        out.game_draws+=month.game_draws;out.watcom_draws+=month.watcom_draws;}
    out.season_end=out.monthly&&month_code(candidate.date)==7;
    out.post_event_stock_pending=*context.entry==DailyEconomyKind::after_event&&!out.season_end;
    candidate.validate();if(state!=before)throw FormatError("world12: daily service owner changed during processing");
    state=std::move(candidate);return out;
}
}
