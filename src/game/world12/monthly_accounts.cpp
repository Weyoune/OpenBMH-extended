// SPDX-License-Identifier: GPL-3.0-only
#include "game/world12/monthly_accounts.hpp"
#include "game/month.hpp"
#include <stdexcept>
namespace bmh::world12 {
namespace {
enum class Phase { sponsors,accounts,investments };
EconomyPhaseOutcome run(CareerSnapshot& state,const GameState& reference,Phase phase,const TextDb* texts){
    state.validate();
    if(!state.world.economy||!state.world.markets||!state.world.settings||state.world.managers.empty())
        throw FormatError("world12: monthly accounts require an initialized career");
    for(const auto& manager:state.world.managers){
        if(!manager.career||!manager.career->season_services)
            throw FormatError("world12: monthly advertising state is unavailable");
        if(!state.world.economy->managers[manager.seat].monthly)
            throw FormatError("world12: older snapshot lacks monthly accounting state");
    }
    const auto before=state;auto candidate=state;std::vector<ClubId> clubs;
    for(const auto& manager:state.world.managers)clubs.push_back(manager.club);
    const auto& settings=*state.world.settings;
    auto projection=AiMatchProjection::create_for_humans(candidate,clubs,reference,
        {settings.mode,settings.difficulty,settings.foreign_rule,true});
    install_career_economy(projection,candidate.world);auto& g=projection.state;
    g.memory.add_block(layout::CareerDuration,1);g.memory.set8(layout::CareerDuration,settings.duration);
    g.texts=texts;
    switch(phase){
    case Phase::sponsors:expire_monthly_advertising(g);break;
    case Phase::accounts:
        for(size_t m=0;m<projection.human_managers();++m)monthly_manager_accounts(g,int(m));
        break;
    case Phase::investments:monthly_shares_and_loans(g,true);break;
    }
    projection.commit_match_state(candidate.world,true);capture_career_economy(candidate.world,projection);
    if(phase==Phase::investments){
        // Only prices change here. Retain canonical global-seat holdings;
        // projection-local holdings must never overwrite that permutation.
        auto& funds=candidate.world.markets->funds;
        for(size_t fund=0;fund<6;++fund)for(size_t b=0;b<4;++b)
            funds[fund*0x1c+b]=g.memory.u8(layout::FundBlock+uint32_t(fund*0x1c+b));
    }
    candidate.validate();if(state!=before)throw FormatError("world12: monthly account owner changed");
    const EconomyPhaseOutcome outcome{0,0,projection.human_managers()};state=std::move(candidate);return outcome;
}
}
EconomyPhaseOutcome expire_monthly_sponsors(CareerSnapshot& state,const GameState& reference,const TextDb& texts){return run(state,reference,Phase::sponsors,&texts);}
EconomyPhaseOutcome settle_monthly_accounts(CareerSnapshot& state,const GameState& reference){return run(state,reference,Phase::accounts,nullptr);}
EconomyPhaseOutcome settle_monthly_investments(CareerSnapshot& state,const GameState& reference){return run(state,reference,Phase::investments,nullptr);}
}
