// SPDX-License-Identifier: GPL-3.0-only
#include "game/world12/national_annual_players.hpp"
#include "game/world12/inactive_roster_projection.hpp"
#include "game/world12/market_bids.hpp"
#include "game/season.hpp"
#include "game/turn.hpp"
#include <algorithm>
#include <set>
namespace bmh::world12 {
NationalAnnualPlayers stage_national_annual_players(const CareerSnapshot& source,uint32_t target,const GameState& reference,const TextDb& texts) {
    source.validate();GameState date;date.date=source.date;if(target!=uint64_t(source.season)+1||target>32767||!is_season_end(date))throw FormatError("world12: national annual population needs its actual July closing year");const auto receipt=inactive_population_receipt(source.world);const auto order=inactive_origin_order(source.world);
    NationalAnnualPlayers outcome;outcome.source_binding=source;outcome.world=source.world;auto& world=outcome.world;GameRandom game(source.game_seed);WatcomRandom random(source.watcom_seed);std::set<PlayerId> visited;
    const auto pass=[&](std::optional<ClubId> club){auto projection=InactiveRosterProjection::create(world,club,reference,true);auto& local=projection.state;local.texts=&texts;local.date=source.date;local.season=int(target);local.memory.add_block(layout::Career_RemoveJoinLoans_RuntimeField,200*4);for(uint32_t i=0;i<200;++i)local.memory.set32(layout::Career_RemoveJoinLoans_RuntimeField+i*4,0);
        auto working=world;std::set<PlayerId> removed;std::set<ClubId> overflow_clubs;
        PlayerReleaseDestination destination;destination.pool_slot=[&](const GameState& state,int player){const auto nationality=state.players.at(size_t(player)).nationality()+((state.player_bonus.at(size_t(player))&128)?32:0);const bool domestic=nationality==int(receipt.primary);const int first=domestic?runtime_dimensions.free_first:runtime_dimensions.foreign_free_first,count=domestic?80:20;for(int n=0;n<count;++n)if(state.squad_slots.at(size_t(first+n))==0xfff)return first+n;return -1;};
        destination.overflow=[&](GameState& state,int slot,GameRandom&,WatcomRandom& random,MinuteReport&){projection.capture(working,true);const auto player=projection.global_player(state.squad_slots.at(size_t(slot))&0xfff);
            if(club){auto origin=std::find_if(working.national_origins->begin(),working.national_origins->end(),[&](const auto& record){return record.club.id==*club;});auto& roster=origin->club.roster;const auto at=std::find_if(roster.begin(),roster.end(),[&](const auto& row){return row.player==player;});if(at==roster.end())throw FormatError("world12: national annual overflow lost its actual origin");std::move(at+1,roster.end(),at);roster.back()={};}
            else{for(auto& id:working.free_players->domestic)if(id==player)id={};for(auto& id:working.free_players->foreign)if(id==player)id={};}
            const auto buyer=place_unlisted_national_player(working,player,reference,random,club?*club:ClubId{});overflow_clubs.insert(buyer);for(size_t n=0;n<state.players.size();++n){const auto& person=working.players.at(projection.global_player(uint16_t(n)));state.players[n]=person.attributes;state.player_cards[n]=person.cards;state.player_bonus[n]=person.bonus;}
        };
        SeasonPlayerRenewalHooks hooks;hooks.first_visit=[&](int player){return visited.insert(projection.global_player(uint16_t(player))).second;};hooks.release=[&](GameState& state,int slot,GameRandom& game,WatcomRandom& random,MinuteReport& report){const auto player=projection.global_player(state.squad_slots.at(size_t(slot))&0xfff);if(!removed.insert(player).second)throw FormatError("world12: national annual player was released twice");return release_player(state,slot,game,random,report,&destination);};MinuteReport report;const auto run=[&](int first,int last){if(!renew_season_player_slots(local,first,last,game,random,report,hooks)||report.unsupported)throw FormatError(report.unsupported?report.reason:"world12: national annual player release failed");};
        if(club)run(0,24);else{run(runtime_dimensions.free_first,runtime_dimensions.free_first+80);run(runtime_dimensions.foreign_free_first,runtime_dimensions.foreign_free_first+20);}projection.capture(world,true);
        for(auto id:overflow_clubs){const auto origin=std::find_if(working.national_origins->begin(),working.national_origins->end(),[&](const auto& record){return record.club.id==id;});auto destination=std::find_if(world.national_origins->begin(),world.national_origins->end(),[&](const auto& record){return record.club.id==id;});destination->club=origin->club;for(const auto& row:origin->club.roster)if(!row.empty())world.players.at(row.player)=working.players.at(row.player);}
        outcome.released+=removed.size();world.validate();
    };
    for(auto club:order)pass(club);pass({});for(auto& origin:*world.national_origins)origin.club.fairplay_points=0;world.validate();outcome.game_seed=game.seed();outcome.watcom_seed=random.seed();outcome.game_draws=game.calls();outcome.watcom_draws=random.calls();outcome.visited=visited.size();if(source!=outcome.source_binding)throw FormatError("world12: national annual population owner changed during renewal");return outcome;
}
}
