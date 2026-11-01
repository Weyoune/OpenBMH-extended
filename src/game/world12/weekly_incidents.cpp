// SPDX-License-Identifier: GPL-3.0-only
#include "game/world12/weekly_incidents.hpp"
#include <algorithm>
#include <bit>
#include <set>
#include <stdexcept>
namespace bmh::world12 {
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
