// SPDX-License-Identifier: GPL-3.0-only
#include "game/world12/initialization.hpp"
#include "game/career.hpp"
#include "game/season.hpp"
#include "game/week.hpp"
#include <algorithm>
#include "formats/le.hpp"
#include "formats/executable_layout.hpp"
#include <set>
#include <stdexcept>
#include <type_traits>
namespace bmh::world12 {
static_assert(std::is_nothrow_move_assignable_v<World>);
InitialStrengthRanges read_initial_strength_ranges(const Catalogue& catalogue,const LeImage& german,const LeImage& english) {
    InitialStrengthRanges result;
    for(const auto& league:catalogue.leagues) {
        const bool english_domestic=league.country==Country::england && (league.local_index>0 || catalogue.scope.england || !catalogue.scope.club_play());
        const auto& image=english_domestic?english:german;
        const int index=league.country==Country::germany||english_domestic?league.local_index:
            league.country==Country::spain?5:league.country==Country::england?6:league.country==Country::france?7:8;
        const auto at=layout::LeagueStrengthRanges+uint32_t(index*2);
        if(!image.contains(at+1))throw std::invalid_argument("world12: initial strength rules missing");
        result[league.id.value]={image.u8(at),image.u8(at+1)};
        if(result[league.id.value][0]>result[league.id.value][1]||result[league.id.value][1]>99)throw std::invalid_argument("world12: malformed initial strength rules");
    }
    return result;
}
PlayerInitializationPlan make_player_initialization_plan(const World& master,GameScope scope,const InitialStrengthRanges& strengths) {
    master.validate();scope.validate();
    if(master.catalogue.scope!=GameScope{} || master.players_initialized || master.markets || !master.managers.empty() || !master.loans.empty())
        throw std::invalid_argument("world12: initial player plan requires the untouched full master");
    PlayerInitializationPlan plan{scope,strengths,{},{}};std::set<PlayerId> rostered;
    for(const auto& [id,club]:master.clubs)for(const auto& row:club.roster)if(!row.empty())rostered.insert(row.player);
    for(const auto& [id,player]:master.players)if(!rostered.contains(id))plan.extra_players.push_back(id);
    if(plan.extra_players.size()!=54)throw std::invalid_argument("world12: Original initial player pool requires 54 ordered entries");
    const auto selected=Catalogue::dual_classic(scope.club_play()?scope:GameScope{});
    std::vector<LeagueId> order;
    for(auto group:{TableGroup::germany,TableGroup::england,TableGroup::europe}) {
        const auto leagues=selected.table_leagues(group);order.insert(order.end(),leagues.begin(),leagues.end());
    }
    for(auto league:order) {
        if(strengths[league.value][0]>strengths[league.value][1] || strengths[league.value][1]>99)
            throw std::invalid_argument("world12: invalid initial player strength range");
        const auto& clubs=master.membership[league.value].clubs;
        for(size_t rank=0;rank<clubs.size();++rank){InitialPlayerGroup group{league,uint8_t(rank),{}};
            for(const auto& row:master.clubs.at(clubs[rank]).roster)if(!row.empty())group.players.push_back(row.player);
            plan.groups.push_back(std::move(group));}
    }
    return plan;
}
PlayerInitializationOutcome initialize_career_players(World& world,const PlayerInitializationPlan& plan,GameRandom& game,WatcomRandom& random) {
    world.validate();
    if(world.players_initialized || world.markets || world.catalogue.scope!=plan.scope || !world.managers.empty() || !world.loans.empty() ||
        world.development_balance || world.trend_rotation || plan.extra_players.size()!=54)
        throw std::invalid_argument("world12: player generation requires a fresh selected career world");
    for(const auto& [id,club]:world.clubs)if(club.season_stats!=ClubSeasonStats{})throw std::invalid_argument("world12: played club cannot enter career generation");
    for(const auto& [id,p]:world.players)if(p.cards)throw std::invalid_argument("world12: card history cannot enter fresh career generation");
    std::set<PlayerId> seen;
    const auto check=[&](PlayerId id){if(!world.players.contains(id)||!seen.insert(id).second)throw std::invalid_argument("world12: incomplete/repeated initial player identity");};
    for(auto id:plan.extra_players)check(id);
    std::set<std::pair<LeagueId,uint8_t>> ranks;
    for(const auto& group:plan.groups) {
        const auto& league=world.catalogue.league(group.league);const auto range=plan.strengths[group.league.value];
        if(group.rank>=league.clubs || !ranks.emplace(group.league,group.rank).second || group.players.size()<11 || group.players.size()>24 ||
            range[0]>range[1] || range[1]>99 || (plan.scope.club_play()&&!league.active))
            throw std::invalid_argument("world12: invalid initial league/rank group");
        if(plan.scope.club_play()) {
            const auto& club=world.clubs.at(world.membership[group.league.value].clubs[group.rank]);std::vector<PlayerId> expected;
            for(const auto& row:club.roster)if(!row.empty())expected.push_back(row.player);
            if(expected!=group.players)throw std::invalid_argument("world12: initial group does not match authoritative squad order");
        }
        for(auto id:group.players)check(id);
    }
    if(seen.size()!=world.players.size())throw std::invalid_argument("world12: initial plan omits world players");
    const auto before=world;auto candidate=world;auto staged_game=game;auto staged_random=random;
    const auto g_seed=game.seed();const auto w_seed=random.seed();const auto g_calls=game.calls(),w_calls=random.calls();
    GameState local;local.players.resize(1);
    const auto generate=[&](PlayerId id,int level){auto& player=candidate.players.at(id);local.players[0]=player.attributes;
        bmh::generate_player(local,0,level,staged_game,staged_random);player.attributes=local.players[0];};
    for(size_t i=0;i<plan.extra_players.size();++i)generate(plan.extra_players[i],staged_random.range(i<22?20:70,90));
    for(const auto& group:plan.groups) {
        const auto count=world.catalogue.league(group.league).clubs;const auto range=plan.strengths[group.league.value];
        const int level=int((unsigned(count)-group.rank)*unsigned(range[1]-range[0])*100u/unsigned(count)/100u)+range[0];
        for(auto id:group.players)generate(id,level|0x1000);
    }
    candidate.players_initialized=true;candidate.validate();
    if(world!=before || game.seed()!=g_seed || game.calls()!=g_calls || random.seed()!=w_seed || random.calls()!=w_calls)
        throw std::invalid_argument("world12: player-generation owner or RNG changed during initialization");
    const PlayerInitializationOutcome outcome{staged_game.calls()-g_calls,staged_random.calls()-w_calls,seen.size()};
    world=std::move(candidate);game=staged_game;random=staged_random;return outcome;
}

PlayerInitializationOutcome initialize_career_markets_and_moods(World& world,
    const GameState& reference,GameRandom& game,WatcomRandom& random) {
    world.validate();
    if(!world.players_initialized || world.markets || !world.managers.empty() || !world.loans.empty() ||
        world.development_balance || world.trend_rotation)
        throw std::invalid_argument("world12: market initialization requires generated, unplayed career players");
    for(const auto& [id,club]:world.clubs)if(club.season_stats!=ClubSeasonStats{})
        throw std::invalid_argument("world12: played club cannot enter market initialization");
    for(const auto& [id,p]:world.players)if(p.cards)throw std::invalid_argument("world12: played player cannot enter market initialization");
    if(reference.stocks.size()!=12*0x5c || runtime_dimensions.big_squad_teams<1 ||
        runtime_dimensions.record_teams<=0 || runtime_dimensions.record_teams==kNoTeam)
        throw std::invalid_argument("world12: missing market/mood reference layout");
    const auto before=world;auto candidate=world;auto staged_game=game;auto staged_random=random;
    const auto g_seed=game.seed();const auto w_seed=random.seed();const auto g_calls=game.calls(),w_calls=random.calls();
    auto local=reference;
    CareerMarkets markets;
    markets.first_default_name=uint8_t(bmh::initialize_career_market_bases(local,staged_game,staged_random));
    for(int day=0;day<30;++day)bmh::stock_market(local,staged_game);
    std::copy(local.stocks.begin(),local.stocks.end(),markets.stocks.begin());
    for(size_t i=0;i<markets.funds.size();++i)markets.funds[i]=local.memory.u8(layout::FundBlock+uint32_t(i));

    // Membership order first, then the primary/secondary countries' extra clubs,
    // then remaining cup clubs and national teams. No stable ID is renumbered.
    std::vector<ClubId> order;std::set<ClubId> seen;
    const auto append=[&](ClubId id){if(seen.insert(id).second)order.push_back(id);};
    for(auto group:{TableGroup::germany,TableGroup::england,TableGroup::europe})
        for(auto league:world.catalogue.table_leagues(group))for(auto id:world.membership[league.value].clubs)append(id);
    for(auto country:{Country::germany,Country::england})if(world.catalogue.scope.domestic_enabled(country))
        for(const auto& [id,club]:world.clubs)if(club.country==country && club.kind==ClubKind::club && !club.league.valid())append(id);
    for(const auto& [id,club]:world.clubs)if(club.kind==ClubKind::club)append(id);
    for(const auto& [id,club]:world.clubs)if(club.kind==ClubKind::national_team)append(id);

    // A bounded Core workspace uses team zero for actual/empty domestic squads
    // and the first foreign-strength slot for clubs represented by a rating.
    // This retains Source team_average/club_balance and their exact draw order.
    local.manager_teams.clear();local.players.resize(24);local.squad_slots.assign(24,kEmptySlot);
    const int foreign=runtime_dimensions.record_teams;
    local.team_flags.resize(size_t(foreign)+1);local.team_morale.resize(size_t(foreign)+1);
    local.foreign_strength.resize(1);
    const auto primary=world.catalogue.scope.england&&!world.catalogue.scope.germany?Country::england:Country::germany;
    staged_random.range(0,2);
    for(auto id:order) {
        auto& club=candidate.clubs.at(id);
        const bool empty_domestic=club.kind==ClubKind::club && !club.league.valid() && world.catalogue.scope.domestic_enabled(club.country);
        const bool national_pool=club.kind==ClubKind::national_team && club.country==primary;
        const int team=club.league.valid()||empty_domestic||national_pool?0:foreign;
        std::fill(local.squad_slots.begin(),local.squad_slots.end(),kEmptySlot);
        for(size_t i=0;i<club.roster.size();++i)if(!club.roster[i].empty()) {
            const auto& slot=club.roster[i];local.players[i]=candidate.players.at(slot.player).attributes;
            local.squad_slots[i]=uint16_t(i)|uint16_t((slot.flags&1)?kSlotInLineup:0);
        }
        local.team_morale[size_t(team)]=club.morale;local.foreign_strength[0]=club.rating;
        bmh::initialize_career_club_mood(local,team,staged_game,staged_random);
        club.morale=local.team_morale[size_t(team)];club.match_flags=local.team_flags[size_t(team)];
    }
    candidate.markets=markets;candidate.validate();
    if(world!=before || game.seed()!=g_seed || game.calls()!=g_calls || random.seed()!=w_seed || random.calls()!=w_calls)
        throw std::invalid_argument("world12: market/mood owner or RNG changed during initialization");
    const PlayerInitializationOutcome outcome{staged_game.calls()-g_calls,staged_random.calls()-w_calls,0};
    world=std::move(candidate);game=staged_game;random=staged_random;return outcome;
}
}
