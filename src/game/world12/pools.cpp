// SPDX-License-Identifier: GPL-3.0-only
#include "game/world12/pools.hpp"
#include "game/world12/match_projection.hpp"
#include "game/matchday_setup.hpp"
#include "game/world12/economy.hpp"
#include <set>
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
            std::vector<ClubId> clubs;if(group.national||group.friendly_national){for(const auto& record:*candidate.world.national_coaches)if(record.active)clubs.push_back(record.team);}else for(const auto& manager:candidate.world.managers)clubs.push_back(manager.club);
            for(auto club:{match.fixture.home,match.fixture.away})
                if(std::find(clubs.begin(),clubs.end(),club)==clubs.end())clubs.push_back(club);
            auto p=(group.national||group.friendly_national)?AiMatchProjection::create_for_nationals(candidate,clubs,reference,{settings.mode,settings.difficulty,settings.foreign_rule,true}):group.european?AiMatchProjection::create_for_european(candidate,clubs,reference,{settings.mode,settings.difficulty,settings.foreign_rule,true}):AiMatchProjection::create_for_humans(candidate,clubs,reference,
                {settings.mode,settings.difficulty,settings.foreign_rule,true});auto& local=p.state;
            MatchGroup native;native.desc.assign(0xd0,0);local.indoor=group.indoor;native.desc[0]=group.friendly?11:group.national?uint8_t(*group.national+1):group.regional_playoff?14:group.indoor?12:group.league?0:1;native.desc[2]=1;
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
void settle_scheduled_pools(CareerSnapshot& state,const PreparedEvent& event,const std::vector<PoolsMatchOutcome>& receipts,const GameState& reference,const TextDb& texts) {
    state.validate();if(!state.world.economy||!state.world.economy->pools)throw FormatError("world12: pools settlement has no saved coupon owner");
    if(!event.pools_matches||event.desk_phase!=PreparedEvent::DeskPhase::complete||state.prepared_event||!state.world.settings)throw FormatError("world12: pools settlement has no completed appointment");
    const auto& ledger=*state.world.economy->pools;
    if(ledger.appointment&&*ledger.appointment!=std::make_pair(state.season,uint32_t(event.day)))throw FormatError("world12: pools stakes belong to another appointment");
    const bool indoor=std::any_of(event.groups.begin(),event.groups.end(),[](const auto& group){return group.indoor;});
    const auto& matches=*event.pools_matches;if(indoor&&(matches.size()>4||ledger.appointment))throw FormatError("world12: indoor coupon cannot own a full pending stake");if(ledger.appointment&&matches.size()!=11)throw FormatError("world12: pending Toto stake has no full coupon");if(!indoor&&receipts.size()!=matches.size())throw FormatError("world12: incomplete pools regulation receipts");
    const bool national=std::any_of(event.groups.begin(),event.groups.end(),[](const auto& group){return group.national.has_value()||group.friendly_national;});
    const bool european=std::any_of(event.groups.begin(),event.groups.end(),[](const auto& group){return group.european.has_value();});
    const auto before=state;auto candidate=state;std::vector<ClubId> clubs;if(national){for(const auto& record:*candidate.world.national_coaches)if(record.active)clubs.push_back(record.team);}else for(const auto& m:candidate.world.managers)clubs.push_back(m.club);
    for(const auto& f:matches)for(auto id:{f.home,f.away})if(std::find(clubs.begin(),clubs.end(),id)==clubs.end())clubs.push_back(id);
    // An uncoached national day can own a genuinely empty coupon. Retain its
    // actual fixture field as the zero-manager Core workspace, never a club or
    // an invented coach merely to satisfy projection ownership.
    if(national&&clubs.empty())for(const auto& source:event.groups)for(const auto& match:source.matches)
        for(auto id:{match.fixture.home,match.fixture.away})if(std::find(clubs.begin(),clubs.end(),id)==clubs.end())clubs.push_back(id);
    const auto& settings=*candidate.world.settings;auto p=national?AiMatchProjection::create_for_nationals(candidate,clubs,reference,{settings.mode,settings.difficulty,settings.foreign_rule,true}):european?AiMatchProjection::create_for_european(candidate,clubs,reference,{settings.mode,settings.difficulty,settings.foreign_rule,true}):AiMatchProjection::create_for_humans(candidate,clubs,reference,{settings.mode,settings.difficulty,settings.foreign_rule,true});if(national){install_career_mailbox(p,candidate.world);install_career_pools(p,candidate.world);}else install_career_economy(p,candidate.world);auto& g=p.state;g.texts=&texts;g.date=event.day;
    g.memory.add_block(layout::DeskActions_SubmitPoolsCoupon_RuntimeField,44);g.memory.add_block(layout::CareerSession_DispatchToto_RuntimeField,1);g.memory.set8(layout::CareerSession_DispatchToto_RuntimeField,uint8_t(matches.size()));
    MatchGroup group;group.desc.assign(0xd0,0);group.desc[0]=1;group.desc[2]=uint8_t(matches.size());
    for(size_t n=0;n<matches.size();++n){const auto& f=matches[n];const auto found=std::find_if(receipts.begin(),receipts.end(),[&](const auto& row){return row.fixture==f;});
        if(!indoor&&(found==receipts.end()||found->outcome>2||std::count_if(receipts.begin(),receipts.end(),[&](const auto& row){return row.fixture==f;})!=1))throw FormatError("world12: invalid or duplicate pools receipt");
        const auto home=p.local_club(f.home),away=p.local_club(f.away);g.memory.set16(layout::DeskActions_SubmitPoolsCoupon_RuntimeField+uint32_t(n*4),uint16_t(home));g.memory.set16(layout::DeskActions_SubmitPoolsCoupon_RuntimeField+uint32_t(n*4+2),uint16_t(away));if(!indoor)g.memory.set8(layout::MatchdaySetup_EvaluatePoolsCoupon_RuntimeField+uint32_t(n),found->outcome);group.fixtures.push_back(int(home));group.fixtures.push_back(int(away));}
    settle_pools_coupon(g,{group});p.commit_match_state(candidate.world);if(national){capture_career_mailbox(candidate.world,p);capture_career_pools(candidate.world,p);}else capture_career_economy(candidate.world,p);candidate.world.economy->pools->appointment.reset();candidate.validate();
    if(state!=before)throw FormatError("world12: pools settlement changed its source owner");state=std::move(candidate);
}

}
