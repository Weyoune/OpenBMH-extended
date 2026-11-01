// SPDX-License-Identifier: GPL-3.0-only
#include "game/world12/monthly_market.hpp"
#include "game/world12/match_projection.hpp"
#include "game/world12/economy.hpp"
#include "game/market_rules.hpp"
#include "game/month.hpp"
#include <algorithm>
#include <stdexcept>
namespace bmh::world12 {
MonthlyMarketOutcome renew_monthly_market(CareerSnapshot& state,const GameState& reference,const TextDb& texts,const MonthlyMarketPolicy& policy) {
    state.validate();if(!state.world.transfer_market||!state.world.free_players||!state.world.settings||!state.world.economy)throw FormatError("world12: monthly market state is unavailable");
    for(auto id:policy.national_squad)if(!state.world.players.contains(id))throw FormatError("world12: unknown protected national player");
    const auto before=state;auto candidate=state;auto& market=*candidate.world.transfer_market;WatcomRandom random(state.watcom_seed);MonthlyMarketOutcome outcome;
    const auto listed=[&](PlayerId id){return std::any_of(market.rows.begin(),market.rows.end(),[&](const auto& row){return row&&row->player==id;});};
    const auto loaned=[&](PlayerId id){return std::any_of(candidate.world.loans.begin(),candidate.world.loans.end(),[&](const auto& loan){return loan.player==id;});};
    std::array<uint8_t,5> counts{};for(size_t row=0;row<40;++row)counts[0]+=market.rows[row].has_value();
    for(auto id:candidate.world.free_players->domestic)if(id.valid()){++counts[1];counts[3]+=listed(id);}
    for(auto id:candidate.world.free_players->foreign)if(id.valid()){++counts[2];counts[4]+=listed(id);}
    const auto adds=allocate_market_entries(counts,random);
    std::vector<ClubId> clubs;
    for(auto table:{TableGroup::germany,TableGroup::england,TableGroup::europe}) {
        for(auto league:candidate.world.catalogue.table_leagues(table))for(auto id:candidate.world.membership[league.value].clubs)clubs.push_back(id);

    }
    const auto human=[&](int team){return std::any_of(candidate.world.managers.begin(),candidate.world.managers.end(),[&](const auto& m){return m.club==clubs.at(size_t(team));});};
    const auto describe=[&](int team){const auto id=clubs.at(size_t(team));const auto& c=candidate.world.clubs.at(id);MarketClubShape shape;shape.capacity=24;shape.flags=c.match_flags;
        for(const auto& slot:c.roster)if(!slot.empty()){++shape.players;++shape.positions[candidate.world.players.at(slot.player).attributes.position()];}
        shape.players+=int(std::count_if(candidate.world.loans.begin(),candidate.world.loans.end(),[&](const auto& loan){return loan.owner==id;}));shape.players&=255;return shape;};
    const auto choose=[&](int team,int row){const auto& roster=candidate.world.clubs.at(clubs.at(size_t(team))).roster;
        // Do not dereference Native FFF if a reserved returning player makes a
        // physical empty place part of the original candidate range.
        if(row<0||row>=24||roster[size_t(row)].empty())return MarketPlayerChoice{0,true,false};
        const auto id=roster[size_t(row)].player;return MarketPlayerChoice{candidate.world.players.at(id).attributes.position(),listed(id)||loaned(id),policy.national_squad.contains(id)};};
    std::vector<ClubId> human_clubs;for(const auto& m:candidate.world.managers)human_clubs.push_back(m.club);
    const auto publish=[&](PlayerId id,ClubId seller){
        if(!id.valid()||listed(id)||loaned(id))throw FormatError("world12: monthly listing has no available physical player");
        size_t row=0;while(row<40&&market.rows[row])++row;if(row==40)throw FormatError("world12: monthly market has no regular slot");
        GameState quote;const auto& person=candidate.world.players.at(id);quote.players={person.attributes};quote.squad_slots={0};
        const auto priced=quote_market_listing(quote,0,seller.valid(),false,random);MarketListing listing;listing.player=id;listing.seller=seller;listing.fee=priced.fee;listing.salary=priced.salary;listing.asking_percent=priced.asking_percent;listing.demand_percent=priced.demand_percent;listing.weeks=priced.weeks;
        listing.human_seller=seller.valid()&&std::any_of(candidate.world.managers.begin(),candidate.world.managers.end(),[&](const auto& m){return m.club==seller;});market.rows[row]=listing;
        const auto& settings=*candidate.world.settings;auto p=AiMatchProjection::create_for_humans(candidate,human_clubs,reference,{settings.mode,settings.difficulty,settings.foreign_rule,true});
        p.include_players(candidate.world,{id});if(seller.valid())p.include_club_references(candidate.world,{seller});install_career_economy(p,candidate.world);p.state.texts=&texts;
        const auto at=layout::EditorList+uint32_t(row*0x58);p.state.memory.add_block(at,0x58);p.state.memory.set16(at+8,seller.valid()?uint16_t(p.local_club_reference(seller)):0xfff);p.state.memory.set8(at+0x54,listing.asking_percent);
        for(size_t m=0;m<p.human_managers();++m)search_profile_news(p.state,int(m),at,int(p.local_player(id)));
        capture_career_economy(candidate.world,p);++outcome.added;
    };
    int toggle=market.list_toggle;
    for(size_t kind=0;kind<4;++kind)for(size_t left=adds[kind];left;--left){
        if(kind==0||kind==2){std::vector<PlayerId> eligible;const auto collect=[&](const auto& pool){for(auto id:pool)if(id.valid()&&!listed(id)&&!loaned(id))eligible.push_back(id);};
            if(kind==0)collect(candidate.world.free_players->domestic);else collect(candidate.world.free_players->foreign);
            if(eligible.empty())throw FormatError("world12: no available monthly free agent");publish(eligible[size_t(random.range(0,int(eligible.size())-1))],{});
        }else{const auto selected=select_market_club_player(kind==1?0:int(clubs.size()),int(clubs.size()),int(clubs.size()),int(clubs.size()),toggle,random,human,describe,choose);
            if(!selected)throw FormatError("world12: no monthly club listing candidate");const auto seller=clubs.at(size_t(selected->club));
            if(selected->row<0||selected->row>=24)throw FormatError("world12: selected monthly row lies outside roster");publish(candidate.world.clubs.at(seller).roster[size_t(selected->row)].player,seller);}
    }
    market.list_toggle=uint8_t(toggle);candidate.watcom_seed=random.seed();candidate.validate();if(state!=before)throw FormatError("world12: monthly market owner changed during renewal");
    outcome.watcom_draws=random.calls();state=std::move(candidate);return outcome;
}
}
