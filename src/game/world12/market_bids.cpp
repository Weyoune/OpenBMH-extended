// SPDX-License-Identifier: GPL-3.0-only
#include "game/world12/market_bids.hpp"
#include "game/world12/match_projection.hpp"
#include "game/world12/standings.hpp"
#include "game/transfer_bid.hpp"
#include "game/match.hpp"
#include "game/lineup.hpp"
#include "game/schedule.hpp"
#include "game/month.hpp"
#include "game/prediction.hpp"
#include <algorithm>
#include <stdexcept>
namespace bmh::world12 {
namespace {
struct Candidate {
    ClubId id;size_t ordinal=0;bool domestic=false;
    int country=0,league=0,players=0,round=0,rank=-1;
    std::array<int,4> averages{},counts{};
    uint16_t flags=0;
};
std::vector<Candidate> candidates(const CareerSnapshot& state,const GameState& reference,MarketBidPolicy policy) {
    std::vector<Candidate> result;std::vector<ClubId> ai;size_t ordinal=0;
    for(auto table:{TableGroup::germany,TableGroup::england,TableGroup::europe})
        for(auto league_id:state.world.catalogue.simulation_leagues(table)) {
            const auto& league=state.world.catalogue.league(league_id);
            std::map<ClubId,int> ranks;
            if(state.league_rounds[league_id.value]>10&&!policy.completed_ranks) {
                TableQuery query;query.three_points=policy.three_points;
                for(const auto& m:state.world.managers)if(m.match_state&&m.match_state->point_deduction>>5==3&&state.world.clubs.at(m.club).league==league_id)
                    query.point_deductions[m.club]=m.match_state->point_deduction&31;
                for(const auto& row:compute_standings(state.world,state.results,league_id,state.league_rounds[league_id.value],query))ranks[row.club]=int(row.rank)-1;
            }
            for(auto id:state.world.membership[league_id.value].clubs) {
                const auto index=ordinal++;
                if(std::any_of(state.world.managers.begin(),state.world.managers.end(),[&](const auto& m){return m.club==id;}))continue;
                const auto& club=state.world.clubs.at(id);Candidate candidate;candidate.id=id;candidate.ordinal=index;
                candidate.domestic=league.playable;candidate.country=int(club.country);candidate.flags=club.match_flags;
                candidate.league=league.playable?league.local_index:club.country==Country::spain?5:club.country==Country::england?6:club.country==Country::france?7:8;
                candidate.round=state.league_rounds[league_id.value];if(candidate.round>10)candidate.rank=policy.completed_ranks?policy.completed_ranks->at(id):ranks.at(id);
                for(const auto& slot:club.roster)candidate.players+=!slot.empty();
                candidate.players+=int(std::count_if(state.world.loans.begin(),state.world.loans.end(),[&](const auto& loan){return loan.owner==id;}));candidate.players&=255;
                result.push_back(candidate);ai.push_back(id);
            }
        }
    for(size_t first=0;first<ai.size();first+=32) {
        const auto last=std::min(first+32,ai.size());const std::vector<ClubId> batch(ai.begin()+ptrdiff_t(first),ai.begin()+ptrdiff_t(last));
        const auto projection=AiMatchProjection::create(state.world,batch,reference);
        for(size_t n=first;n<last;++n)for(int role=0;role<4;++role) {
            const auto average=team_average(projection.state,int(n-first),uint16_t((1<<(role+4))+3));
            result[n].averages[size_t(role)]=int16_t(average.value);result[n].counts[size_t(role)]=average.players;
        }
    }
    return result;
}
bool add_bid(CareerSnapshot& state,uint8_t row,const std::vector<Candidate>& clubs,bool high,WatcomRandom& random,
    std::optional<uint8_t> forced_term={}) {
    auto& entry=state.world.transfer_market->rows.at(row);if(!entry)return false;
    auto& listing=*entry;size_t bid=0;while(bid<4&&listing.bids[bid].bidder.valid())++bid;if(bid==4)return false;
    const auto& person=state.world.players.at(listing.player);
    const int nationality=person.attributes.nationality()+((person.bonus&128)?32:0);
    const int strength=(person.attributes.technique()+person.attributes.condition())/2;
    std::vector<size_t> eligible;std::vector<uint32_t> keys;int last_need=0;uint8_t foreign_skip=0;
    for(size_t n=0;n<clubs.size();++n) {
        const auto& club=clubs[n];
        // Listed players are passed through a pool slot in Source. Its own=-1
        // therefore does not exclude the seller before scoring, or apply the
        // ordinary human-roster injury G gate. Seller rejection happens later.
        if(nationality<5&&nationality!=club.country&&foreign_skip++%10==0)continue;
        if(club.players>(club.domestic?21:15))continue;
        int16_t average=0;for(auto value:club.averages)average=int16_t(average+value);average=int16_t(average>>2);
        const int need=club.round>10?transfer_recruitment_need(club.league,club.rank,average,strength):0;
        TransferClubScoreInput input;input.averages=club.averages;input.counts=club.counts;input.nationality=nationality;
        input.region=club.country;input.position=person.attributes.position();input.player_strength=strength;input.players=club.players;
        input.need=need;input.flags=club.flags;input.large_squad=true;input.domestic_capacity=club.domestic;
        if(club.players<(club.domestic?17:14)) {
            const auto& market=*state.world.transfer_market;
            if(!market.entry40_history_known&&!market.rows[40])throw std::invalid_argument("world12: old snapshot lacks retained entry-40 bidder references");
            for(size_t n=0;n<4;++n)input.entry40_bids+=(market.rows[40]?market.rows[40]->bids[n].bidder:market.last_entry40_bidders[n])==club.id;
        }
        keys.push_back(uint32_t(transfer_club_score(input,random)+100000));eligible.push_back(n);last_need=need;
    }
    if(eligible.empty())return false;
    std::vector<int16_t> order(eligible.size());for(size_t n=0;n<order.size();++n)order[n]=int16_t(n);
    auto sorted=keys;quicksort_by_key(order.data(),sorted.data(),int(order.size()));
    const Candidate* chosen=nullptr;
    for(auto index:order){const auto& candidate=clubs[eligible[size_t(index)]];
        if(candidate.id==listing.seller||std::any_of(listing.bids.begin(),listing.bids.end(),[&](const auto& b){return b.bidder==candidate.id;}))continue;
        chosen=&candidate;break;
    }
    if(!chosen)return false;
    // Preserve the existing Core's chosen-absolute-index key approximation and
    // last-rated-club need, rather than substituting the winning score/need.
    const uint32_t key=chosen->ordinal<sorted.size()?sorted[chosen->ordinal]:4*0x50;
    uint8_t term;
    if(forced_term)term=*forced_term;
    else {
        bool loan=random.range(0,6)==4;if(!listing.seller.valid())loan=false;
        const int years=random.range(1,loan?1:4);term=uint8_t(years*12|(loan?128:0));
        if(loan&&random.range(0,10)>7)term=0x98;
    }
    const auto amounts=computer_bid_amounts(listing.fee,listing.salary,term,key,chosen->flags,last_need,high,random);
    listing.bids[bid]={chosen->id,amounts.fee,amounts.salary,term,{}};return true;
}
MarketBidOutcome run(CareerSnapshot& state,const GameState& reference,MarketBidPolicy policy,std::optional<uint8_t> single,bool high) {
    state.validate();if(!state.world.settings||!state.world.transfer_market)throw std::invalid_argument("world12: market bid phase is unavailable");
    if(single&&*single>=40)throw std::invalid_argument("world12: computer bids require a regular market row");
    const auto before=state;auto candidate=state;const auto ratings=candidates(state,reference,policy);WatcomRandom random(state.watcom_seed);MarketBidOutcome outcome;
    if(single)outcome.added=add_bid(candidate,*single,ratings,high,random);
    else for(uint8_t row=0;row<40;++row) {
        if(random.range(0,1)!=0)continue;
        const auto& listing=candidate.world.transfer_market->rows[row];if(!listing||listing->bids[3].bidder.valid())continue;
        if(random.range(0,1)==0)continue;
        do {outcome.added+=add_bid(candidate,row,ratings,false,random);}while(random.range(0,2)==0);
    }
    candidate.watcom_seed=random.seed();candidate.validate();if(state!=before)throw std::invalid_argument("world12: market owner changed during bidding");
    outcome.watcom_draws=random.calls();state=std::move(candidate);return outcome;
}
}
std::optional<UnsolicitedOffer> prepare_unsolicited_offer(CareerSnapshot& state,uint8_t seat,
    const GameState& reference,WatcomRandom& random,MarketBidPolicy policy) {
    state.validate();
    if(!state.world.settings||!state.world.transfer_market||!state.prepared_event||!state.desktop||
       !state.desktop->initial_event||state.desktop->initial_event->offer||state.desktop->active_seat||
       state.desktop->initial_event->manager_order[state.desktop->initial_event->manager_position]!=seat)
        throw FormatError("world12: scheduled unsolicited offer lacks its free decision owner");
    const auto manager=std::find_if(state.world.managers.begin(),state.world.managers.end(),[&](const auto& m){return m.seat==seat;});
    if(manager==state.world.managers.end()||!manager->match_state||!manager->tactics||!manager->tactics->backing)
        throw FormatError("world12: scheduled unsolicited offer lacks its manager contracts");
    if(state.world.settings->mode==1)return {};
    const auto before=state;auto candidate=state;auto staged=random;
    const auto seed=random.seed();const auto calls=random.calls();
    const auto decide=[&]() -> std::optional<UnsolicitedOffer> {
        if(staged.range(0,0x46-int16_t(manager->match_state->staff_ratings[0])/0x32)!=0)return {};
        uint8_t entry=40;
        for(;entry<44;++entry) {
            const auto& quote=candidate.world.transfer_market->rows[entry];
            if(!quote||std::none_of(quote->bids.begin(),quote->bids.end(),[](const auto& bid){return bid.bidder.valid();}))break;
        }
        if(entry==44)return {};
        const auto& club=candidate.world.clubs.at(manager->club);
        const auto eligible=[&](PlayerId player){return player.valid()&&!(candidate.world.players.at(player).attributes.w35c>>27)&&
            std::none_of(candidate.world.loans.begin(),candidate.world.loans.end(),[&](const auto& loan){return loan.player==player;});};
        if(std::none_of(club.roster.begin(),club.roster.end(),[&](const auto& row){return row.empty()||eligible(row.player);}))
            throw FormatError("world12: no eligible unsolicited-offer player; Original selection would not terminate");
        int row=0,empty=0;PlayerId player;
        for(;;) {
            row=staged.range(0,23);player=club.roster[size_t(row)].player;
            if(!player.valid()){if(empty++<100)continue;return {};}
            if(eligible(player))break;
        }
        const auto& settings=*candidate.world.settings;
        auto projection=AiMatchProjection::create_for_humans(candidate,{manager->club},reference,
            {settings.mode,settings.difficulty,settings.foreign_rule,policy.three_points});
        MarketListing listing;listing.player=player;listing.seller=manager->club;listing.human_seller=true;
        listing.asking_percent=uint8_t(staged.range(90,130));listing.demand_percent=100;
        market_value(projection.state,row,0,listing.asking_percent,listing.fee,listing.salary,1,0,staged);
        const bool lure=staged.range(0,1)!=0;
        candidate.world.transfer_market->rows[entry]=listing;
        const auto term=lure?std::optional<uint8_t>{uint8_t((projection.state.tactics.at(size_t(row)*0x58+0xc)/12+1)*12)}:std::nullopt;
        if(!add_bid(candidate,entry,candidates(candidate,reference,policy),true,staged,term))return {};
        const auto& bid=candidate.world.transfer_market->rows[entry]->bids[0];bool warned=false;
        if(lure) {
            const auto board=size_t(row)*0x58;uint32_t salary=0;
            for(size_t b=0;b<4;++b)salary|=uint32_t(projection.state.tactics.at(board+0x50+b))<<(b*8);
            const uint32_t earns=expected_bonus(projection.state,0,0xff,row,int(projection.local_player(player)),true)+salary;
            warned=uint32_t(staged.range(98,120))*earns/100<bid.salary;
        }
        return UnsolicitedOffer{entry,lure,warned};
    };
    const auto notice=decide();
    if(candidate.world.transfer_market->rows[40]) {
        for(size_t n=0;n<4;++n)candidate.world.transfer_market->last_entry40_bidders[n]=candidate.world.transfer_market->rows[40]->bids[n].bidder;
        candidate.world.transfer_market->entry40_history_known=true;
    }
    if(notice)candidate.desktop->initial_event->offer=PendingDeskOffer{seat,notice->row,notice->salary_only,notice->warned};
    candidate.watcom_seed=staged.seed();candidate.validate();
    if(state!=before||random.seed()!=seed||random.calls()!=calls)throw FormatError("world12: unsolicited offer changed its source owner");
    state=std::move(candidate);random=staged;return notice;
}
ClubId place_unlisted_player(CareerSnapshot& state,PlayerId player,const GameState& reference,WatcomRandom& random,MarketBidPolicy policy){
    state.validate();const auto before=state;auto candidate=state;auto staged=random;
    if(!candidate.world.transfer_market||!candidate.world.players.contains(player))throw FormatError("world12: missing unlisted player owner");
    for(const auto& [id,club]:candidate.world.clubs)for(const auto& slot:club.roster)if(slot.player==player)throw FormatError("world12: unlisted player still belongs to club");
    if(candidate.world.free_players){for(auto id:candidate.world.free_players->domestic)if(id==player)throw FormatError("world12: unlisted player still belongs to pool");for(auto id:candidate.world.free_players->foreign)if(id==player)throw FormatError("world12: unlisted player still belongs to foreign pool");}
    const auto clubs=candidates(candidate,reference,policy);const auto& person=candidate.world.players.at(player);
    const int nationality=person.attributes.nationality()+((person.bonus&128)?32:0),strength=(person.attributes.condition()+person.attributes.technique())/2;
    ClubId chosen;uint8_t foreign_skip=0;
    for(int pass=0;pass<2&&!chosen.valid();++pass){std::vector<int16_t> order;std::vector<uint32_t> keys;
        for(size_t n=0;n<clubs.size();++n){const auto& club=clubs[n];
            if(club.id==policy.released_from)continue;
            if(pass==0&&nationality<5&&nationality!=club.country&&foreign_skip++%10==0)continue;
            if(club.players>(club.domestic?21:15))continue;
            int16_t average=0;for(auto value:club.averages)average=int16_t(average+value);average=int16_t(average>>2);
            TransferClubScoreInput input;input.averages=club.averages;input.counts=club.counts;input.nationality=nationality;input.region=club.country;input.position=person.attributes.position();input.player_strength=strength;input.players=club.players;input.flags=club.flags;input.large_squad=true;input.domestic_capacity=club.domestic;
            input.need=club.round>10?transfer_recruitment_need(club.league,club.rank,average,strength):0;
            if(club.players<(club.domestic?17:14)){const auto& market=*candidate.world.transfer_market;if(!market.entry40_history_known&&!market.rows[40])throw FormatError("world12: release lacks retained bidder history");for(size_t b=0;b<4;++b)input.entry40_bids+=(market.rows[40]?market.rows[40]->bids[b].bidder:market.last_entry40_bidders[b])==club.id;}
            keys.push_back(uint32_t(transfer_club_score(input,staged)+100000));order.push_back(int16_t(n));
        }
        if(!order.empty()){quicksort_by_key(order.data(),keys.data(),int(order.size()));chosen=clubs.at(size_t(order[0])).id;}
    }
    if(!chosen.valid())throw FormatError("world12: no club for released player");
    auto& roster=candidate.world.clubs.at(chosen).roster;const auto free=std::find_if(roster.begin(),roster.end(),[](const auto& slot){return slot.empty();});if(free==roster.end())throw FormatError("world12: released player destination is full");*free={player,0};
    auto p=AiMatchProjection::create(candidate.world,{chosen},reference);auto_lineup(p.state,0,100);p.commit_match_state(candidate.world);
    candidate.validate();if(state!=before)throw FormatError("world12: release destination changed during scoring");state=std::move(candidate);random=staged;return chosen;
}
MarketBidOutcome add_computer_market_bid(CareerSnapshot& state,uint8_t row,const GameState& reference,MarketBidPolicy policy,bool high){return run(state,reference,policy,row,high);}
MarketBidOutcome generate_weekly_market_bids(CareerSnapshot& state,const GameState& reference,MarketBidPolicy policy){return run(state,reference,policy,{},false);}
}
