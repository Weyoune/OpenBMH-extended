// SPDX-License-Identifier: GPL-3.0-only
#include "game/world12/season_awards.hpp"
#include "game/season_awards.hpp"
#include "game/world12/manager_finance.hpp"
#include "game/world12/economy.hpp"
#include "game/messages.hpp"
#include "game/week.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>
namespace bmh::world12 {
namespace {
[[noreturn]] void invalid(const char* reason){throw FormatError(reason);}
}
std::vector<AnnualAward> calculate_annual_awards(const World& world) {
    world.validate();std::vector<AnnualAward> awards;
    for(uint8_t kind=0;kind<3;++kind)for(const auto& league:world.catalogue.leagues) {
        if(!league.active||!world.catalogue.selectable(league.country))continue;
        AnnualAward award;award.kind=AnnualAwardKind(kind);award.league=league.id;
        award.prize=bmh::season_award_prize(league.local_index,kind);
        const auto& clubs=world.membership[league.id.value].clubs;
        if(kind<2) {
            std::vector<std::pair<ClubId,PlayerId>> identities;
            std::vector<SeasonScoringEntry> counters;
            for(auto club:clubs)for(const auto& slot:world.clubs.at(club).roster) {
                if(slot.empty())continue;
                const auto& person=world.players.at(slot.player).attributes;
                identities.push_back({club,slot.player});
                counters.push_back({uint8_t((person.w364>>2)&63),uint8_t((person.w368>>18)&63)});
            }
            const auto order=bmh::rank_season_scorers(counters,kind==0?2:6);
            if(!order.empty()) {
                const auto first=order.front();award.club=identities[first].first;award.player=identities[first].second;
                award.goals=counters[first].goals;award.assists=kind==1?counters[first].assists:0;
            }
        }else {
            std::vector<uint16_t> discipline;discipline.reserve(clubs.size());
            for(auto club:clubs)discipline.push_back(world.clubs.at(club).fairplay_points);
            award.club=clubs.at(bmh::season_fairplay_winner(discipline));
        }
        if(award.club.valid())for(const auto& manager:world.managers)if(manager.club==award.club)award.manager=manager.seat;
        awards.push_back(award);
    }
    return awards;
}
AnnualAwardsStage stage_annual_awards(const CareerSnapshot& state,const GameState& reference,const TextDb& texts) {
    state.validate();
    if(!state.progress||state.progress->phase!=ProgressPhase::season_end||!state.world.settings||!state.world.economy||
       state.world.managers.empty()||!state.date||month_code(state.date)!=7||month_code(state.progress->previous_date)==7||
       state.season==std::numeric_limits<uint32_t>::max())
        invalid("world12: annual awards require the owned July closing boundary");
    if(!state.world.annual_awards_completed)invalid("world12: annual award completion is unknown in this snapshot");
    if(*state.world.annual_awards_completed!=state.season)invalid("world12: annual awards are already paid or an earlier season is unclosed");
    for(const auto& league:state.world.catalogue.leagues)if(league.active) {
        if(state.league_rounds[league.id.value]!=league.rounds()||
           std::any_of(state.results.leagues[league.id.value].begin(),state.results.leagues[league.id.value].end(),[](const auto& score){return !score.played;}))
            invalid("world12: annual awards require complete actual league results");
    }
    const auto before=state;AnnualAwardsStage stage{state.world,calculate_annual_awards(state.world),state.season+1};
    auto local=reference;local.texts=&texts;
    const auto source=[&](uint32_t pointer)->const std::string&{
        const auto index=text_id(pointer);
        if(index>=texts.strings.size())invalid("world12: annual awards need installed Source text");
        return texts.strings[index];
    };
    for(const auto& award:stage.awards)if(award.manager) {
        auto& manager=*std::find_if(stage.world.managers.begin(),stage.world.managers.end(),[&](const auto& m){return m.seat==*award.manager;});
        if(!manager.finance)invalid("world12: annual prize account is unknown");
        const auto money=calculate_manager_booking(*manager.finance,state.world.settings->difficulty,-award.prize,true,true);
        if(!money.allowed)invalid("world12: forced Source annual prize was not booked");
        manager.finance=money.finance;
        const auto kind=uint32_t(award.kind);
        const auto text=source(layout::Season_SeasonAwards_TextPointer3)+
            source(layout::Season_SeasonAwards_TextPointer2+(award.kind==AnnualAwardKind::fair_play?4u:0u))+
            source(layout::Season_SeasonAwards_TextPointer4)+source(layout::Season_SeasonAwards_TextPointer+kind*4)+
            source(layout::Season_SeasonAwards_TextPointer5)+stage.world.catalogue.league(award.league).name+
            source(layout::Season_SeasonAwards_TextPointer6)+format_amount(local,award.prize)+".";
        EconomyMessage message;message.recipients=1u<<*award.manager;message.date=uint16_t(std::min<uint32_t>(state.date,65535));
        std::copy_n(text.begin(),std::min(text.size(),message.text.size()),message.text.begin());
        append_economy_message(*stage.world.economy,message);
    }
    stage.world.annual_awards_completed=stage.completed_seasons;
    stage.world.validate();if(state!=before)invalid("world12: annual awards owner changed during staging");return stage;
}
}
