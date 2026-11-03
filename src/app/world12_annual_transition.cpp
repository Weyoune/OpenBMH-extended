// SPDX-License-Identifier: GPL-3.0-only
#include "app/world12_annual_transition.hpp"
#include "game/world12/annual_archive.hpp"
#include "game/world12/annual_moods.hpp"
#include "game/world12/annual_fields.hpp"
#include "game/world12/annual_players.hpp"
#include "game/world12/annual_opening.hpp"
#include "game/world12/annual_calendar.hpp"
#include "game/world12/regional_playoff.hpp"
#include "game/world12/national_coach.hpp"
#include "game/world12/national.hpp"
#include "game/world12/inactive_national_draw.hpp"
#include "game/week.hpp"
#include <algorithm>
#include <fstream>
#include <iterator>
#include <limits>
#include <type_traits>
namespace bmh::app::world12 {
using namespace bmh::world12;
namespace {
struct ScoreFile {bool present=false;Bytes bytes;HighscoreTable rows;};
ScoreFile read_score(const std::filesystem::path& path) {
    ScoreFile out;if(path.empty())throw FormatError("world12: ranked annual highscore has no configured file owner");
    std::error_code error;out.present=std::filesystem::exists(path,error);if(error)throw FormatError("world12: cannot inspect annual highscore file");
    if(!out.present)return out;std::ifstream input(path,std::ios::binary);if(!input)throw FormatError("world12: cannot read owned annual highscore file");out.bytes=Bytes(std::istreambuf_iterator<char>(input),{});
    if(out.bytes.size()!=360)throw FormatError("world12: owned annual highscore file has invalid size");HighscoreBytes raw;std::copy(out.bytes.begin(),out.bytes.end(),raw.begin());out.rows=decode_highscore_table(raw);return out;
}
void commit_score(const std::filesystem::path& path,const ScoreFile& before,const HighscoreTable& rows,uint64_t scope,uint64_t revision) {
    const auto now=read_score(path);if(now.present!=before.present||now.bytes!=before.bytes)throw FormatError("world12: annual highscore file owner changed");
    const auto bytes=encode_highscore_table(rows);auto temporary=path;temporary+=".annual-"+std::to_string(scope)+"-"+std::to_string(revision)+".tmp";auto backup=temporary;backup+=".bak";
    if(std::filesystem::exists(temporary)||std::filesystem::exists(backup))throw FormatError("world12: annual highscore transaction path is already owned");
    if(!path.parent_path().empty())std::filesystem::create_directories(path.parent_path());
    try {
        {std::ofstream output(temporary,std::ios::binary);if(!output)throw FormatError("world12: cannot stage owned annual highscore");output.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());output.close();if(!output)throw FormatError("world12: annual highscore staging failed");}
        const auto current=read_score(path);if(current.present!=before.present||current.bytes!=before.bytes)throw FormatError("world12: annual highscore file changed before replacement");
        if(before.present)std::filesystem::rename(path,backup);
        try {std::filesystem::rename(temporary,path);}catch(...){if(before.present)std::filesystem::rename(backup,path);throw;}
        std::error_code ignored;if(before.present)std::filesystem::remove(backup,ignored);
    }catch(...){std::error_code ignored;std::filesystem::remove(temporary,ignored);throw;}
}
}
AnnualTransitionOutcome publish_annual_transition(CareerSnapshot& state,ManagerActionOwner& owner,ManagerActionOwner expected,
    const GameState& reference,const LeImage& german,const LeImage& english,const TextDb& texts,
    const std::filesystem::path& score_file,const AnnualTransitionHooks& hooks) {
    static_assert(std::is_nothrow_move_assignable_v<CareerSnapshot>);
    const auto owned=[&]{return owner.scope&&owner.scope==expected.scope&&owner.revision==expected.revision&&owner.revision!=std::numeric_limits<uint64_t>::max();};
    if(!owned())throw FormatError("world12: stale annual publication owner");state.validate();
    if(!state.progress||state.progress->phase!=ProgressPhase::annual_transition||!state.desktop||state.desktop->active_seat||state.desktop->initial_event||state.desktop->autosave_pending||!state.world.settings||!state.world.settings->duration||!state.world.highscore_signature||state.season==UINT32_MAX||state.calendar->revision==UINT64_MAX)
        throw FormatError("world12: actual closing/length/signature owner is not ready for annual publication");
    const auto before=state;auto candidate=state;const uint32_t target=state.season+1;AnnualTransitionOutcome out;
    const auto unchanged=[&]{if(state!=before||!owned())throw FormatError("world12: annual owner changed during Source presentation");};
    const auto archive=stage_annual_title_archive(state);
    std::optional<ScoreFile> score;if(*state.world.highscore_signature)score=read_score(score_file);
    const auto moods=stage_annual_moods(candidate,reference);candidate=moods.snapshot;out.game_draws+=moods.game_draws;out.watcom_draws+=moods.watcom_draws;unchanged();
    {
        GameRandom game(candidate.game_seed);WatcomRandom random(candidate.watcom_seed);
        const auto draw=candidate.world.catalogue.scope.international?prepare_national_campaign(candidate.world,german,target,game,random):advance_inactive_national_draw(candidate.world,target,game,random);
        candidate.game_seed=game.seed();candidate.watcom_seed=random.seed();out.game_draws+=draw.game_draws;out.watcom_draws+=draw.watcom_draws;
        if(candidate.world.catalogue.scope.international){if(!hooks.national_draw||!hooks.national_draw(candidate.world.national_campaigns->seasons.at(target)))throw FormatError("world12: annual national draw needs its real presenter");unchanged();}
    }
    const auto qualification=stage_annual_qualification(candidate);out.game_draws+=qualification.game_draws;out.watcom_draws+=qualification.watcom_draws;
    if(!hooks.qualification||!hooks.qualification(qualification))throw FormatError("world12: actual next-European entrants need their presenter");unchanged();
    const auto accounts=stage_annual_accounts(qualification,reference,texts,hooks.tax);candidate=accounts.snapshot;out.game_draws+=accounts.game_draws;out.watcom_draws+=accounts.watcom_draws;out.academy=accounts.academy.size();unchanged();
    auto market=hooks.market;market.active_seat=candidate.progress->last_manager;
    const auto settled=process_market_auctions(candidate,reference,texts,market,true);out.game_draws+=settled.game_draws;out.watcom_draws+=settled.watcom_draws;unchanged();
    // Completed historical physical namespaces stay retained by before/accounts.
    // None is relabelled under the new membership while it is unpublished.
    candidate.world.indoor.reset();candidate.world.regional_playoff.reset();
    std::vector<AnnualPromotionOutcome> promotions;
    for(auto country:{Country::germany,Country::england})if(candidate.world.catalogue.selectable(country)) {
        if(!candidate.world.amateur_slots)throw FormatError("world12: annual amateur physical order is unknown");AnnualDecisionContext context{{country,(*candidate.world.amateur_slots)[country==Country::germany?0:1]},RegionalPromotion::not_applicable,{}};
        if(country==Country::germany){context.regional=RegionalPromotion::playoff_complete;context.regional_winners=annual_regional_qualifiers(before);}
        auto decisions=evaluate_annual_promotion_decisions(candidate,context);
        for(size_t group=0;group<decisions.plan.groups.size();++group){if(!hooks.promotion||!hooks.promotion({country,group,&decisions.plan.groups[group]}))throw FormatError("world12: actual annual promotion needs its presenter");unchanged();}
        const auto membership=stage_annual_membership(candidate,decisions,texts);candidate.world=membership.world;candidate.game_seed=membership.game_seed;candidate.watcom_seed=membership.watcom_seed;out.game_draws+=decisions.game_draws;out.watcom_draws+=decisions.watcom_draws;promotions.push_back(std::move(decisions));
    }
    for(auto& manager:candidate.world.managers)manager.match_state->point_deduction=accounts.pending_sanctions[manager.seat];
    const auto fields=stage_annual_fields(candidate,qualification,german,english);candidate.world=fields.world;candidate.cups=fields.cups;candidate.game_seed=fields.game_seed;candidate.watcom_seed=fields.watcom_seed;out.game_draws+=fields.game_draws;out.watcom_draws+=fields.watcom_draws;
    for(const auto& draw:fields.draws){if(!hooks.cup_draw||!hooks.cup_draw(draw))throw FormatError("world12: actual annual cup draw needs its presenter");unchanged();}
    const auto players=stage_annual_player_renewal(candidate,qualification,promotions,reference,texts);candidate.world=players.world;candidate.desktop=players.desktop;candidate.transfers=players.transfers;candidate.results=players.results;candidate.league_rounds=players.league_rounds;candidate.game_seed=players.game_seed;candidate.watcom_seed=players.watcom_seed;out.game_draws+=players.game_draws;out.watcom_draws+=players.watcom_draws;out.retired=players.released;unchanged();
    if(players.inactive_origin_refresh_required)throw FormatError("world12: actual inactive population refresh is incomplete");
    begin_annual_manager_history(candidate.world,target);candidate.season=target;candidate.world.annual_title_archive=archive;candidate.calendar.reset();candidate.progress.reset();candidate.prepared_event.reset();
    out.highscore=project_owned_highscores(candidate,reference,score?score->rows:HighscoreTable{});
    if(!hooks.highscore||!hooks.highscore(out.highscore,false))throw FormatError("world12: annual highscore update needs its actual owner");unchanged();
    if(candidate.world.settings->duration<5)--candidate.world.settings->duration;out.career_complete=candidate.world.settings->duration==0;
    if(out.career_complete){if(!hooks.highscore(out.highscore,true))throw FormatError("world12: completed career needs its actual final score presenter");unchanged();}
    else {
        if(candidate.world.catalogue.scope.international){if(!candidate.world.national_coaches)throw FormatError("world12: annual coach account context is unknown");activate_national_coaches(candidate.world,candidate.desktop->manager_order);}
        MonthlyMarketPolicy listing;for(const auto& squad:candidate.world.national_squads)for(const auto& row:squad.roster)if(!row.empty())listing.national_squad.insert(row.player);
        const auto opening=stage_annual_opening_services(candidate,reference,texts,listing);candidate=opening.snapshot;out.game_draws+=opening.game_draws;out.watcom_draws+=opening.watcom_draws;
        const auto calendar=make_annual_calendar(candidate.world.catalogue,german,english,candidate.date);candidate.calendar=CalendarCursor{calendar,0,before.calendar->revision+1};
        if(candidate.world.catalogue.scope.germany){candidate.world.indoor=make_annual_indoor_season(candidate.world,before,german);candidate.world.regional_playoff=make_regional_playoff_season(candidate.world,german,calendar,target);}
    }
    candidate.validate();unchanged();if(score)commit_score(score_file,*score,out.highscore.rows,owner.scope,owner.revision);
    state=std::move(candidate);++owner.revision;return out;
}
}
