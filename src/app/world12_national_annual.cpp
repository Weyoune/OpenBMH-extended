// SPDX-License-Identifier: GPL-3.0-only
#include "app/world12_national_annual.hpp"
#include "game/world12/annual_archive.hpp"
#include "game/world12/national_final_facts.hpp"
#include "game/world12/national_annual_players.hpp"
#include "game/world12/inactive_club_state.hpp"
#include "game/world12/national_setup.hpp"
#include "game/world12/daily_services.hpp"
#include "game/calendar.hpp"
#include "game/week.hpp"
#include <limits>
#include <type_traits>
namespace bmh::app::world12 {
using namespace bmh::world12;
AnnualTransitionOutcome publish_national_annual_transition(CareerSnapshot& state,ManagerActionOwner& owner,ManagerActionOwner expected,const GameState& reference,const LeImage& de,const LeImage& en,const TextDb& texts,const AnnualTransitionHooks& hooks){
    static_assert(std::is_nothrow_move_assignable_v<CareerSnapshot>);const auto owned=[&]{return owner.scope&&owner.scope==expected.scope&&owner.revision==expected.revision&&owner.revision!=std::numeric_limits<uint64_t>::max();};
    if(!owned())throw FormatError("world12: stale national annual owner");state.validate();if(!national_annual_ready(state)||state.season>=32767||state.calendar->revision==UINT64_MAX)throw FormatError("world12: actual national-only closing owners are incomplete");
    const auto before=state;auto candidate=state;const uint32_t target=state.season+1;const auto primary=state.world.manager_country(state.world.managers.front().seat);const auto& source=primary==Country::germany?de:en;AnnualTransitionOutcome result;const auto unchanged=[&]{if(state!=before||!owned())throw FormatError("world12: national annual owner changed during staging/presentation");};
    // There are no club awards/prizes in this defined scope. The actual national
    // award/money owner has already settled its real final. Persist closure once.
    candidate.world.annual_awards_completed=target;const auto archive=stage_annual_title_archive(candidate);unchanged();
    {
        GameRandom game(candidate.game_seed);WatcomRandom random(candidate.watcom_seed);const auto national=prepare_national_campaign(candidate.world,source,target,game,random,primary);candidate.game_seed=game.seed();candidate.watcom_seed=random.seed();result.game_draws+=national.game_draws;result.watcom_draws+=national.watcom_draws;
        const auto& campaign=candidate.world.national_campaigns->seasons.at(target);const bool accepted=hooks.national_draw_owned?hooks.national_draw_owned(candidate,campaign):hooks.national_draw&&hooks.national_draw(campaign);if(!accepted)throw FormatError("world12: actual next national draw requires its presenter");unchanged();
        const auto inactive=advance_inactive_club_state(candidate.world,target,de,en,game,random);candidate.game_seed=game.seed();candidate.watcom_seed=random.seed();result.game_draws+=inactive.first;result.watcom_draws+=inactive.second;unchanged();
    }
    const auto players=stage_national_annual_players(candidate,target,reference,texts);candidate.world=players.world;candidate.game_seed=players.game_seed;candidate.watcom_seed=players.watcom_seed;result.game_draws+=players.game_draws;result.watcom_draws+=players.watcom_draws;result.retired=players.released;unchanged();
    candidate.season=target;candidate.world.annual_title_archive=archive;candidate.results=SeasonResults::empty(candidate.world.catalogue);candidate.league_rounds.fill(0);candidate.cups.clear();candidate.calendar.reset();candidate.progress.reset();candidate.prepared_event.reset();
    result.highscore=project_owned_highscores(candidate,reference);if(!hooks.highscore||!hooks.highscore(result.highscore,false))throw FormatError("world12: actual national score update requires its presenter");unchanged();if(candidate.world.settings->duration<5)--candidate.world.settings->duration;result.career_complete=!candidate.world.settings->duration;
    if(result.career_complete){if(!hooks.highscore(result.highscore,true))throw FormatError("world12: completed national career requires its final score presenter");unchanged();}
    else {
        // Scope policy: only the real nominated national squad/account receives
        // ordinary daily interest/training. No hidden club/rest-of-day services,
        // contracts, tickets, advertising or market renewal are synthesized.
        size_t days=0;while(weekday_code(candidate.date)!=2){if(days++==6)throw FormatError("world12: national annual first-Wednesday bound exceeded");const auto previous=candidate.date++;DailyServiceContext context;context.previous_date=previous;context.season=target;context.epoch=calendar_epoch();context.entry=DailyEconomyKind::plain_day;const auto daily=process_daily_services(candidate,reference,texts,context,{});result.game_draws+=daily.game_draws;result.watcom_draws+=daily.watcom_draws;unchanged();}
        std::vector<uint16_t> words(366);for(uint32_t day=0;day<366;++day){const auto at=layout::CompetitionCalendar+day*2;if(!source.contains(at+1))throw FormatError("world12: actual primary national calendar is missing");auto word=source.u16(at);if((word&0x4f0e)==0x4000)word=0;words[day]=word;}
        const auto aligned=align_source_calendar(words,calendar_epoch_from_executable(source).weekday,2,366);const auto calendar=make_national_only_calendar(candidate.world.catalogue,aligned.entries);candidate.calendar=CalendarCursor{calendar,0,before.calendar->revision+1};CareerProgress progress;progress.phase=ProgressPhase::inspect_calendar;progress.entry=DayEntry::plain_day;progress.previous_date=candidate.date-1;progress.season_start=candidate.date;progress.epoch_year=before.progress->epoch_year;progress.epoch_weekday=before.progress->epoch_weekday;progress.last_manager=before.progress->last_manager;candidate.progress=progress;
    }
    candidate.validate();unchanged();state=std::move(candidate);++owner.revision;return result;
}
}
