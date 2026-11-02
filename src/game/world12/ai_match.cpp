// SPDX-License-Identifier: GPL-3.0-only
#include "game/world12/ai_match.hpp"
#include "game/world12/cup_match.hpp"
#include "game/cup.hpp"
#include "formats/le.hpp"
#include <set>
#include <algorithm>
#include "game/world12/save.hpp"
#include "game/driver.hpp"
#include "game/lineup.hpp"
#include "game/match.hpp"
#include "game/matchday_setup.hpp"
#include "game/matchday.hpp"
#include <limits>
#include <stdexcept>
#include <type_traits>
namespace bmh::world12 {
static_assert(std::is_nothrow_move_assignable_v<World>);
namespace {
AiLeagueRoundOutcome run_ai_league_matches(World& world,const GameState& base,const std::vector<Fixture>& fixtures,
    const std::vector<AiLeagueConditions>& conditions,GameRandom& game,WatcomRandom& random,bool settle,
    const CareerSnapshot* human_snapshot=nullptr,HumanProjectionPolicy policy={},const HumanRoundHooks* hooks=nullptr,
    const KnockoutStage* cup=nullptr,const LeImage* cup_source=nullptr,CupRoundOutcome* cup_out=nullptr) {
    if(fixtures.empty() || fixtures.size()>(cup?32u:10u) || conditions.size()!=fixtures.size())throw std::invalid_argument("world12: incomplete AI league match batch");
    std::vector<ClubId> participants;LeagueId league;
    for(size_t m=0;m<fixtures.size();++m) {
        const auto fixture=fixtures[m];const auto& c=conditions[m];
        if(fixture.home==fixture.away || c.month<1 || c.month>12 || c.temperature<-128 || c.temperature>127 || c.rain<0 || c.rain>7 ||
            c.spectators>uint32_t(std::numeric_limits<int>::max()) || c.referee>=c.referee_properties.size() || c.presentation_buttons.size()<0xb4)
            throw std::invalid_argument("world12: incomplete AI league match conditions");
        if(c.month!=conditions[0].month || c.date!=conditions[0].date || c.referee_properties!=conditions[0].referee_properties || c.presentation_buttons!=conditions[0].presentation_buttons || c.teams_prepared!=conditions[0].teams_prepared)
            throw std::invalid_argument("world12: inconsistent shared league-round conditions");
        const auto& home=world.clubs.at(fixture.home);const auto& away=world.clubs.at(fixture.away);
        if(!home.league.valid()||!away.league.valid()||!world.catalogue.league(home.league).active||!world.catalogue.league(away.league).active||
           (!cup&&(home.league!=away.league||(league.valid()&&league!=home.league)))||
           (cup&&(home.country!=cup_country(cup->cup)||away.country!=cup_country(cup->cup))))
            throw std::invalid_argument("world12: match fixture crosses its competition ownership");
        league=home.league;participants.push_back(fixture.home);participants.push_back(fixture.away);
    }
    if(human_snapshot&&(human_snapshot->world!=world||conditions[0].date!=human_snapshot->date))throw std::invalid_argument("world12: human forecast snapshot/date does not match round owner");
    const bool human_mode=human_snapshot&&(!cup||std::any_of(world.managers.begin(),world.managers.end(),[&](const auto& m){return std::find(participants.begin(),participants.end(),m.club)!=participants.end();}));
    auto projection=human_mode?AiMatchProjection::create_for_humans(*human_snapshot,participants,base,policy):AiMatchProjection::create(world,participants,base);auto& local=projection.state;
    local.indoor=false;local.lineups_frozen=false;local.pools_open=false;local.date=conditions[0].date;
    local.chance_rain=conditions[0].rain;local.chance_temperature=conditions[0].temperature;
    uint8_t group_type=0,group_flags=2;uint32_t source_entry=0;
    if(cup) {
        if(!cup_source||!cup_out)throw FormatError("world12: cup match owner lacks verified source rules");
        const auto verified=KnockoutStage::from_source(cup->cup,*cup_source,cup->source_entry,cup->rounds.front().fixtures,world,cup->seeded);
        if(verified.flags!=cup->flags||verified.planned_rounds!=cup->planned_rounds||verified.next_entry!=cup->next_entry)
            throw FormatError("world12: cup match rules changed since its draw");
        source_entry=cup_entry_address(cup->source_entry);
        group_type=uint8_t(cup_source->u16(source_entry+0x18)+1);group_flags=cup->flags;
        if((group_flags&5)==5&&cup->next_leg()==1)group_flags&=0xf7;
        if(group_type==1&&fixtures.size()==1)group_flags&=0xfd;
        std::set<uint8_t> visited;int entry=cup->source_entry;
        while(entry>=0) {
            if(entry>=kCupEntries||!visited.insert(uint8_t(entry)).second)throw FormatError("world12: cyclic/missing cup predecessor metadata");
            const auto address=cup_entry_address(uint8_t(entry));
            if(!cup_source->contains(address+0x27))throw FormatError("world12: missing verified cup predecessor rules");
            local.memory.add_block(address,0x28);
            for(uint32_t b=0;b<0x28;++b)local.memory.set8(address+b,cup_source->u8(address+b));
            const auto* history=cup;
            if(entry!=cup->source_entry) {
                if(!human_snapshot)throw FormatError("world12: cup predecessor requires the complete saved programme");
                const auto found=std::find_if(human_snapshot->cups.begin(),human_snapshot->cups.end(),[&](const auto& stage){return stage.cup==cup->cup&&stage.source_entry==entry;});
                if(found==human_snapshot->cups.end()||!found->complete())throw FormatError("world12: incomplete saved cup predecessor");
                history=&*found;
            }
            const auto legs=history->two_legs()?2u:1u;
            const auto played=history->complete()?history->rounds.size()*legs:(history->rounds.size()-1)*legs+(history->next_leg()==2?1u:0u);
            if(played>255)throw FormatError("world12: cup leg cursor exceeds native byte");
            local.memory.set8(address+0x1d,uint8_t(played));
            entry=int8_t(cup_source->u8(address+0x15));
        }
    }
    for(size_t team=0;team<participants.size();++team) {
        if(!conditions[0].teams_prepared)bmh::prepare_team(local,int(team),match_competition_class(group_type),int(team));
        const auto formation=size_t(local.formation(int(team)))*22;
        if(formation+22>local.formations.size())throw std::invalid_argument("world12: missing Original formation table");
        for(size_t row=0;row<11;++row)if(local.formations[formation+row]>=24 || local.formations[formation+11+row]>8)
            throw std::invalid_argument("world12: invalid Original formation coordinates");
        bmh::auto_lineup(local,int(team),local.manager_of(int(team))>=0?0xe4:100);
        if(local.manager_of(int(team))>=0){bmh::prepare_lineup_entry(local,int(team));const auto manager=local.manager_of(int(team));
            for(size_t row=0;row<24;++row){const auto slot=local.squad_slots[team*24+row];local.tactics[size_t(manager)*0x840+row*0x58+0x10]=uint8_t(slot&0x8000?0:slot&0x4000?0x7f:0x7e);}}
        const auto fit=bmh::team_average(local,int(team),0x1f1,nullptr);
        if(fit.players<8)throw std::invalid_argument("world12: short-squad forfeit needs the full matchday settlement owner");
    }
    bmh::reset_match_played_flags(local);
    auto staged_game=game;auto staged_random=random;
    const auto initial_game_seed=game.seed();const auto initial_watcom_seed=random.seed();
    const auto initial_game_calls=game.calls(),initial_watcom_calls=random.calls();
    bmh::MatchGroup group;group.desc.assign(0xd0,0);group.desc[0]=group_type;group.desc[1]=group_flags;group.desc[2]=uint8_t(fixtures.size());
    const auto competition=cup?source_entry:uint32_t(bmh::match_team_league(local,0));
    for(size_t b=0;b<4;++b)group.desc[0xcc+b]=uint8_t(competition>>(8*b));
    if(cup&&cup->two_legs()&&cup->next_leg()==2)group.cup_other_leg=cup->rounds.back().first_leg;
    for(size_t m=0;m<fixtures.size();++m) {
        const auto& c=conditions[m];group.desc[0x10+m]=c.referee;group.desc[0x30+m]=uint8_t(c.temperature);group.desc[0x50+m]=uint8_t(c.rain);
        group.fixtures.push_back(int(2*m));group.fixtures.push_back(int(2*m+1));if(cup)group.cup_results.push_back(0);else group.results.push_back(0);group.end_minute.push_back(0);group.spectators.push_back(c.spectators);
        bmh::MatchSetup setup;setup.home=int(2*m);setup.away=int(2*m+1);setup.match_index=int(m);setup.climate_effect=(group_flags&2)!=0;
        setup.month=c.month;setup.temperature=c.temperature;setup.spectators=int(c.spectators);
        const auto sides=bmh::build_side_records(local,setup,staged_random,&staged_game);
        for(const auto& side:sides)group.records.insert(group.records.end(),side.b.begin(),side.b.end());
    }
    std::vector<bmh::MatchGroup> groups;groups.push_back(std::move(group));
    bmh::MatchEngine engine(local,groups,staged_game,staged_random,conditions[0].referee_properties);
    engine.month=conditions[0].month;engine.buttons=conditions[0].presentation_buttons;engine.leave_screens=!human_mode||(hooks&&hooks->decline_unpresented_events);
    if(hooks&&hooks->live_event)engine.live_event=[&](const bmh::LiveMatchEvent& event){hooks->live_event(projection,engine,event);};
    std::vector<size_t> pool_rows;for(size_t m=0;m<conditions.size();++m)if(conditions[m].pools_selected)pool_rows.push_back(m);
    if(pool_rows.size()>11)throw FormatError("world12: pools receipt exceeds eleven fixtures");
    if(!pool_rows.empty()) {
        local.memory.add_block(layout::DeskActions_SubmitPoolsCoupon_RuntimeField,44);local.memory.add_block(layout::MatchdaySetup_EvaluatePoolsCoupon_RuntimeField,11);local.memory.add_block(layout::CareerSession_DispatchToto_RuntimeField,1);
        for(size_t n=0;n<pool_rows.size();++n){const auto m=pool_rows[n];local.memory.set16(layout::DeskActions_SubmitPoolsCoupon_RuntimeField+uint32_t(n*4),uint16_t(m*2));local.memory.set16(layout::DeskActions_SubmitPoolsCoupon_RuntimeField+uint32_t(n*4+2),uint16_t(m*2+1));}
        local.pools_open=true;
    }
    if(hooks&&hooks->before_matchday)hooks->before_matchday(projection,engine);
    bmh::MinuteReport report;
    bmh::play_matchday(engine,local,groups,conditions[0].presentation_buttons,staged_game,staged_random,report,
        hooks&&hooks->after_minute?std::function<void(int)>([&](int minute){hooks->after_minute(projection,engine,minute);}):std::function<void(int)>{});
    if(report.unsupported)throw std::runtime_error("world12: Original AI match could not complete: "+report.reason);
    const auto& finished=groups[0];
    for(auto minute:finished.end_minute)if(minute<90 || minute>(cup?124:94))throw std::runtime_error("world12: Original AI league match did not reach full time");
    if(settle) {
        for(auto score:finished.results)if((score&0xcc)==0xcc)throw std::invalid_argument("world12: result collides with the legacy post-match sentinel");
        bmh::process_group(local,finished,staged_game,staged_random,report);
        if(report.unsupported)throw std::runtime_error("world12: AI post-match settlement could not complete: "+report.reason);
    }
    if(game.seed()!=initial_game_seed || game.calls()!=initial_game_calls || random.seed()!=initial_watcom_seed || random.calls()!=initial_watcom_calls)
        throw std::invalid_argument("world12: random stream ownership changed during the match");
    AiLeagueRoundOutcome outcome;outcome.game_draws=staged_game.calls()-initial_game_calls;outcome.watcom_draws=staged_random.calls()-initial_watcom_calls;
    if(!pool_rows.empty()) {
        if(local.pools_open||local.memory.u8(layout::CareerSession_DispatchToto_RuntimeField)!=pool_rows.size())throw FormatError("world12: Core did not evaluate the selected pools fixtures");
        for(size_t n=0;n<pool_rows.size();++n){const auto value=local.memory.u8(layout::MatchdaySetup_EvaluatePoolsCoupon_RuntimeField+uint32_t(n));if(value>2)throw FormatError("world12: invalid Core pools outcome");outcome.pools.push_back({fixtures[pool_rows[n]],value});}
    }
    if(cup)cup_out->pools=outcome.pools;
    if(cup){cup_out->scores=finished.cup_results;cup_out->end_minutes=finished.end_minute;cup_out->game_draws=outcome.game_draws;cup_out->watcom_draws=outcome.watcom_draws;}
    else for(size_t m=0;m<fixtures.size();++m)outcome.matches.push_back({fixtures[m],{true,uint8_t(finished.results[m]>>4),uint8_t(finished.results[m]&15)},finished.end_minute[m],0,0,settle});
    projection.commit_match_state(world);game=staged_game;random=staged_random;return outcome;
}
}
AiLeagueOutcome simulate_ai_league_match(World& world,const GameState& base,Fixture fixture,const AiLeagueConditions& conditions,GameRandom& game,WatcomRandom& random) {
    auto result=run_ai_league_matches(world,base,{fixture},{conditions},game,random,false);auto outcome=result.matches.front();outcome.game_draws=result.game_draws;outcome.watcom_draws=result.watcom_draws;return outcome;
}
AiLeagueOutcome simulate_and_settle_ai_league_match(World& world,const GameState& base,Fixture fixture,const AiLeagueConditions& conditions,GameRandom& game,WatcomRandom& random) {
    auto result=run_ai_league_matches(world,base,{fixture},{conditions},game,random,true);auto outcome=result.matches.front();outcome.game_draws=result.game_draws;outcome.watcom_draws=result.watcom_draws;return outcome;
}
AiLeagueRoundOutcome simulate_and_settle_ai_league_round(World& world,const GameState& base,LeagueId league,int round,const std::vector<AiLeagueConditions>& conditions,GameRandom& game,WatcomRandom& random) {
    return run_ai_league_matches(world,base,league_fixtures(world,league,round),conditions,game,random,true);
}
AiLeagueRoundOutcome simulate_and_settle_human_league_round(CareerSnapshot& staged,const GameState& base,LeagueId league,int round,
    const std::vector<AiLeagueConditions>& conditions,HumanProjectionPolicy policy,GameRandom& game,WatcomRandom& random,const HumanRoundHooks& hooks) {
    const auto before=staged;auto candidate=staged;auto staged_game=game;auto staged_random=random;
    const auto game_seed=game.seed();const auto game_calls=game.calls();const auto random_seed=random.seed();const auto random_calls=random.calls();
    const auto fixtures=league_fixtures(candidate.world,league,round);
    const auto outcome=run_ai_league_matches(candidate.world,base,fixtures,conditions,staged_game,staged_random,true,&candidate,policy,&hooks);
    candidate.game_seed=staged_game.seed();candidate.watcom_seed=staged_random.seed();candidate.validate();
    if(staged!=before||game.seed()!=game_seed||game.calls()!=game_calls||random.seed()!=random_seed||random.calls()!=random_calls)
        throw std::invalid_argument("world12: human round snapshot/RNG owner changed");
    staged=std::move(candidate);game=staged_game;random=staged_random;return outcome;
}


CupRoundOutcome simulate_and_settle_domestic_cup_round(CareerSnapshot& state,const GameState& base,
    const KnockoutStage& cup,const LeImage& source,const std::vector<AiLeagueConditions>& conditions,
    HumanProjectionPolicy policy,GameRandom& game,WatcomRandom& random,const HumanRoundHooks& hooks) {
    state.validate();cup.validate(state.world);
    const auto stored=std::find(state.cups.begin(),state.cups.end(),cup);
    if(stored==state.cups.end()||cup.complete()||conditions.empty()||
       std::any_of(conditions.begin(),conditions.end(),[&](const auto& c){return c.date!=state.date;}))
        throw FormatError("world12: missing/stale cup match programme/date");
    const auto before=state;auto candidate=state;auto staged_game=game;auto staged_random=random;
    const auto game_seed=game.seed();const auto random_seed=random.seed();
    const auto game_calls=game.calls(),random_calls=random.calls();
    CupRoundOutcome outcome;
    run_ai_league_matches(candidate.world,base,cup.next_fixtures(),conditions,staged_game,staged_random,true,
        &candidate,policy,&hooks,&cup,&source,&outcome);
    candidate.game_seed=staged_game.seed();candidate.watcom_seed=staged_random.seed();candidate.validate();
    if(state!=before||game.seed()!=game_seed||random.seed()!=random_seed||game.calls()!=game_calls||random.calls()!=random_calls)
        throw FormatError("world12: cup match snapshot/random owner changed");
    state=std::move(candidate);game=staged_game;random=staged_random;return outcome;
}

}
