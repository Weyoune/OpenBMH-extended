// SPDX-License-Identifier: GPL-3.0-only
#include "app/world12_manager.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <type_traits>
namespace bmh::app::world12 {
static_assert(std::is_nothrow_move_assignable_v<bmh::world12::World>);
static_assert(std::is_nothrow_copy_assignable_v<std::optional<bmh::world12::ManagerFinance>>);
static_assert(std::is_nothrow_copy_assignable_v<std::optional<bmh::world12::ManagerTactics>>);
bmh::world12::PlayerInitializationOutcome add_initial_manager(bmh::world12::World& world,
    ManagerActionOwner& owner,ManagerActionOwner expected,const bmh::world12::InitialManagerChoice& choice,
    bmh::world12::CareerSettings settings,const bmh::world12::InitialStrengthRanges& ranges,
    const bmh::GameState& reference,const bmh::LeImage& rules,bmh::GameRandom& game,bmh::WatcomRandom& random) {
    if(!owner.scope||owner.scope!=expected.scope||owner.revision!=expected.revision||owner.revision==std::numeric_limits<uint64_t>::max())throw std::invalid_argument("world12: stale manager confirmation owner");
    const auto before=world;const auto old_game=game;const auto old_random=random;auto candidate=world;auto staged_game=game;auto staged_random=random;
    const auto outcome=bmh::world12::add_initial_manager(candidate,choice,settings,ranges,reference,rules,staged_game,staged_random);
    if(world!=before||owner.scope!=expected.scope||owner.revision!=expected.revision||game.seed()!=old_game.seed()||game.calls()!=old_game.calls()||random.seed()!=old_random.seed()||random.calls()!=old_random.calls())throw std::invalid_argument("world12: manager confirmation owner changed");
    world=std::move(candidate);game=staged_game;random=staged_random;++owner.revision;return outcome;
}
bmh::world12::PlayerInitializationOutcome initialize_club_player_state(bmh::world12::World& world,
    ManagerActionOwner& owner,ManagerActionOwner expected,const bmh::world12::PlayerInitializationPlan& plan,
    const bmh::GameState& reference,bool foreign_rule,bmh::GameRandom& game,bmh::WatcomRandom& random) {
    if(!owner.scope || owner.scope!=expected.scope || owner.revision!=expected.revision || owner.revision==std::numeric_limits<uint64_t>::max())
        throw std::invalid_argument("world12: stale club player initialization owner");
    const auto before=world;const auto old_game=game;const auto old_random=random;
    auto candidate=world;auto staged_game=game;auto staged_random=random;
    bmh::world12::initialize_career_players(candidate,plan,staged_game,staged_random);
    bmh::world12::initialize_career_markets_and_moods(candidate,reference,staged_game,staged_random);
    bmh::world12::initialize_career_player_pools(candidate,plan,reference,foreign_rule,staged_game,staged_random);
    if(world!=before || owner.scope!=expected.scope || owner.revision!=expected.revision ||
        game.seed()!=old_game.seed() || game.calls()!=old_game.calls() || random.seed()!=old_random.seed() || random.calls()!=old_random.calls())
        throw std::invalid_argument("world12: club player initialization session changed");
    const bmh::world12::PlayerInitializationOutcome outcome{staged_game.calls()-old_game.calls(),staged_random.calls()-old_random.calls(),candidate.players.size()};
    world=std::move(candidate);game=staged_game;random=staged_random;++owner.revision;return outcome;
}
uint64_t initialize_manager_contracts(bmh::world12::World& world,ManagerActionOwner& owner,
    const ManagerActionIdentity& identity,const bmh::GameState& rules,bmh::world12::TacticalPolicy policy,bmh::WatcomRandom& random) {
    if(!owner.scope || identity.scope!=owner.scope || identity.revision!=owner.revision || owner.revision==std::numeric_limits<uint64_t>::max())
        throw std::invalid_argument("world12: stale initial manager owner");
    const auto found=std::find_if(world.managers.begin(),world.managers.end(),[&](const auto& manager){return manager.seat==identity.seat;});
    if(found==world.managers.end() || found->club!=identity.club)throw std::invalid_argument("world12: initial manager action belongs to another club");
    const auto before=world;const auto old_owner=owner;const auto old_seed=random.seed();const auto old_calls=random.calls();
    auto candidate=world;auto staged_random=random;
    const auto draws=bmh::world12::initialize_manager_contracts(candidate,identity.seat,rules,policy,staged_random);
    if(world!=before || owner.scope!=old_owner.scope || owner.revision!=old_owner.revision || random.seed()!=old_seed || random.calls()!=old_calls)
        throw std::invalid_argument("world12: manager creation session changed during initialization");
    world=std::move(candidate);random=staged_random;++owner.revision;return draws;
}
bmh::world12::TacticalOutcome apply_manager_tactical_action(bmh::world12::World& world,ManagerActionOwner& owner,
    const ManagerActionIdentity& identity,const bmh::world12::TacticalAction& action,bmh::world12::TacticalPolicy policy) {
    if(!owner.scope || identity.scope!=owner.scope || identity.revision!=owner.revision || owner.revision==std::numeric_limits<uint64_t>::max())
        throw std::invalid_argument("world12: stale tactical manager action");
    world.validate();
    const auto manager=std::find_if(world.managers.begin(),world.managers.end(),[&](const auto& row){return row.seat==identity.seat;});
    if(manager==world.managers.end() || manager->club!=identity.club)throw std::invalid_argument("world12: tactical action belongs to another manager/club");
    const auto result=bmh::world12::calculate_manager_tactical_action(world,identity.seat,action,policy);
    if(result.changed) {
        if(result.lineup) {
            auto candidate=world;
            const auto index=size_t(manager-world.managers.begin());candidate.managers[index].tactics=result.tactics;
            candidate.clubs.at(identity.club).roster=result.lineup->roster;
            candidate.clubs.at(identity.club).match_flags=result.lineup->match_flags;candidate.clubs.at(identity.club).lineup_indoor=result.lineup->indoor;
            for(size_t row=0;row<24;++row)if(!result.lineup->roster[row].empty())
                candidate.players.at(result.lineup->roster[row].player).cards=result.lineup->cards_by_row[row];
            candidate.validate();world=std::move(candidate);
        }else manager->tactics=result.tactics;
        ++owner.revision;
    }
    return result;
}
bmh::world12::ManagerMoneyOutcome book_manager_funds(bmh::world12::World& world,ManagerActionOwner& owner,
    const ManagerActionIdentity& identity,int difficulty,int32_t amount,bool force,bool statistics) {
    if(!owner.scope||identity.scope!=owner.scope||identity.revision!=owner.revision||owner.revision==std::numeric_limits<uint64_t>::max())
        throw std::invalid_argument("world12: stale financial manager action");
    world.validate();
    const auto manager=std::find_if(world.managers.begin(),world.managers.end(),[&](const auto& row){return row.seat==identity.seat;});
    if(manager==world.managers.end()||manager->club!=identity.club)throw std::invalid_argument("world12: financial action belongs to another manager/club");
    if(!manager->finance)throw std::invalid_argument("world12: manager finances have not been initialized by the career owner");
    const auto result=bmh::world12::calculate_manager_booking(*manager->finance,difficulty,amount,force,statistics);
    if(result.changed){manager->finance=result.finance;++owner.revision;}
    return result;
}
}
