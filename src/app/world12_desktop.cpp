// SPDX-License-Identifier: GPL-3.0-only
#include "app/world12_desktop.hpp"
#include "game/world12/match_projection.hpp"
#include "game/desk.hpp"
#include "game/schedule.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>
namespace bmh::app::world12 {
using namespace bmh::world12;
void initialize_desktops(CareerSnapshot& state,const GameState& reference) {
    state.validate();
    if(state.desktop||!state.world.settings||
       state.world.managers.size()!=state.world.settings->planned_managers||state.date||state.season)
        throw std::invalid_argument("world12: invalid desktop initialization phase");
    DesktopRuntime runtime;
    for(const auto& manager:state.world.managers){
        ManagerDesktop desk;desk.seat=manager.seat;
        for(size_t record=0;record<desk.layout.size();++record)
            for(size_t byte=0;byte<16;++byte)
                desk.layout[record][byte]=reference.memory.u8(layout::Desktops+uint32_t(manager.seat)*0x360+uint32_t(record*16+byte));
        runtime.managers.push_back(desk);
    }
    auto candidate=state;candidate.desktop=std::move(runtime);candidate.validate();state=std::move(candidate);
}
DesktopEntryOutcome enter_initial_manager_desktop(CareerSnapshot& state,ManagerActionOwner& owner,
    ManagerActionOwner expected,uint8_t seat,const GameState& reference) {
    state.validate();
    if(!owner.scope||owner.scope!=expected.scope||owner.revision!=expected.revision||
       owner.revision==std::numeric_limits<uint64_t>::max()||!state.desktop||!state.world.settings||state.date||state.season||state.desktop->active_seat||
       std::any_of(state.league_rounds.begin(),state.league_rounds.end(),[](auto r){return r!=0;}))
        throw std::invalid_argument("world12: invalid desktop owner/state");
    const auto manager=std::find_if(state.world.managers.begin(),state.world.managers.end(),[&](const auto& m){return m.seat==seat;});
    if(manager==state.world.managers.end())throw std::invalid_argument("world12: unknown desktop manager");
    // Initial confirmations occupy seats in order. Source uses manager index
    // zero for monthly accounting; do not misapply that to an arbitrary sparse
    // or reordered roster, or to an English process-wide reference layout.
    for(size_t i=0;i<state.world.managers.size();++i)if(state.world.managers[i].seat!=i)
        throw std::invalid_argument("world12: initial desktop requires ordered manager seats");
    if(kEnglishCompetitions)throw std::invalid_argument("world12: desktop requires the German numeric reference profile");
    // Season-end sponsorship has its own financial/message owner. Do not run
    // that extra path accidentally with a stale Core template flag.
    if(reference.memory.has(layout::CareerSession_RuntimeError_RuntimeField4)&&reference.memory.u8(layout::CareerSession_RuntimeError_RuntimeField4))
        throw std::invalid_argument("world12: season-end desktop needs its sponsorship owner");
    const auto before=state;auto candidate=state;std::vector<ClubId> clubs;
    for(const auto& m:state.world.managers)clubs.push_back(m.club);
    const auto settings=*state.world.settings;
    auto projected=AiMatchProjection::create_for_humans(candidate,clubs,reference,
        {settings.mode,settings.difficulty,settings.foreign_rule,true});
    auto& local=projected.state;auto& runtime=*candidate.desktop;
    local.memory.add_block(layout::CareerDuration,1);local.memory.set8(layout::CareerDuration,settings.duration);
    local.autosave_month=runtime.autosave_month;local.autosave_months=runtime.autosave_months;
    size_t index=0;
    for(size_t i=0;i<projected.human_managers();++i){
        const auto global=projected.global_manager(uint8_t(i));if(global==seat)index=i;
        const auto& desk=*std::find_if(runtime.managers.begin(),runtime.managers.end(),[&](const auto& d){return d.seat==global;});
        local.manager_records.at(i*0xb0c+0x211)=desk.prestige;
        for(size_t r=0;r<54;++r)for(size_t b=0;b<16;++b)
            local.memory.set8(layout::Desktops+uint32_t(i*0x360+r*16+b),desk.layout[r][b]);
    }
    GameRandom game(state.game_seed);WatcomRandom random(state.watcom_seed);bool save=false;
    local.memory.set8(layout::ActiveManager,uint8_t(index));
    // Supply the hook to avoid consuming legacy save-container random bytes.
    // The typed owner creates its own snapshot only after this commit succeeds.
    bmh::enter_desk(local,int(index),game,random,[&](const GameState&,WatcomRandom&){save=true;});
    // The Core reference profile is German; this menu policy belongs to each
    // actual manager's country, not the process-global competition edition.
    if(candidate.world.manager_country(seat)==Country::england){
        const auto at=layout::Desktops+uint32_t(index*0x360+10*16+1);
        local.memory.set8(at,uint8_t(local.memory.u8(at)|0x10));
    }
    projected.commit_match_state(candidate.world);
    auto& desk=*std::find_if(runtime.managers.begin(),runtime.managers.end(),[&](const auto& d){return d.seat==seat;});
    desk.prestige=local.manager_records.at(index*0xb0c+0x211);
    for(size_t r=0;r<54;++r)for(size_t b=0;b<16;++b)
        desk.layout[r][b]=local.memory.u8(layout::Desktops+uint32_t(index*0x360+r*16+b));
    runtime.active_seat=seat;runtime.autosave_pending=save;runtime.autosave_month=uint16_t(local.autosave_month);runtime.autosave_months=uint16_t(local.autosave_months);
    candidate.game_seed=game.seed();candidate.watcom_seed=random.seed();candidate.validate();
    if(state!=before||owner.scope!=expected.scope||owner.revision!=expected.revision)
        throw std::invalid_argument("world12: desktop owner changed during entry");
    DesktopEntryOutcome result{game.calls(),random.calls(),save};state=std::move(candidate);++owner.revision;return result;
}
}
