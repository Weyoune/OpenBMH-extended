// SPDX-License-Identifier: GPL-3.0-only
#include "game/world12/annual_opening.hpp"
#include "game/world12/economy.hpp"
#include "game/world12/weekly_players.hpp"
#include "game/turn.hpp"
#include "game/week.hpp"
#include <algorithm>
#include <set>
namespace bmh::world12 {
bool annual_closing_ready(const CareerSnapshot& state) {
    GameState date;date.date=state.date;
    if(!state.world.settings||!state.calendar||!state.progress||state.progress->phase!=ProgressPhase::season_end||!is_season_end(date)||state.prepared_event||
       !state.world.annual_awards_completed||uint64_t(*state.world.annual_awards_completed)!=uint64_t(state.season)+1||!state.desktop||state.world.managers.empty()||state.desktop->manager_order.size()!=state.world.managers.size())return false;
    for(const auto& league:state.world.catalogue.leagues)if(league.active){if(state.league_rounds[league.id.value]!=league.rounds()||std::any_of(state.results.leagues[league.id.value].begin(),state.results.leagues[league.id.value].end(),[](const auto& result){return !result.played;}))return false;}
    std::set<uint8_t> order;for(auto seat:state.desktop->manager_order)if(!order.insert(seat).second||std::none_of(state.world.managers.begin(),state.world.managers.end(),[&](const auto& manager){return manager.seat==seat;}))return false;
    return true;
}
bool annual_opening_ready(const CareerSnapshot& state) {
    if(!state.season||state.date<7||weekday_code(state.date)!=2||month_code(state.date)!=7||month_code(state.date-7)==7||!state.world.settings||!state.world.settings->duration||
       !state.world.annual_awards_completed||*state.world.annual_awards_completed!=state.season||!state.world.annual_title_archive||state.world.annual_title_archive->seasons.empty()||
       uint64_t(state.world.annual_title_archive->seasons.back().season)+1!=state.season||state.progress||state.prepared_event||!state.calendar||state.calendar->next_day||
       !state.calendar->schedule.leagues_on(0).empty()||!state.calendar->schedule.others_on(0).empty()||!state.desktop||state.desktop->manager_order.size()!=state.world.managers.size()||state.world.managers.empty())return false;
    if(std::any_of(state.league_rounds.begin(),state.league_rounds.end(),[](auto round){return round!=0;}))return false;
    for(const auto& league:state.results.leagues)if(std::any_of(league.begin(),league.end(),[](const auto& result){return result.played;}))return false;
    std::set<uint8_t> order;for(auto seat:state.desktop->manager_order)if(!order.insert(seat).second||std::none_of(state.world.managers.begin(),state.world.managers.end(),[&](const auto& manager){return manager.seat==seat;}))return false;
    return order.size()==state.world.managers.size();
}
AnnualOpeningStage stage_annual_opening_services(const CareerSnapshot& source,const GameState& reference,
    const TextDb& texts,const MonthlyMarketPolicy& policy) {
    source.validate();const auto before=source;GameState date;date.date=source.date;
    if(!source.season||!source.world.settings||!source.world.settings->duration||!is_season_end(date)||source.calendar||source.progress||source.prepared_event||
       std::any_of(source.league_rounds.begin(),source.league_rounds.end(),[](auto round){return round!=0;})||
       std::any_of(source.results.leagues.begin(),source.results.leagues.end(),[](const auto& rows){return std::any_of(rows.begin(),rows.end(),[](const auto& score){return score.played;});}))
        throw FormatError("world12: annual opening services need an owned quiescent new year on1July");
    AnnualOpeningStage out;out.snapshot=source;auto& candidate=out.snapshot;
    while(weekday_code(candidate.date)!=2) {
        if(out.days==6)throw FormatError("world12: first-Wednesday annual service bound exceeded");const auto previous=candidate.date++;
        // Source SeasonEndSteps calls DailyInterest directly. It does not run
        // plain_day's stock phase on these days.
        const auto interest=daily_finance(candidate,reference,DailyEconomyKind::after_event);
        out.game_draws+=interest.game_draws;out.watcom_draws+=interest.watcom_draws;++out.days;
        const auto weekday=weekday_code(previous);
        if(weekday==2||weekday==6) {
            // A valid first-Wednesday loop never crosses Wednesday→Thursday:
            // it stops on Wednesday. Likewise1..7July cannot cross a month.
            if(weekday_code(candidate.date)==3)throw FormatError("world12: annual opening crossed its Source stop weekday");
            const auto trained=train_manager_squads(candidate,reference);out.game_draws+=trained.game_draws;out.watcom_draws+=trained.watcom_draws;++out.training_passes;
        }
        if(month_code(previous)!=month_code(candidate.date))throw FormatError("world12: annual opening crossed its Source July boundary");
    }
    const auto market=renew_monthly_market(candidate,reference,texts,policy);out.watcom_draws+=market.watcom_draws;out.new_listings=market.added;
    candidate.validate();if(source!=before)throw FormatError("world12: annual opening owner changed during services");
    // Keep the exact immutable source separately: callbacks/random observers may
    // change the external owner while the unpublished candidate is prepared.
    return out;
}
}
