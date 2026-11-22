// SPDX-License-Identifier: GPL-3.0-only
// Implementation of the change of club (manager_change.hpp).
#include "game/world12/manager_change.hpp"
#include "game/world12/national_coach.hpp"
#include <algorithm>
namespace bmh::world12 {
namespace {
const Manager* manager_of(const World& world,uint8_t seat){
    const auto found=std::find_if(world.managers.begin(),world.managers.end(),[&](const auto& m){return m.seat==seat;});
    return found==world.managers.end()?nullptr:&*found;
}
}

std::string manager_change_refusal(const CareerSnapshot& state,uint8_t seat){
    const auto& world=state.world;
    const auto* manager=manager_of(world,seat);
    if(!manager||!world.extended||!world.extended->job_offers.contains(seat)||world.extended->job_offer_season!=state.season)
        return "Es liegt kein Angebot vor.";
    if(!world.settings||!world.economy||!world.markets||!state.desktop||!state.calendar||!state.transfers||
       !state.desktop->initial_event||state.desktop->active_seat!=seat||state.prepared_event)
        return "Ein Wechsel ist nur am ersten Schreibtisch der Saison möglich.";
    if(state.desktop->initial_event->offer)return "Zuerst muss das offene Transferangebot entschieden werden.";
    if(national_roles_active(world))return "Als Nationaltrainer ist kein Wechsel möglich.";
    // Credits between human managers bind two persons; a new start cannot simply drop them.
    const auto& economy=world.economy->managers;
    for(size_t lender=0;lender<4;++lender)if(economy[seat].loans[1+lender][0])return "Zuerst müssen die Kredite bei Mitspielern getilgt sein.";
    for(const auto& other:world.managers)if(other.seat!=seat&&economy[other.seat].loans[1+size_t(seat)][0])
        return "Zuerst müssen die Kredite an Mitspieler zurückgezahlt sein.";
    return {};
}

ManagerChangeOutcome change_manager_club(CareerSnapshot& state,uint8_t seat,ClubId club,
    const GameState& reference,const LeImage& country_rules){
    state.validate();
    if(const auto refusal=manager_change_refusal(state,seat);!refusal.empty())throw FormatError("world12: "+refusal);
    const auto& offers=state.world.extended->job_offers.at(seat);
    if(std::none_of(offers.begin(),offers.end(),[&](const auto& offer){return offer.club==club;}))
        throw FormatError("world12: the club made this manager no offer");
    const auto target=state.world.clubs.find(club);
    if(target==state.world.clubs.end()||target->second.kind!=ClubKind::club||!target->second.league.valid()||
       !state.world.catalogue.selectable(target->second.country)||manager_of(state.world,seat)->club==club)
        throw FormatError("world12: the offering club cannot be taken over");
    const auto visible=state.world.catalogue.player_leagues(target->second.country);
    if(std::find(visible.begin(),visible.end(),target->second.league)==visible.end())throw FormatError("world12: hidden league cannot be taken over");

    const auto before=state;auto candidate=state;auto& world=candidate.world;
    const auto index=size_t(manager_of(world,seat)-world.managers.data());
    const Manager old=world.managers[index];

    // 1. The person leaves the old club. Everything bound to the seat's club goes.
    auto& transfers=*candidate.transfers;
    transfers.loans.erase(std::remove_if(transfers.loans.begin(),transfers.loans.end(),[&](const auto& loan){return loan.manager==seat;}),transfers.loans.end());
    transfers.balance[seat]=0;transfers.history[seat].clear();
    if(world.transfer_market){
        for(auto& row:world.transfer_market->rows)if(row){
            if(row->seller==old.club){row.reset();continue;}
            std::array<MarketBid,4> remaining{};size_t count=0;
            for(const auto& bid:row->bids)if(bid.bidder.valid()&&bid.bidder!=old.club)remaining[count++]=bid;
            row->bids=remaining;
        }
        for(auto& bidder:world.transfer_market->last_entry40_bidders)if(bidder==old.club)bidder={};
    }
    if(candidate.friendly_plan){
        auto& pending=candidate.friendly_plan->pending;
        const auto gone=std::remove_if(pending.begin(),pending.end(),[&](const auto& booking){
            return booking.payer==seat||(!booking.national&&(booking.fixture.home==old.club||booking.fixture.away==old.club));});
        if(gone!=pending.end()){pending.erase(gone,pending.end());++candidate.friendly_plan->revision;}
    }
    // Shares and fund units of the seat (two bytes per seat in each paper's record) are given up.
    for(size_t fund=0;fund<6;++fund)for(size_t byte=0;byte<2;++byte)world.markets->funds[fund*0x1c+0x14+size_t(seat)*2+byte]=0;
    for(size_t stock=0;stock<12;++stock)for(size_t byte=0;byte<2;++byte)world.markets->stocks[stock*0x5c+0x50+size_t(seat)*2+byte]=0;
    world.clubs.at(old.club).presentation_category.reset();   // a computer club again
    world.managers.erase(world.managers.begin()+std::ptrdiff_t(index));
    world.extended->job_offers.erase(seat);

    // 2. The same person takes over the new club like a manager who joins the career.
    const auto loans_of_club=size_t(std::count_if(world.loans.begin(),world.loans.end(),[&](const auto& loan){return loan.owner==club;}));
    if(loans_of_club>3||transfers.loans.size()+loans_of_club>12)throw FormatError("world12: takeover exceeds the existing human loan-contract capacity");
    WatcomRandom random(candidate.watcom_seed);
    seat_manager_at_club(candidate,{seat,club,target->second.league,old.name,old.portrait},reference,country_rules,random);
    std::rotate(world.managers.begin()+std::ptrdiff_t(index),world.managers.end()-1,world.managers.end());   // keep the list order
    auto& fresh=world.managers[index];

    // 3. The career record stays with the person.
    if(old.career&&fresh.career){
        fresh.career->season_records=old.career->season_records;
        fresh.career->first_recorded_season=old.career->first_recorded_season;
        fresh.career->cup_titles=old.career->cup_titles;
        if(old.career->annual_records&&fresh.career->annual_records){
            fresh.career->annual_records->league_titles=old.career->annual_records->league_titles;
            fresh.career->annual_records->player_awards=old.career->annual_records->player_awards;
        }
        // Personal view settings of the screens.
        fresh.career->strengths_display_word=old.career->strengths_display_word;
        fresh.career->pairings_display_byte=old.career->pairings_display_byte;
        fresh.career->table_display_word=old.career->table_display_word;
        fresh.career->other_games_view=old.career->other_games_view;
        fresh.career->statistics_flags=old.career->statistics_flags;
        fresh.career->player_statistics_flags=old.career->player_statistics_flags;
        fresh.career->stadium_curve_flags=old.career->stadium_curve_flags;
    }
    if(old.match_state&&fresh.match_state)fresh.match_state->best_streaks=old.match_state->best_streaks;

    candidate.watcom_seed=random.seed();candidate.validate();
    if(state!=before)throw FormatError("world12: manager change source changed during takeover");
    const auto draws=random.calls();state=std::move(candidate);
    return {old.club,club,draws};
}
}
