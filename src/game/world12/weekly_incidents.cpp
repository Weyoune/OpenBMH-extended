// SPDX-License-Identifier: GPL-3.0-only
#include "game/world12/weekly_incidents.hpp"
#include "game/world12/european_campaign.hpp"
#include <algorithm>
#include <bit>
#include <set>
#include <stdexcept>
namespace bmh::world12 {
WeeklyIncidentContext current_weekly_club_relations(const CareerSnapshot& state){
    state.validate();const auto& world=state.world;
    if(!world.initial_competitions||!world.european_campaigns)throw FormatError("world12: current weekly titles/campaign ownership is unknown");
    const auto& fields=*world.initial_competitions;WeeklyIncidentContext out{state.date,state.season,{}};
    const auto country_title=[](const auto& rows,Country country){const auto at=std::find_if(rows.begin(),rows.end(),[&](const auto& row){return row.first==country;});if(at==rows.end()||!at->second.valid())throw FormatError("world12: current country title ownership is unknown");return at->second;};
    for(const auto& manager:world.managers){WeeklyClubRelations row;row.club=manager.club;const auto country=world.manager_country(manager.seat);
        row.advertising.champion=country_title(fields.champions,country)==manager.club;
        auto domestic=country_title(fields.domestic_holders,country);const auto kind=country==Country::germany?DomesticCup::german_cup:DomesticCup::fa_cup;
        for(auto stage=state.cups.rbegin();stage!=state.cups.rend();++stage)if(stage->cup==kind&&!stage->next_entry&&stage->complete()){const auto winners=stage->qualifiers();if(winners.size()!=1)throw FormatError("world12: completed domestic title has no unique owner");domestic=winners[0];break;}
        row.advertising.holders[0]=domestic==manager.club;
        if(const auto status=european_club_status(world,manager.club);status&&status->active)row.advertising.european_cup=uint8_t(status->competition);
        for(size_t cup=0;cup<3;++cup){const auto& campaign=(*world.european_campaigns)[cup];const auto holder=campaign.complete?campaign.winner:fields.holders[cup];if(!holder.valid())throw FormatError("world12: current European title ownership is unknown");row.advertising.holders[cup+1]=holder==manager.club;}
        out.clubs.push_back(row);
    }
    return out;
}
WeeklyIncidentOutcome begin_weekly_managers(CareerSnapshot& state,const GameState& reference,
    const TextDb& texts,const WeeklyIncidentContext& context,const WeeklyIncidentHooks& hooks) {
    state.validate();
    if(!state.world.economy||!state.world.transfer_market||!state.world.settings||state.world.managers.empty()||
       context.date!=state.date||context.season!=state.season||context.clubs.size()!=state.world.managers.size())
        throw std::invalid_argument("world12: invalid weekly incident ownership/context");
    std::set<ClubId> seen;
    for(const auto& relation:context.clubs)if(!seen.insert(relation.club).second||relation.advertising.european_cup>3||
        std::none_of(state.world.managers.begin(),state.world.managers.end(),[&](const auto& m){return m.club==relation.club;}))
        throw std::invalid_argument("world12: weekly relations do not match current human clubs");
    for(const auto& m:state.world.managers)if(!m.career||!m.career->season_services)throw std::invalid_argument("world12: weekly incidents require complete stadium/sponsor state");
    const auto relations=context;
    const auto before=state;auto candidate=state;std::vector<ClubId> clubs;
    for(const auto& m:state.world.managers)clubs.push_back(m.club);
    const auto settings=*state.world.settings;
    auto projection=AiMatchProjection::create_for_humans(candidate,clubs,reference,
        {settings.mode,settings.difficulty,settings.foreign_rule,true});
    auto& local=projection.state;local.texts=&texts;
    install_career_economy(projection,candidate.world);
    local.memory.add_block(layout::CompetitionSelection,4);local.memory.set32(layout::CompetitionSelection,1|(state.world.catalogue.scope.international?2:0));
    local.memory.add_block(layout::CareerSession_Invalid_RuntimeField2,4);local.memory.set32(layout::CareerSession_Invalid_RuntimeField2,uint32_t(state.world.transfer_market->checkpoint_game_seed));
    WeeklyIncidentOutcome outcome;GameRandom game(state.game_seed);WatcomRandom random(state.watcom_seed);MinuteReport report;
    WeeklyOfferHooks owned;
    owned.answer=[&](const GameState& g,const WeeklyOfferQuestion& question)->std::optional<bool>{
        if(!hooks.answer)return {};
        ++outcome.questions;return hooks.answer(g,projection.global_manager(uint8_t(question.manager)),question);
    };
    for(size_t m=0;m<projection.human_managers();++m){
        manager_offers(local,int(m),random,report,&owned);
        if(report.unsupported)throw FormatError(report.reason);
        facility_ageing(local,int(m),game,random,report);facility_damage(local,int(m),game,random);
        if(report.unsupported)throw FormatError(report.reason);
        if(local.memory.u8(layout::Savestate_Layout_ExecutableField)%3==0){
            const auto seat=projection.global_manager(uint8_t(m));
            const auto club=std::find_if(candidate.world.managers.begin(),candidate.world.managers.end(),[&](const auto& manager){return manager.seat==seat;})->club;
            const auto& relation=*std::find_if(relations.clubs.begin(),relations.clubs.end(),[&](const auto& c){return c.club==club;});
            advertising_offers(local,int(m),random,report,relation.advertising);
        }
        if(report.unsupported)throw FormatError(report.reason);
    }
    projection.commit_match_state(candidate.world);capture_career_economy(candidate.world,projection);
    candidate.world.transfer_market->checkpoint_game_seed=std::bit_cast<int32_t>(local.memory.u32(layout::CareerSession_Invalid_RuntimeField2));
    candidate.game_seed=game.seed();candidate.watcom_seed=random.seed();candidate.validate();
    if(state!=before)throw std::invalid_argument("world12: weekly incident owner changed during decision");
    outcome.game_draws=game.calls();outcome.watcom_draws=random.calls();outcome.managers=projection.human_managers();
    state=std::move(candidate);return outcome;
}
}
