// SPDX-License-Identifier: GPL-3.0-only
#include "game/world12/annual_players.hpp"
#include "game/world12/economy.hpp"
#include "game/world12/manager_creation.hpp"
#include "game/world12/market_auctions.hpp"
#include "game/world12/market_bids.hpp"
#include "game/month.hpp"
#include <algorithm>
#include <set>
namespace bmh::world12 {
namespace {
CareerSnapshot expand_population(const CareerSnapshot& source) {
    auto expanded=source;if(!source.world.national_origins||source.world.national_origins->empty())return expanded;
    auto& world=expanded.world;world.catalogue=Catalogue::dual_classic({true,true,true});
    for(size_t league=0;league<kLeagueSlots;++league)if(!source.world.catalogue.leagues[league].active) {
        std::vector<const NationalRosterOrigin*> slots;
        for(const auto& origin:*source.world.national_origins)if(origin.club.league==LeagueId{uint32_t(league)})slots.push_back(&origin);
        std::sort(slots.begin(),slots.end(),[](const auto* a,const auto* b){return a->rank<b->rank;});
        if(slots.size()!=world.catalogue.leagues[league].clubs)throw FormatError("world12: inactive annual population lacks actual physical origin slots");
        auto& members=world.membership[league];members.league={uint32_t(league)};members.clubs.clear();
        for(size_t rank=0;rank<slots.size();++rank){const auto& origin=*slots[rank];if(origin.rank!=rank||!world.clubs.emplace(origin.club.id,origin.club).second)throw FormatError("world12: inactive annual origin order/identity is repeated");members.clubs.push_back(origin.club.id);}
        expanded.league_rounds[league]=0;expanded.results.leagues[league]=SeasonResults::empty(world.catalogue).leagues[league];
    }
    // This is only a numeric roster workspace. Keep every real owner in the
    // source receipt; none of these detached campaign/archive fields is erased
    // from the canonical result.
    world.national_origins.reset();world.european_campaigns.reset();world.indoor.reset();world.regional_playoff.reset();world.annual_title_archive.reset();
    expanded.calendar.reset();expanded.progress.reset();expanded.prepared_event.reset();expanded.cups.clear();expanded.validate();return expanded;
}
World restore_population(const World& source,const World& expanded) {
    if(!source.national_origins||source.national_origins->empty())return expanded;
    auto result=source;result.players=expanded.players;result.managers=expanded.managers;result.economy=expanded.economy;result.markets=expanded.markets;result.free_players=expanded.free_players;result.loans=expanded.loans;result.player_statistics=expanded.player_statistics;result.development_balance=expanded.development_balance;result.trend_rotation=expanded.trend_rotation;
    for(auto& [id,club]:result.clubs)club=expanded.clubs.at(id);
    for(auto& origin:*result.national_origins)origin.club=expanded.clubs.at(origin.club.id);
    result.validate();return result;
}
void forget_loans(CareerSnapshot& state,const std::set<PlayerId>& released) {
    std::erase_if(state.world.loans,[&](const auto& row){return released.contains(row.player);});
    if(state.transfers)std::erase_if(state.transfers->loans,[&](const auto& row){return released.contains(row.player);});
}
void capture_loan_boards(CareerSnapshot& state,const AiMatchProjection& projection) {
    if(!state.transfers)return;const auto& local=projection.state;
    for(auto& loan:state.transfers->loans) {
        if(!loan.source_slot)throw FormatError("world12: annual loan reset lacks an owned physical board");const auto slot=*loan.source_slot;
        const auto player=local.memory.u16(layout::Career_CareerManagersEnd_RuntimeField+uint32_t(slot*2));
        const auto owner=local.memory.u8(layout::ClubInfo_FormatError_RuntimeField2+slot);
        if(player!=projection.local_player(loan.player)||owner>=projection.human_managers()||projection.global_manager(owner)!=loan.manager)
            throw FormatError("world12: annual reset changed a loan's source identity");
        for(size_t b=0;b<loan.board.size();++b)loan.board[b]=local.memory.u8(layout::ClubInfo_FormatError_RuntimeField+uint32_t(slot*0x58+b));
        const auto reference=[&](size_t at){const auto raw=uint16_t(loan.board[at]|uint16_t(loan.board[at+1])<<8)&0xfff;return raw==0xfff?PlayerId{}:projection.global_player(raw);};
        loan.mark=reference(0x44);loan.marked_by=reference(0x46);for(size_t b=0x44;b<0x48;++b)loan.board[b]=0;
    }
}
}
std::map<ClubId,uint8_t> annual_player_ranks(const World& world,const UnpublishedAnnualQualification& qualification,
    const std::vector<AnnualPromotionOutcome>& promotions) {
    validate_annual_qualification(qualification.source_binding,qualification);
    if(world.catalogue!=qualification.source_binding.world.catalogue)throw FormatError("world12: annual player lineage changed catalogue");
    std::map<ClubId,uint8_t> ranks;for(const auto& league:qualification.final_rankings)for(size_t rank=0;rank<league.size();++rank)ranks[league[rank]]=uint8_t(rank);
    auto membership=qualification.source_binding.world.membership;size_t index=0;
    for(auto country:{Country::germany,Country::england})if(world.catalogue.selectable(country)) {
        if(index==promotions.size())throw FormatError("world12: annual player lineage lacks a country exchange receipt");const auto& outcome=promotions[index++];
        if(outcome.plan.decisions.country!=country||outcome.plan.before!=membership||outcome.source_world.membership!=membership||
            outcome.plan.source_results!=qualification.source_binding.results||outcome.plan.source_rounds!=qualification.source_binding.league_rounds)
            throw FormatError("world12: annual player lineage has stale or unordered exchanges");
        validate_annual_promotion_plan(outcome.source_world,outcome.plan.source_results,outcome.plan.source_rounds,outcome.plan);
        for(const auto& group:outcome.plan.groups)for(const auto& exchange:group.exchanges)if(!exchange.protected_manager&&!exchange.lower.valid()) {
            ranks[exchange.promoted]=ranks.at(exchange.relegated);ranks.erase(exchange.relegated);
        }
        membership=outcome.plan.after;
    }
    if(index!=promotions.size())throw FormatError("world12: unexpected annual player country receipt");
    for(size_t league=0;league<kLeagueSlots;++league) {
        auto expected=membership[league].clubs,actual=world.membership[league].clubs;std::sort(expected.begin(),expected.end());std::sort(actual.begin(),actual.end());
        if(expected!=actual)throw FormatError("world12: annual player fixture shuffle changed membership");for(auto id:actual)if(!ranks.contains(id))throw FormatError("world12: annual player retained position is unknown");
    }
    return ranks;
}
AnnualPlayerRenewalStage stage_annual_player_renewal(const CareerSnapshot& state,const UnpublishedAnnualQualification& qualification,
    const std::vector<AnnualPromotionOutcome>& promotions,const GameState& reference,const TextDb& texts) {
    state.validate();const auto before=state;const auto ranks=annual_player_ranks(state.world,qualification,promotions);
    if(state.season!=qualification.source_season||state.date!=qualification.source_binding.date||!state.world.settings||!state.world.economy||
       !state.world.free_players||!state.world.transfer_market||!state.transfers||state.prepared_event||
       std::any_of(state.world.transfer_market->rows.begin(),state.world.transfer_market->rows.end(),[](const auto& row){return row.has_value();}))
        throw FormatError("world12: annual player renewal needs the settled unpublished boundary");
    if(std::any_of(state.world.loans.begin(),state.world.loans.end(),[](const auto& loan){return !loan.source_slot;})||
       std::any_of(state.transfers->loans.begin(),state.transfers->loans.end(),[](const auto& loan){return !loan.source_slot;}))
        throw FormatError("world12: annual loan reset has unknown physical Source slots");
    auto candidate=expand_population(state);assign_transfer_loan_slots(candidate);const auto settings=*candidate.world.settings;
    GameRandom game(state.game_seed);WatcomRandom random(state.watcom_seed);AnnualPlayerRenewalStage result;result.source_binding=before;
    std::vector<ClubId> humans;for(const auto& manager:candidate.world.managers){humans.push_back(manager.club);if(!manager.career||!manager.career->season_records||!manager.career->season_services||!manager.career->annual_records||!candidate.world.economy->managers[manager.seat].monthly)
        throw FormatError("world12: annual manager reset has unknown backing");}
    if(humans.empty())throw FormatError("world12: club annual player pass has no real managers");
    // Every manager resets before any real player ages. This preserves Source
    // advertising/familiarity and the loan-owner board order across humans.
    {
        auto projection=AiMatchProjection::create_for_humans(candidate,humans,reference,{settings.mode,settings.difficulty,settings.foreign_rule,true});
        install_career_economy(projection,candidate.world);install_transfer_loans(projection,candidate);auto& local=projection.state;local.texts=&texts;local.season=int(qualification.target_season);
        for(size_t club=0;club<humans.size();++club)local.projected_match_clubs[club].table_rank=ranks.at(humans[club]);
        for(auto [address,size]:std::initializer_list<std::pair<uint32_t,size_t>>{{layout::CareerSession_DispatchOtherGames_RuntimeField,4},{layout::CareerSession_CloseLeagueFamily_ExecutableField,4},{layout::LeagueTableDisplayModes,8},{layout::ManagerSettings,8},{layout::CareerSession_AnyOf_ExecutableField,8},{layout::Savestate_Layout_ExecutableField3,8},{layout::Flags,12}})local.memory.add_block(address,size);
        if(!local.memory.has(layout::ActiveSelectors+7)){local.memory.add_block(layout::ActiveSelectors,8);for(uint32_t i=0;i<8;++i)local.memory.set8(layout::ActiveSelectors+i,0x14);}
        reset_annual_presentation(local);
        MinuteReport report;for(uint8_t manager=0;manager<projection.human_managers();++manager){const auto club=projection.global_club(uint32_t(local.manager_teams[manager]));reset_manager_season(local,manager,random,report,annual_club_relations(qualification,club));if(report.unsupported)throw FormatError(report.reason);}
        projection.commit_match_state(candidate.world,true);capture_career_economy(candidate.world,projection);capture_transfer_history(candidate,projection);capture_loan_boards(candidate,projection);
        projection.commit_desktop_preferences(candidate);
        for(uint8_t manager=0;manager<projection.human_managers();++manager){const auto seat=projection.global_manager(manager);auto& career=*std::find_if(candidate.world.managers.begin(),candidate.world.managers.end(),[&](const auto& actor){return actor.seat==seat;})->career;
            career.other_games_view=local.memory.u8(layout::CareerSession_DispatchOtherGames_RuntimeField+manager);career.pairings_display_byte=local.memory.u8(layout::CareerSession_CloseLeagueFamily_ExecutableField+manager);
            career.table_display_word=local.memory.u16(layout::LeagueTableDisplayModes+uint32_t(manager*2));career.strengths_display_word=local.memory.u16(layout::ManagerSettings+uint32_t(manager*2));career.stadium_curve_flags=local.memory.u8(layout::Flags+uint32_t(manager*3+1));
            capture_manager_statistics_flags(local,manager,career,5);
        }
    }
    std::vector<ClubId> order;for(auto group:{TableGroup::germany,TableGroup::england,TableGroup::europe})for(auto league:candidate.world.catalogue.simulation_leagues(group))for(auto id:candidate.world.membership[league.value].clubs)order.push_back(id);
    std::set<PlayerId> visited;
    const auto pass=[&](std::optional<ClubId> club) {
        auto clubs=humans;if(club&&std::find(clubs.begin(),clubs.end(),*club)==clubs.end())clubs.push_back(*club);
        auto projection=AiMatchProjection::create_for_humans(candidate,clubs,reference,{settings.mode,settings.difficulty,settings.foreign_rule,true});
        install_career_economy(projection,candidate.world);install_transfer_loans(projection,candidate);std::vector<PlayerId> free;
        for(auto id:candidate.world.free_players->domestic)if(id.valid())free.push_back(id);for(auto id:candidate.world.free_players->foreign)if(id.valid())free.push_back(id);projection.include_players(candidate.world,free);
        auto& local=projection.state;local.texts=&texts;local.season=int(qualification.target_season);
        for(size_t team=0;team<clubs.size();++team)local.projected_match_clubs[team].table_rank=ranks.contains(clubs[team])?ranks.at(clubs[team]):-1;
        const auto install_pool=[&](const auto& pool,size_t first){for(size_t n=0;n<pool.size();++n)local.squad_slots.at(first+n)=pool[n].valid()?uint16_t(projection.local_player(pool[n])):0xfff;};
        install_pool(candidate.world.free_players->domestic,size_t(runtime_dimensions.free_first));install_pool(candidate.world.free_players->foreign,size_t(runtime_dimensions.foreign_free_first));
        auto working=candidate;std::vector<std::pair<PlayerId,ClubId>> released;std::set<PlayerId> removed;std::set<ClubId> overflow_clubs;
        const auto sync=[&]{
            for(size_t team=0;team<clubs.size();++team)for(size_t row=0;row<24;++row){const auto raw=local.squad_slots.at(team*24+row);working.world.clubs.at(clubs[team]).roster[row]=(raw&0xfff)==0xfff?RosterSlot{}:RosterSlot{projection.global_player(raw&0xfff),uint16_t(((raw&0x8000)?1:0)|((raw&0x4000)?2:0)|((raw&0x2000)?4:0)|((raw&0x1000)?8:0))};}
            for(size_t n=0;n<local.players.size();++n){auto& player=working.world.players.at(projection.global_player(uint32_t(n)));player.attributes=local.players[n];player.cards=local.player_cards[n];player.bonus=local.player_bonus[n];}
            const auto pool=[&](auto& target,size_t first){for(size_t n=0;n<target.size();++n){const auto raw=local.squad_slots.at(first+n)&0xfff;target[n]=raw==0xfff?PlayerId{}:projection.global_player(raw);}};
            pool(working.world.free_players->domestic,size_t(runtime_dimensions.free_first));pool(working.world.free_players->foreign,size_t(runtime_dimensions.foreign_free_first));forget_loans(working,removed);
        };
        PlayerReleaseDestination destination;
        destination.pool_slot=[&](const GameState& current,int player){const auto& person=current.players.at(size_t(player));const auto nationality=person.nationality()+((current.player_bonus.at(size_t(player))&128)?32:0);const bool domestic=candidate.world.catalogue.scope.domestic_enabled(Country(nationality));const auto first=domestic?runtime_dimensions.free_first:runtime_dimensions.foreign_free_first;const int count=domestic?80:20;for(int n=0;n<count;++n)if(current.squad_slots.at(size_t(first+n))==0xfff)return first+n;return -1;};
        destination.overflow=[&](GameState& current,int slot,GameRandom&,WatcomRandom& wr,MinuteReport&){
            sync();const auto player=projection.global_player(current.squad_slots.at(size_t(slot))&0xfff);
            if(club){auto& roster=working.world.clubs.at(*club).roster;const auto at=std::find_if(roster.begin(),roster.end(),[&](const auto& row){return row.player==player;});if(at==roster.end())throw FormatError("world12: annual overflow lost its actual club owner");std::move(at+1,roster.end(),at);roster.back()={};}
            else {for(auto& id:working.world.free_players->domestic)if(id==player)id={};for(auto& id:working.world.free_players->foreign)if(id==player)id={};}
            const auto target=place_unlisted_player(working,player,reference,wr,{true,&ranks,club?*club:ClubId{}});overflow_clubs.insert(target);
            for(size_t n=0;n<current.players.size();++n){const auto& person=working.world.players.at(projection.global_player(uint32_t(n)));current.players[n]=person.attributes;current.player_cards[n]=person.cards;current.player_bonus[n]=person.bonus;}
        };
        SeasonPlayerRenewalHooks hooks;hooks.first_visit=[&](int player){return visited.insert(projection.global_player(uint32_t(player))).second;};
        hooks.release=[&](GameState& current,int slot,GameRandom& gr,WatcomRandom& wr,MinuteReport& report){const auto player=projection.global_player(current.squad_slots.at(size_t(slot))&0xfff);if(!removed.insert(player).second)throw FormatError("world12: annual player released twice");if(club)released.push_back({player,*club});return release_player(current,slot,gr,wr,report,&destination);};
        MinuteReport report;
        const auto run=[&](int first,int end){if(!renew_season_player_slots(local,first,end,game,random,report,hooks)||report.unsupported)throw FormatError(report.unsupported?report.reason:"world12: annual player release failed");};
        if(club){const auto first=int(projection.local_club(*club)*24);run(first,first+24);}else {run(runtime_dimensions.free_first,runtime_dimensions.free_first+80);run(runtime_dimensions.foreign_free_first,runtime_dimensions.foreign_free_first+20);}
        sync();forget_loans(candidate,removed);if(released.empty())projection.commit_match_state(candidate.world,true);else projection.commit_released_players(candidate.world,released);
        candidate.world.free_players=working.world.free_players;
        for(auto id:overflow_clubs){candidate.world.clubs.at(id)=working.world.clubs.at(id);for(const auto& slot:working.world.clubs.at(id).roster)if(!slot.empty())candidate.world.players.at(slot.player)=working.world.players.at(slot.player);}
        capture_career_economy(candidate.world,projection);result.released+=removed.size();candidate.world.validate();
    };
    for(auto id:order)pass(id);pass({});
    for(auto& [id,club]:candidate.world.clubs)if(club.league.valid())club.fairplay_points=0;
    result.desktop=std::move(candidate.desktop);result.transfers=std::move(candidate.transfers);result.results=SeasonResults::empty(state.world.catalogue);result.world=restore_population(state.world,candidate.world);result.game_seed=game.seed();result.watcom_seed=random.seed();result.game_draws=game.calls();result.watcom_draws=random.calls();result.visited=visited.size();result.inactive_origin_refresh_required=false;
    if(state!=before)throw FormatError("world12: annual player owner changed during reset/retirement");return result;
}
void begin_annual_manager_history(World& world,uint32_t target) {
    world.validate();auto candidate=world;
    for(auto& manager:candidate.managers){if(!manager.career||!manager.career->season_records)throw FormatError("world12: annual manager history is unknown");auto& career=*manager.career;auto& records=*career.season_records;
        if(target>49){if(!career.first_recorded_season)throw FormatError("world12: annual history window origin is unknown");for(size_t i=0;i<147;++i)records[i]=records[i+3];career.first_recorded_season=uint16_t(*career.first_recorded_season+1);}
        records[size_t(std::min<uint32_t>(target,49))*3]=0xfe;
    }
    candidate.validate();world=std::move(candidate);
}
}
