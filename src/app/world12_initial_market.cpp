// SPDX-License-Identifier: GPL-3.0-only
#include "app/world12_initial_season.hpp"
#include "game/market_rules.hpp"
#include "game/month.hpp"
#include <algorithm>
#include <limits>
#include <set>
#include <stdexcept>
namespace bmh::app::world12 {
InitialMarketOutcome initialize_first_season_market(bmh::world12::CareerSnapshot& state,
    ManagerActionOwner& owner,ManagerActionOwner expected,const bmh::GameState* initial_reference) {
    using namespace bmh;using namespace bmh::world12;state.validate();
    if(!owner.scope||owner.scope!=expected.scope||owner.revision!=expected.revision||owner.revision==std::numeric_limits<uint64_t>::max()||
        state.date||state.season||!state.calendar||state.calendar->next_day||!state.world.initial_competitions||!state.world.initial_competitions->manager_services_complete||
        state.world.transfer_market||!state.world.free_players||!state.world.loans.empty())throw std::invalid_argument("world12: invalid initial market phase/owner");
    const auto before=state;auto candidate=state;WatcomRandom random(state.watcom_seed);TransferMarket market;market.checkpoint_game_seed=state.game_seed;
    std::vector<ClubId> clubs;int first=0;
    for(auto group:{TableGroup::germany,TableGroup::england,TableGroup::europe})for(auto league:candidate.world.catalogue.simulation_leagues(group)) {
        auto& members=candidate.world.membership[league.value].clubs;
        shuffle_initial_positions(first,int(members.size()),random,[&](int a,int b){std::swap(members.at(size_t(a)),members.at(size_t(b)));});
        first+=int(members.size());clubs.insert(clubs.end(),members.begin(),members.end());
    }
    const auto listed=[&](PlayerId id){for(const auto& row:market.rows)if(row&&row->player==id)return true;return false;};
    const auto human=[&](int team){return std::any_of(candidate.world.managers.begin(),candidate.world.managers.end(),[&](const auto& m){return m.club==clubs.at(size_t(team));});};
    const auto describe=[&](int team){const auto& club=candidate.world.clubs.at(clubs.at(size_t(team)));MarketClubShape shape;shape.capacity=24;shape.flags=club.match_flags;
        for(const auto& slot:club.roster)if(!slot.empty()){++shape.players;++shape.positions[candidate.world.players.at(slot.player).attributes.position()];}return shape;};
    const auto choose=[&](int team,int row){const auto id=candidate.world.clubs.at(clubs.at(size_t(team))).roster.at(size_t(row)).player;return MarketPlayerChoice{candidate.world.players.at(id).attributes.position(),listed(id),false};};
    const auto publish=[&](PlayerId id,ClubId seller){if(listed(id))throw std::invalid_argument("world12: initial market chose an already listed player");
        size_t free=0;while(free<40&&market.rows[free])++free;if(free==40)throw std::invalid_argument("world12: initial market exceeds regular slots");
        const auto& player=candidate.world.players.at(id);GameState quote;quote.players={player.attributes};quote.squad_slots={0};quote.player_names.insert(quote.player_names.end(),player.attributes.name.begin(),player.attributes.name.end());quote.player_names.push_back(0);quote.player_name_offsets={0};
        const auto priced=quote_market_listing(quote,0,seller.valid(),false,random);MarketListing listing;listing.player=id;listing.seller=seller;listing.fee=priced.fee;listing.salary=priced.salary;listing.asking_percent=priced.asking_percent;listing.demand_percent=priced.demand_percent;listing.weeks=priced.weeks;
        if(seller.valid())listing.human_seller=std::any_of(candidate.world.managers.begin(),candidate.world.managers.end(),[&](const auto& m){return m.club==seller;});market.rows[free]=listing;
    };
    std::array<uint8_t,5> counts{};for(auto id:candidate.world.free_players->domestic)counts[1]+=id.valid();for(auto id:candidate.world.free_players->foreign)counts[2]+=id.valid();
    const auto adds=allocate_market_entries(counts,random);int toggle=0;
    for(size_t kind=0;kind<4;++kind)for(size_t left=adds[kind];left;--left) {
        if(kind==0||kind==2){std::vector<PlayerId> eligible;const auto collect=[&](const auto& pool){for(auto id:pool)if(id.valid()&&!listed(id))eligible.push_back(id);};
            if(kind==0)collect(candidate.world.free_players->domestic);else collect(candidate.world.free_players->foreign);
            if(eligible.empty())throw std::invalid_argument("world12: no eligible initial free agent");publish(eligible.at(size_t(random.range(0,int(eligible.size())-1))),{});
        }else{
            // All current 1.2 squads have 24 slots, exactly like the expanded
            // 1.1 reference. The small-club request retains Source fallback and
            // small positional targets while scanning those large squads.
            const int total=int(clubs.size());const auto selected=select_market_club_player(kind==1?0:total,total,total,total,toggle,random,human,describe,choose);
            if(!selected)throw std::invalid_argument("world12: no initial club listing candidate");const auto seller=clubs.at(size_t(selected->club));const auto id=candidate.world.clubs.at(seller).roster.at(size_t(selected->row)).player;publish(id,seller);
        }
    }
    market.entry40_history_known=initial_reference!=nullptr;
    if(initial_reference)for(size_t n=0;n<4;++n){const auto local=initial_reference->memory.u16(layout::EditorList+40*0x58+uint32_t(n*2));
        if(local!=0xffff){if(local>=clubs.size())throw std::invalid_argument("world12: initial special bidder is outside selected league scope");market.last_entry40_bidders[n]=clubs[local];}}
    market.list_toggle=uint8_t(toggle);candidate.world.transfer_market=market;candidate.watcom_seed=random.seed();candidate.validate();
    if(state!=before||owner.scope!=expected.scope||owner.revision!=expected.revision)throw std::invalid_argument("world12: initial market owner changed during selection");
    InitialMarketOutcome outcome{random.calls(),0};for(const auto& row:market.rows)outcome.listings+=row.has_value();state=std::move(candidate);++owner.revision;return outcome;
}
}
