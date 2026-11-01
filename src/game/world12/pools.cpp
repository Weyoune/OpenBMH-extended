// SPDX-License-Identifier: GPL-3.0-only
#include "game/world12/pools.hpp"
#include "game/world12/match_projection.hpp"
#include "game/matchday_setup.hpp"
#include <algorithm>
namespace bmh::world12 {
void prepare_scheduled_pools(CareerSnapshot& state,const GameState& reference,WatcomRandom& random) {
    state.validate();
    if(!state.prepared_event||state.prepared_event->desk_phase!=PreparedEvent::DeskPhase::desks||
       !state.desktop||!state.desktop->initial_event||state.desktop->active_seat||!state.world.settings)
        throw FormatError("world12: pools selection has no prepared manager entry");
    const auto before=state;auto candidate=state;auto staged=random;
    const auto seed=random.seed();const auto calls=random.calls();
    const auto& settings=*candidate.world.settings;std::vector<Fixture> selection;
    for(const auto& group:candidate.prepared_event->groups) {
        for(const auto& match:group.matches) {
            std::vector<ClubId> clubs;for(const auto& manager:candidate.world.managers)clubs.push_back(manager.club);
            for(auto club:{match.fixture.home,match.fixture.away})
                if(std::find(clubs.begin(),clubs.end(),club)==clubs.end())clubs.push_back(club);
            auto p=AiMatchProjection::create_for_humans(candidate,clubs,reference,
                {settings.mode,settings.difficulty,settings.foreign_rule,true});auto& local=p.state;
            MatchGroup native;native.desc.assign(0xd0,0);native.desc[0]=group.league?0:1;native.desc[2]=1;
            native.fixtures={int(p.local_club(match.fixture.home)),int(p.local_club(match.fixture.away))};
            // The validated prepared appointment has not been played. Materialize
            // its actual unplayed marker in this bounded, read-only Core workspace.
            local.results.assign(9*0x17c,0xcc);local.league_rounds.assign(9,0);native.cup_results={0xcccc};
            local.memory.add_block(layout::DeskActions_SubmitPoolsCoupon_RuntimeField,44);
            local.memory.add_block(layout::CareerSession_DispatchToto_RuntimeField,1);
            build_pools_coupon(local,{native},&staged);
            const auto count=local.memory.u8(layout::CareerSession_DispatchToto_RuntimeField);
            if(count>1)throw FormatError("world12: bounded pools selection exceeded its fixture");
            if(count)selection.push_back(match.fixture);
            if(selection.size()==11)break;
        }
        if(selection.size()==11)break;
    }
    candidate.prepared_event->pools_matches=std::move(selection);candidate.watcom_seed=staged.seed();candidate.validate();
    if(state!=before||random.seed()!=seed||random.calls()!=calls)throw FormatError("world12: pools preparation changed its source owner");
    state=std::move(candidate);random=staged;
}
}
