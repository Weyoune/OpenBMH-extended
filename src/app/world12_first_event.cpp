// SPDX-License-Identifier: GPL-3.0-only
#include "app/world12_first_event.hpp"
#include "game/world12/annual_opening.hpp"
#include "game/world12/annual_qualification.hpp"
#include "game/world12/match_projection.hpp"
#include "game/transfer_bid.hpp"
#include "game/month.hpp"
#include "game/prediction.hpp"
#include "game/lineup.hpp"
#include "game/world12/market_bids.hpp"
#include "game/world12/pools.hpp"
#include "game/world12/national_coach.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>
namespace bmh::app::world12 {
using namespace bmh::world12;
namespace {
bool human(const World& world,ClubId club) {
    return std::any_of(world.managers.begin(),world.managers.end(),[&](const auto& m){return m.club==club;});
}
std::optional<PendingDeskOffer> initial_offer(CareerSnapshot& state,uint8_t seat,
    const GameState& reference,WatcomRandom& random) {
    const auto& world=state.world;const auto& settings=*world.settings;
    if(settings.mode==1||national_roles_active(world))return {};
    const auto& manager=*std::find_if(world.managers.begin(),world.managers.end(),[&](const auto& m){return m.seat==seat;});
    if(!manager.match_state||!manager.tactics||!manager.finance||!manager.career)throw std::invalid_argument("world12: initial offer needs complete human state");
    const int chance=random.range(0,0x46-int16_t(manager.match_state->staff_ratings[0])/0x32);
    if(chance!=0)return {};
    auto projection=AiMatchProjection::create_for_humans(state,{manager.club},reference,
        {settings.mode,settings.difficulty,settings.foreign_rule,true});
    auto& local=projection.state;const auto& club=world.clubs.at(manager.club);
    // A fresh event cannot contain bought-for-later or already pending rows.
    // Subsequent-day offer processing will own those separate state machines.
    for(size_t k=40;k<44;++k)if(world.transfer_market->rows[k]){
        // A quote without a bidder does not occupy Source OfferSlots. The next
        // manager may overwrite it, exactly as the native entry routine does.
        if(k!=40||std::any_of(world.transfer_market->rows[k]->bids.begin(),world.transfer_market->rows[k]->bids.end(),[](const auto& bid){return bid.bidder.valid();}))
            throw std::invalid_argument("world12: initial unsolicited offer slots need their decision owner");
    }
    bool fit=false,empty=false;
    for(const auto& row:club.roster){if(row.empty())empty=true;else if(world.players.at(row.player).attributes.w35c>>27==0)fit=true;}
    if(!fit&&!empty)throw std::invalid_argument("world12: no fit initial offer player; Original selection would not terminate");
    int row=0,misses=0;PlayerId player;
    for(;;){row=random.range(0,23);player=club.roster[size_t(row)].player;
        if(!player.valid()){if(misses++<100)continue;return {};}
        if(world.players.at(player).attributes.w35c>>27)continue;
        break;
    }
    const auto& person=world.players.at(player);MarketListing listing;
    listing.player=player;listing.seller=manager.club;listing.human_seller=true;
    listing.asking_percent=uint8_t(random.range(90,130));listing.demand_percent=100;
    bmh::market_value(local,row,0,listing.asking_percent,listing.fee,listing.salary,1,0,random);
    const bool lure=random.range(0,1)!=0;
    std::vector<ClubId> clubs;
    for(auto group:{TableGroup::germany,TableGroup::england,TableGroup::europe})
        for(auto league:world.catalogue.simulation_leagues(group)){
            const auto& members=world.membership[league.value].clubs;clubs.insert(clubs.end(),members.begin(),members.end());}
    std::vector<int16_t> order;std::vector<uint32_t> keys;uint8_t foreign_skip=0;
    const int nationality=person.attributes.nationality()+((person.bonus&128)?32:0);
    for(size_t index=0;index<clubs.size();++index){
        const auto& candidate=world.clubs.at(clubs[index]);if(human(world,candidate.id))continue;
        const auto& league=world.catalogue.league(candidate.league);const int region=int(candidate.country);
        // Preserve the first-pass skip counter, including its post-increment.
        if(nationality<5&&nationality!=region&&foreign_skip++%10==0)continue;
        const int count=int(std::count_if(candidate.roster.begin(),candidate.roster.end(),[](const auto& r){return !r.empty();}));
        if(count>(league.playable?21:15))continue;
        auto projected=AiMatchProjection::create(world,{candidate.id},reference);TransferClubScoreInput score;
        for(int p=0;p<4;++p){const auto average=bmh::team_average(projected.state,0,uint16_t((1<<(p+4))+3));score.averages[size_t(p)]=average.value;score.counts[size_t(p)]=average.players;}
        score.nationality=nationality;score.region=region;score.position=person.attributes.position();
        score.player_strength=(person.attributes.technique()+person.attributes.condition())/2;
        score.players=count;score.flags=candidate.match_flags;score.large_squad=true;score.domestic_capacity=league.playable;
        // At the initial event all round cursors and entry-40 bids are zero.
        // Unlike direct overflow moves, a listed offer has no second pass.
        keys.push_back(uint32_t(bmh::transfer_club_score(score,random)+100000));order.push_back(int16_t(index));
    }
    std::optional<PendingDeskOffer> notice;
    if(!order.empty()){
        bmh::quicksort_by_key(order.data(),keys.data(),int(order.size()));const auto index=size_t(order.front());
        auto& bid=listing.bids[0];bid.bidder=clubs[index];const auto& bidder=world.clubs.at(bid.bidder);
        if(lure)bid.months=uint8_t((local.tactics.at(size_t(row)*0x58+0xc)/12+1)*12);
        else {const bool loan=random.range(0,6)==4;bid.months=uint8_t(random.range(1,loan?1:4)*12|(loan?128:0));if(loan&&random.range(0,10)>7)bid.months=0x98;}
        const uint32_t key=index<keys.size()?keys[index]:4*0x50;
        const auto amounts=bmh::computer_bid_amounts(listing.fee,listing.salary,bid.months,key,bidder.match_flags,0,true,random);
        bid.fee=amounts.fee;bid.salary=amounts.salary;bool warned=false;
        if(lure){const int local_player=int(projection.local_player(player));const auto board=size_t(row)*0x58;
            const uint32_t salary=uint32_t(local.tactics.at(board+0x50))|uint32_t(local.tactics.at(board+0x51))<<8|uint32_t(local.tactics.at(board+0x52))<<16|uint32_t(local.tactics.at(board+0x53))<<24;
            const uint32_t earns=bmh::expected_bonus(local,0,0xff,row,local_player,true)+salary;
            warned=uint32_t(random.range(98,120))*earns/100<bid.salary;
        }
        notice=PendingDeskOffer{seat,40,lure,warned};
    }
    // Source retains even a quote which found no bidder; OfferSlots remains free.
    state.world.transfer_market->rows[40]=listing;
    for(size_t n=0;n<4;++n)state.world.transfer_market->last_entry40_bidders[n]=listing.bids[n].bidder;state.world.transfer_market->entry40_history_known=true;return notice;
}
}
std::optional<PendingDeskOffer> prepare_manager_desk_offer(CareerSnapshot& state,uint8_t seat,
    const GameState& reference,WatcomRandom& random) {
    if(national_roles_active(state.world))return {};
    if(!state.prepared_event&&!annual_opening_ready(state)&&!annual_closing_ready(state))return initial_offer(state,seat,reference,random);
    const auto notice=prepare_unsolicited_offer(state,seat,reference,random,{true});
    if(!notice)return {};
    return PendingDeskOffer{seat,notice->row,notice->salary_only,notice->warned};
}
static DesktopEntryOutcome prepare_empty_desk_event(CareerSnapshot& state,ManagerActionOwner& owner,
    ManagerActionOwner expected,const GameState& reference,bool annual,bool closing=false) {
    state.validate();
    if(!owner.scope||owner.scope!=expected.scope||owner.revision!=expected.revision||owner.revision==std::numeric_limits<uint64_t>::max()||
        (closing?!annual_closing_ready(state):annual?!annual_opening_ready(state):bool(state.date||state.season))||!state.world.settings||state.world.managers.empty()||state.world.managers.size()!=state.world.settings->planned_managers||!state.calendar||(!closing&&state.calendar->next_day)||(state.world.catalogue.scope.club_play()&&!state.world.transfer_market)||!state.desktop||state.desktop->active_seat||state.desktop->initial_event||
        (!annual&&!closing&&!state.world.loans.empty())||(!closing&&(!state.calendar->schedule.leagues_on(0).empty()||!state.calendar->schedule.others_on(0).empty())))
        throw std::invalid_argument("world12: invalid initial empty event owner/phase");
    const auto before=state;if(closing)(void)completed_annual_titles(state);auto candidate=state;GameRandom game(state.game_seed);WatcomRandom random(state.watcom_seed);
    InitialDeskEvent event;event.matchday_number=uint16_t(random.range(0,10000));
    if(annual||closing)event.manager_order=candidate.desktop->manager_order;
    else for(const auto& manager:candidate.world.managers)event.manager_order.push_back(manager.seat);
    for(size_t i=0;i<event.manager_order.size();++i)std::swap(event.manager_order[i],event.manager_order[size_t(game.range(0,int(event.manager_order.size())-1))]);
    if(annual||closing){candidate.desktop->manager_order=event.manager_order;candidate.desktop->matchday_number=event.matchday_number;candidate.desktop->initial_event=event;}
    event.offer=(annual||closing)?prepare_manager_desk_offer(candidate,event.manager_order.front(),reference,random):initial_offer(candidate,event.manager_order.front(),reference,random);
    candidate.desktop->manager_order=event.manager_order;
    candidate.desktop->matchday_number=event.matchday_number;
    candidate.game_seed=game.seed();candidate.watcom_seed=random.seed();candidate.desktop->initial_event=std::move(event);
    ManagerActionOwner staged{owner.scope,0};const auto entered=enter_initial_manager_desktop(candidate,staged,staged,candidate.desktop->initial_event->manager_order.front(),reference);
    candidate.validate();
    if(state!=before||owner.scope!=expected.scope||owner.revision!=expected.revision)throw std::invalid_argument("world12: initial event owner changed during preparation");
    DesktopEntryOutcome outcome{game.calls()+entered.game_draws,random.calls()+entered.watcom_draws,entered.autosave_due};state=std::move(candidate);++owner.revision;return outcome;
}
DesktopEntryOutcome prepare_first_desk_event(CareerSnapshot& state,ManagerActionOwner& owner,ManagerActionOwner expected,const GameState& reference) {
    return prepare_empty_desk_event(state,owner,expected,reference,false);
}
DesktopEntryOutcome prepare_annual_closing_desk_event(CareerSnapshot& state,ManagerActionOwner& owner,ManagerActionOwner expected,const GameState& reference) {
    return prepare_empty_desk_event(state,owner,expected,reference,false,true);
}
DesktopEntryOutcome prepare_annual_desk_event(CareerSnapshot& state,ManagerActionOwner& owner,ManagerActionOwner expected,const GameState& reference) {
    return prepare_empty_desk_event(state,owner,expected,reference,true);
}
DesktopEntryOutcome advance_initial_manager(CareerSnapshot& state,ManagerActionOwner& owner,ManagerActionOwner expected,const GameState& reference) {
    state.validate();
    if(!owner.scope||owner.scope!=expected.scope||owner.revision!=expected.revision||owner.revision==std::numeric_limits<uint64_t>::max()||!state.desktop||!state.desktop->active_seat||!state.desktop->initial_event)
        throw std::invalid_argument("world12: stale or missing initial manager-loop owner");
    const auto& event=*state.desktop->initial_event;
    if(state.desktop->autosave_pending)throw std::invalid_argument("world12: pending autosave needs its file owner before the next desk");
    if(event.offer)throw std::invalid_argument("world12: current player offer needs its decision before leaving the desk");
    if(size_t(event.manager_position)+1>=event.manager_order.size())throw std::invalid_argument("world12: last manager needs the complete daily/weekly service owner");
    const auto before=state;auto candidate=state;WatcomRandom random(state.watcom_seed);
    auto& next=*candidate.desktop->initial_event;++next.manager_position;
    const auto seat=next.manager_order[next.manager_position];candidate.desktop->active_seat.reset();
    const auto offer=prepare_manager_desk_offer(candidate,seat,reference,random);
    candidate.desktop->initial_event->offer=offer;
    if(candidate.prepared_event)prepare_scheduled_pools(candidate,reference,random);
    candidate.watcom_seed=random.seed();candidate.desktop->active_seat.reset();
    ManagerActionOwner staged{owner.scope,0};const auto entered=enter_initial_manager_desktop(candidate,staged,staged,seat,reference);
    candidate.validate();
    if(state!=before||owner.scope!=expected.scope||owner.revision!=expected.revision)throw std::invalid_argument("world12: manager-loop owner changed during transition");
    DesktopEntryOutcome outcome{entered.game_draws,random.calls()+entered.watcom_draws,entered.autosave_due};state=std::move(candidate);++owner.revision;return outcome;
}

}
