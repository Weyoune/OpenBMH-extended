// SPDX-License-Identifier: GPL-3.0-only
#include "game/world12/daily_services.hpp"
#include "game/week.hpp"
#include "game/world12/national_coach.hpp"
#include "game/world12/match_projection.hpp"
#include <algorithm>
#include <limits>
namespace bmh::world12 {
DailyServiceOutcome process_daily_services(CareerSnapshot& state,const GameState& reference,const TextDb& texts,
    const DailyServiceContext& context,const DailyServiceHooks& hooks){
    state.validate();const auto epoch=calendar_epoch();
    if(context.previous_date==std::numeric_limits<uint32_t>::max()||
       state.date!=context.previous_date+1||state.season!=context.season||!context.entry||
       context.epoch.year!=epoch.year||context.epoch.weekday!=epoch.weekday)
        throw FormatError("world12: stale or unqualified daily service clock");
    const auto before=state;auto candidate=state;DailyServiceOutcome out;
    const bool club_play=candidate.world.catalogue.scope.club_play();
    const bool national=national_roles_active(candidate.world);
    const bool training_day=weekday_code(context.previous_date)==2||weekday_code(context.previous_date)==6;
    if(!club_play){
        if(!national)throw FormatError("world12: national-only daily service has no actual national actor");
        std::vector<ClubId> teams;for(const auto& record:*candidate.world.national_coaches)if(record.active)teams.push_back(record.team);
        const auto settings=*candidate.world.settings;auto projection=AiMatchProjection::create_for_nationals(candidate,teams,reference,{settings.mode,settings.difficulty,settings.foreign_rule,true});auto& local=projection.state;
        if(candidate.world.economy)install_career_mailbox(projection,candidate.world);
        GameRandom game(candidate.game_seed);WatcomRandom random(candidate.watcom_seed);
        for(size_t n=0;n<projection.human_managers();++n)manager_daily_interest(local,int(n),random);
        if(training_day){familiarity_decay(local,game,random);for(size_t n=0;n<projection.human_managers();++n)train_squad(local,int(n),game,random);}
        projection.commit_match_state(candidate.world);if(candidate.world.economy)capture_career_mailbox(candidate.world,projection);candidate.game_seed=game.seed();candidate.watcom_seed=random.seed();
        out.game_draws=game.calls();out.watcom_draws=random.calls();out.trained=training_day;out.monthly=month_code(context.previous_date)!=month_code(candidate.date);out.season_end=out.monthly&&month_code(candidate.date)==7;
        candidate.validate();if(state!=before)throw FormatError("world12: national-only daily owner changed");state=std::move(candidate);return out;
    }
    std::vector<std::pair<ClubId,uint8_t>> retained_roles;
    if(national){
        std::vector<ClubId> teams;for(const auto& record:*candidate.world.national_coaches)if(record.active){teams.push_back(record.team);retained_roles.push_back({record.team,*record.coach});}
        if(training_day){const auto settings=*candidate.world.settings;auto projection=AiMatchProjection::create_for_nationals(candidate,teams,reference,{settings.mode,settings.difficulty,settings.foreign_rule,true});GameRandom game(candidate.game_seed);WatcomRandom random(candidate.watcom_seed);familiarity_decay(projection.state,game,random);projection.commit_match_state(candidate.world);candidate.game_seed=game.seed();candidate.watcom_seed=random.seed();out.game_draws+=game.calls();out.watcom_draws+=random.calls();}
        park_national_coaches(candidate.world);
    }
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
    // DayStepRest retakes the same temporarily parked role, without reselection.
    for(const auto& [team,seat]:retained_roles){for(auto& record:*candidate.world.national_coaches)if(record.team==team){record.coach=seat;record.active=true;}for(auto& squad:candidate.world.national_squads)if(squad.team==team)squad.coach=seat;}
    candidate.validate();if(state!=before)throw FormatError("world12: daily service owner changed during processing");
    state=std::move(candidate);return out;
}
}
