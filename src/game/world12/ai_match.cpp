// SPDX-License-Identifier: GPL-3.0-only
#include "game/world12/ai_match.hpp"
#include "game/world12/regional_playoff_match.hpp"
#include "game/world12/cup_match.hpp"
#include "game/world12/european_match.hpp"
#include "game/world12/national_match.hpp"
#include "game/world12/friendly_match.hpp"
#include "game/national_identity.hpp"
#include "game/world12/economy.hpp"
#include "game/cup.hpp"
#include "formats/le.hpp"
#include <set>
#include <algorithm>
#include "game/world12/save.hpp"
#include "game/world12/rescheduling.hpp"
#include "game/world12/progression.hpp"
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
    CareerSnapshot* human_snapshot=nullptr,HumanProjectionPolicy policy={},const HumanRoundHooks* hooks=nullptr,
    const KnockoutStage* cup=nullptr,const LeImage* cup_source=nullptr,CupRoundOutcome* cup_out=nullptr,const EuropeanGroup* european=nullptr,const std::vector<EuropeanGroup>* european_batch=nullptr,const std::vector<size_t>* league_groups=nullptr,const RegionalPlayoffGroup* regional=nullptr,const std::vector<NationalGroup>* national=nullptr,const FriendlyGroup* friendly=nullptr,std::vector<std::optional<uint8_t>>* friendly_kickoffs=nullptr,bool observe_ai=false) {
    const bool national_mode=national||(friendly&&friendly->national);
    if(fixtures.empty() || fixtures.size()>(friendly?30u:national?12u:european?64u:cup?32u:regional?2u:league_groups?40u:10u) || conditions.size()!=fixtures.size())throw std::invalid_argument("world12: incomplete AI league match batch");
    if(national&&(!human_snapshot||national->empty()))throw FormatError("world12: national clock lacks its snapshot/group owner");
    std::vector<ClubId> participants;LeagueId league;
    const auto european_groups=european_batch?*european_batch:european?std::vector<EuropeanGroup>{*european}:std::vector<EuropeanGroup>{};
    std::vector<uint8_t> european_flags;for(const auto& group:european_groups)for(size_t i=0;i<group.matches.size()*2;++i)european_flags.push_back(group.flags);
    if(european&&european_flags.size()!=fixtures.size()*2)throw FormatError("world12: incomplete European shared-clock group descriptors");
    for(size_t m=0;m<fixtures.size();++m) {
        const auto fixture=fixtures[m];const auto& c=conditions[m];
        if(c.replay_source&&(*c.replay_source>=kCupEntries||!base.memory.has(cup_entry_address(*c.replay_source)+0x27)||base.memory.u16(cup_entry_address(*c.replay_source)+0x18)!=9))throw FormatError("world12: invalid replay Source entry");
        if(c.replay_source&&(cup||european||regional||national||friendly))throw FormatError("world12: replay descriptor cannot replace a different competition");
        if(fixture.home==fixture.away || c.month<1 || c.month>12 || c.temperature<-128 || c.temperature>127 || c.rain<0 || c.rain>7 ||
            c.spectators>uint32_t(std::numeric_limits<int>::max()) || c.referee>=c.referee_properties.size() || c.presentation_buttons.size()<0xb4)
            throw std::invalid_argument("world12: incomplete AI league match conditions");
        if(c.month!=conditions[0].month || c.date!=conditions[0].date || c.referee_properties!=conditions[0].referee_properties || c.presentation_buttons!=conditions[0].presentation_buttons || c.teams_prepared!=conditions[0].teams_prepared)
            throw std::invalid_argument("world12: inconsistent shared league-round conditions");
        const auto& home=world.clubs.at(fixture.home);const auto& away=world.clubs.at(fixture.away);
        if((!european&&!national_mode&&(!home.league.valid()||!away.league.valid()))||(home.league.valid()&&!world.catalogue.league(home.league).active)||(away.league.valid()&&!world.catalogue.league(away.league).active)||
           (!cup&&!european&&!regional&&!national&&!friendly&&(home.league!=away.league||(!league_groups&&league.valid()&&league!=home.league)))||
           (cup&&(home.country!=cup_country(cup->cup)||away.country!=cup_country(cup->cup)))||
           (national_mode&&(home.kind!=ClubKind::national_team||away.kind!=ClubKind::national_team))||
           (regional&&(home.country!=Country::germany||away.country!=Country::germany||(home.league.value!=2&&home.league.value!=3)||(away.league.value!=2&&away.league.value!=3))))
            throw std::invalid_argument("world12: match fixture crosses its competition ownership");
        league=home.league;participants.push_back(fixture.home);participants.push_back(fixture.away);
    }
    if(human_snapshot&&(human_snapshot->world!=world||conditions[0].date!=human_snapshot->date))throw std::invalid_argument("world12: human forecast snapshot/date does not match round owner");
    const bool club_human_mode=human_snapshot&&!observe_ai&&((!cup&&!european&&!regional)||std::any_of(world.managers.begin(),world.managers.end(),[&](const auto& m){return std::find(participants.begin(),participants.end(),m.club)!=participants.end();}));
    const auto original_economy=world.economy;
    auto projection=[&] {
        if(observe_ai&&!world.managers.empty()) {
            // Spectators keep their real settings/view ownership without
            // becoming participants in this AI-only group.
            auto clubs=participants;
            for(const auto& manager:world.managers)
                if(std::find(clubs.begin(),clubs.end(),manager.club)==clubs.end())clubs.push_back(manager.club);
            return AiMatchProjection::create_for_humans(*human_snapshot,clubs,base,policy);
        }
        return national_mode?AiMatchProjection::create_for_nationals(*human_snapshot,participants,base,policy):european?AiMatchProjection::create_for_european(*human_snapshot,participants,base,policy):club_human_mode?(league_groups?AiMatchProjection::create_for_human_leagues(*human_snapshot,participants,base,policy):AiMatchProjection::create_for_humans(*human_snapshot,participants,base,policy)):AiMatchProjection::create(world,participants,base);
    }();
    auto& local=projection.state;
    if(observe_ai&&projection.human_managers()) {
        const auto viewer=human_snapshot->desktop&&human_snapshot->desktop->active_seat?*human_snapshot->desktop->active_seat:human_snapshot->progress?human_snapshot->progress->last_manager:projection.global_manager(0);
        for(size_t n=0;n<projection.human_managers();++n)if(projection.global_manager(uint8_t(n))==viewer)local.memory.set8(layout::ActiveManager,uint8_t(n));
    }
    const bool human_mode=national_mode?projection.human_managers()!=0:club_human_mode;
    if(original_economy){if(national_mode)install_career_mailbox(projection,world);else install_career_economy(projection,world);}
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
    if(regional){
        if(!cup_source||!cup_out||!human_snapshot||!human_snapshot->world.regional_playoff||regional_playoff_rules(*cup_source)!=human_snapshot->world.regional_playoff->rules)
            throw FormatError("world12: regional match lacks its actual programme/source owner");
        group_type=14;group_flags=regional->flags;source_entry=cup_entry_address(35);
        local.memory.add_block(source_entry,0x28);for(uint32_t b=0;b<0x28;++b)local.memory.set8(source_entry+b,cup_source->u8(source_entry+b));local.memory.set8(source_entry+0x1d,regional->played);
    }
    if(european){
        if(!cup_source||!cup_out||!human_snapshot||!human_snapshot->world.european_campaigns)throw FormatError("world12: European match owner lacks its saved programme/source");
        std::set<EuropeanCompetition> installed;
        for(const auto& group:european_groups)if(installed.insert(group.competition).second){const auto& campaign=(*human_snapshot->world.european_campaigns)[size_t(uint8_t(group.competition)-1)];
            const auto rules=european_programme_rules(*cup_source,group.competition);if(rules.size()!=campaign.stages.size())throw FormatError("world12: European source programme changed before match");
            for(size_t i=0;i<rules.size();++i){const auto& stage=campaign.stages[i];if(rules[i]!=stage.rules)throw FormatError("world12: European source stage changed before match");const auto at=cup_entry_address(stage.rules.entry);local.memory.add_block(at,0x28);for(uint32_t b=0;b<0x28;++b)local.memory.set8(at+b,cup_source->u8(at+b));local.memory.set8(at+0x1d,stage.played);}}
    }
    if(national){
        if(!cup_source||!cup_out||!human_snapshot||!human_snapshot->world.national_campaigns)throw FormatError("world12: national match owner lacks its saved programme/source");
        const auto& campaign=human_snapshot->world.national_campaigns->seasons.at(human_snapshot->season);const auto rules=national_programme_rules(*cup_source,campaign.season);if(rules.size()!=campaign.stages.size())throw FormatError("world12: national source programme changed before match");
        for(size_t i=0;i<rules.size();++i){const auto& stage=campaign.stages[i];if(rules[i]!=stage.rules)throw FormatError("world12: national source stage changed before match");const auto at=cup_entry_address(stage.rules.entry);local.memory.add_block(at,0x28);for(uint32_t b=0;b<0x28;++b)local.memory.set8(at+b,cup_source->u8(at+b));local.memory.set8(at+0x1d,stage.played);}
    }
    if(friendly){if(!human_snapshot||!cup_source||!cup_out||friendly->matches!=fixtures||friendly->flags!=friendly_source_flags(*cup_source))throw FormatError("world12: friendly match lacks its actual dated Source owner");group_type=11;group_flags=friendly->flags;source_entry=cup_entry_address(24);local.memory.add_block(source_entry,0x28);for(uint32_t byte=0;byte<0x28;++byte)local.memory.set8(source_entry+byte,cup_source->u8(source_entry+byte));local.memory.set8(source_entry+0x1a,uint8_t(fixtures.size()*2));local.memory.set8(source_entry+0x1d,0);}
    for(size_t participant=0;participant<participants.size();++participant) {
        const auto team=projection.local_club(participants[participant]);
        if(!conditions[0].teams_prepared){uint8_t type=group_type;if(european){size_t position=0;for(const auto& group:european_groups){position+=group.matches.size()*2;if(participant<position){type=uint8_t(group.competition)+1;break;}}}if(national){size_t position=0;for(const auto& group:*national){position+=group.matches.size()*2;if(participant<position){type=group.competition+1;break;}}}bmh::prepare_team(local,int(team),match_competition_class(type),int(participant));}
        const auto formation=size_t(local.formation(int(team)))*22;
        if(formation+22>local.formations.size())throw std::invalid_argument("world12: missing Original formation table");
        for(size_t row=0;row<11;++row)if(local.formations[formation+row]>=24 || local.formations[formation+11+row]>8)
            throw std::invalid_argument("world12: invalid Original formation coordinates");
        bmh::auto_lineup(local,int(team),local.manager_of(int(team))>=0?0xe4:100);
        if(local.manager_of(int(team))>=0){bmh::prepare_lineup_entry(local,int(team));const auto manager=local.manager_of(int(team));
            for(size_t row=0;row<24;++row){const auto slot=local.squad_slots[squad_range(local,int(team)).first+row];if((slot&0xfff)==0xfff)break;local.tactics[size_t(manager)*0x840+row*0x58+0x10]=uint8_t(slot&0x8000?0:slot&0x4000?0x7f:0x7e);}}

    }
    bmh::reset_match_played_flags(local);
    auto staged_game=game;auto staged_random=random;
    const auto initial_game_seed=game.seed();const auto initial_watcom_seed=random.seed();
    const auto initial_game_calls=game.calls(),initial_watcom_calls=random.calls();
    std::vector<bmh::MatchGroup> groups;
    size_t flat=0;const size_t group_count=national?national->size():european?european_groups.size():league_groups?league_groups->size():1;
    for(size_t g=0;g<group_count;++g){
        const auto type=conditions[flat].replay_source?10:national?national->at(g).competition+1:european?uint8_t(european_groups[g].competition)+1:group_type;
        const auto flags=conditions[flat].replay_source?local.memory.u8(cup_entry_address(*conditions[flat].replay_source)+0x14):national?national->at(g).flags:european?european_groups[g].flags:group_flags;
        const auto count=national?national->at(g).matches.size():european?european_groups[g].matches.size():league_groups?league_groups->at(g):fixtures.size();
        const auto competition=conditions[flat].replay_source?cup_entry_address(*conditions[flat].replay_source):national?cup_entry_address(national->at(g).source_entry):european?cup_entry_address(european_groups[g].source_entry):(cup||regional||friendly)?source_entry:uint32_t(bmh::match_team_league(local,int(projection.local_club(fixtures[flat].home))));
        if(conditions[flat].replay_source){if(*conditions[flat].replay_source>=kCupEntries||!local.memory.has(competition+0x27)||local.memory.u16(competition+0x18)!=9)throw FormatError("world12: replay lacks verified Source competition metadata");for(size_t m=0;m<count;++m)if(conditions[flat+m].replay_source!=conditions[flat].replay_source)throw FormatError("world12: replay group changed source programme");}
        bmh::MatchGroup group;group.desc.assign(0xd0,0);group.desc[0]=uint8_t(type);group.desc[1]=flags;group.desc[2]=uint8_t(count);
        for(size_t b=0;b<4;++b)group.desc[0xcc+b]=uint8_t(competition>>(8*b));
        if(cup&&cup->two_legs()&&cup->next_leg()==2)group.cup_other_leg=cup->rounds.back().first_leg;
        if(european)group.cup_other_leg=european_groups[g].other_leg;
        if(national)group.cup_other_leg=national->at(g).other_leg;
        for(size_t m=0;m<count;++m,++flat){const auto& c=conditions[flat];group.desc[0x10+m]=c.referee;group.desc[0x30+m]=uint8_t(c.temperature);group.desc[0x50+m]=uint8_t(c.rain);
            group.fixtures.push_back(int(projection.local_club(fixtures[flat].home)));group.fixtures.push_back(int(projection.local_club(fixtures[flat].away)));if(type==10||cup||european||regional||national||friendly)group.cup_results.push_back(0);else group.results.push_back(0);group.end_minute.push_back(0);group.spectators.push_back(c.spectators);
            bmh::MatchSetup setup;setup.home=group.fixtures[m*2];setup.away=group.fixtures[m*2+1];setup.match_index=int(m);setup.climate_effect=(flags&2)!=0;setup.month=c.month;setup.temperature=c.temperature;setup.spectators=int(c.spectators);
            bmh::select_live_match_source(local,setup.home,setup.away);
            if(c.kickoff_disposition&&*c.kickoff_disposition>2)throw FormatError("world12: invalid kickoff receipt");
            const auto forfeit=c.kickoff_disposition?int(*c.kickoff_disposition)-1:bmh::prepare_kickoff_fixture(local,setup.home,setup.away,staged_random);
            if(friendly_kickoffs)friendly_kickoffs->push_back(uint8_t(forfeit+1));
            if(forfeit>=0){group.records.insert(group.records.end(),18,0);bmh::award_kickoff_forfeit(local,group,int(m),forfeit,false);group.spectators[m]=0;continue;}
            const auto sides=bmh::build_side_records(local,setup,staged_random,&staged_game);for(const auto& side:sides)group.records.insert(group.records.end(),side.b.begin(),side.b.end());}
        groups.push_back(std::move(group));
    }
    bmh::MatchEngine engine(local,groups,staged_game,staged_random,conditions[0].referee_properties);
    engine.month=conditions[0].month;engine.buttons=conditions[0].presentation_buttons;engine.leave_screens=(!human_mode&&!observe_ai)||(hooks&&hooks->decline_unpresented_events);
    if(hooks&&hooks->live_event)engine.live_event=[&](const bmh::LiveMatchEvent& event){hooks->live_event(projection,engine,event);};
    std::vector<size_t> pool_rows;for(size_t m=0;m<conditions.size();++m)if(conditions[m].pools_selected)pool_rows.push_back(m);
    if(pool_rows.size()>11)throw FormatError("world12: pools receipt exceeds eleven fixtures");
    if(!pool_rows.empty()) {
        local.memory.add_block(layout::DeskActions_SubmitPoolsCoupon_RuntimeField,44);local.memory.add_block(layout::MatchdaySetup_EvaluatePoolsCoupon_RuntimeField,11);local.memory.add_block(layout::CareerSession_DispatchToto_RuntimeField,1);
        for(size_t n=0;n<pool_rows.size();++n){const auto m=pool_rows[n];local.memory.set16(layout::DeskActions_SubmitPoolsCoupon_RuntimeField+uint32_t(n*4),uint16_t(projection.local_club(fixtures[m].home)));local.memory.set16(layout::DeskActions_SubmitPoolsCoupon_RuntimeField+uint32_t(n*4+2),uint16_t(projection.local_club(fixtures[m].away)));}
        local.pools_open=true;
    }
    if(hooks&&hooks->before_matchday)hooks->before_matchday(projection,engine);
    if(human_snapshot&&hooks&&hooks->before_owned)hooks->before_owned(projection,engine,*human_snapshot);
    bmh::MinuteReport report;
    bmh::play_matchday(engine,local,groups,conditions[0].presentation_buttons,staged_game,staged_random,report,
        hooks&&hooks->after_minute?std::function<void(int)>([&](int minute){hooks->after_minute(projection,engine,minute);}):std::function<void(int)>{});
    if(report.unsupported)throw std::runtime_error("world12: Original AI match could not complete: "+report.reason);
    for(auto& finished:groups){
        for(size_t m=0;m<finished.end_minute.size();++m)if(!(finished.kickoff_void_mask&(1u<<m))&&(finished.end_minute[m]<90||finished.end_minute[m]>((cup||european||national)?124:94)))throw std::runtime_error("world12: Original match did not reach full time");
        if(settle){for(auto score:finished.results)if((score&0xcc)==0xcc)throw std::invalid_argument("world12: result collides with the legacy post-match sentinel");bmh::process_group(local,finished,staged_game,staged_random,report);if(report.unsupported)throw std::runtime_error("world12: Original post-match settlement could not complete: "+report.reason);}
    }
    if(game.seed()!=initial_game_seed || game.calls()!=initial_game_calls || random.seed()!=initial_watcom_seed || random.calls()!=initial_watcom_calls)
        throw std::invalid_argument("world12: random stream ownership changed during the match");
    AiLeagueRoundOutcome outcome;outcome.game_draws=staged_game.calls()-initial_game_calls;outcome.watcom_draws=staged_random.calls()-initial_watcom_calls;
    if(!pool_rows.empty()) {
        if(local.pools_open||local.memory.u8(layout::CareerSession_DispatchToto_RuntimeField)!=pool_rows.size())throw FormatError("world12: Core did not evaluate the selected pools fixtures");
        for(size_t n=0;n<pool_rows.size();++n){const auto value=local.memory.u8(layout::MatchdaySetup_EvaluatePoolsCoupon_RuntimeField+uint32_t(n));if(value>2)throw FormatError("world12: invalid Core pools outcome");outcome.pools.push_back({fixtures[pool_rows[n]],value});}
    }
    if(cup||european||regional||national||friendly)cup_out->pools=outcome.pools;
    if(cup||european||regional||national||friendly){for(const auto& finished:groups){cup_out->scores.insert(cup_out->scores.end(),finished.cup_results.begin(),finished.cup_results.end());cup_out->end_minutes.insert(cup_out->end_minutes.end(),finished.end_minute.begin(),finished.end_minute.end());}cup_out->game_draws=outcome.game_draws;cup_out->watcom_draws=outcome.watcom_draws;}
    else {size_t index=0;for(const auto& finished:groups){const bool replay=finished.desc[0]==10;for(size_t m=0;m<size_t(finished.matches());++m,++index){const auto value=replay?finished.cup_results.at(m):finished.results.at(m);const auto home=uint8_t(replay?(value>>5)&31:value>>4),away=uint8_t(replay?value&31:value&15);if(home>15||away>15)throw FormatError("world12: replay score exceeds retained league result width");outcome.matches.push_back({fixtures[index],{true,home,away},finished.end_minute[m],0,0,settle});}}}
    if(world.economy!=original_economy)throw FormatError("world12: career economy changed during match calculation");
    auto published=world;projection.commit_match_state(published);if(original_economy){if(national_mode)capture_career_mailbox(published,projection);else capture_career_economy(published,projection);}published.validate();
    if(human_snapshot)projection.commit_desktop_preferences(*human_snapshot);world=std::move(published);game=staged_game;random=staged_random;return outcome;
}
}
AiLeagueOutcome simulate_ai_league_match(World& world,const GameState& base,Fixture fixture,const AiLeagueConditions& conditions,GameRandom& game,WatcomRandom& random) {
    auto result=run_ai_league_matches(world,base,{fixture},{conditions},game,random,false);auto outcome=result.matches.front();outcome.game_draws=result.game_draws;outcome.watcom_draws=result.watcom_draws;return outcome;
}
AiLeagueOutcome simulate_and_settle_ai_league_match(World& world,const GameState& base,Fixture fixture,const AiLeagueConditions& conditions,GameRandom& game,WatcomRandom& random) {
    auto result=run_ai_league_matches(world,base,{fixture},{conditions},game,random,true);auto outcome=result.matches.front();outcome.game_draws=result.game_draws;outcome.watcom_draws=result.watcom_draws;return outcome;
}
AiLeagueRoundOutcome simulate_and_settle_ai_league_fixtures(World& world,const GameState& base,LeagueId league,int round,const std::vector<uint8_t>& indices,const std::vector<AiLeagueConditions>& conditions,GameRandom& game,WatcomRandom& random){return run_ai_league_matches(world,base,selected_league_fixtures(world,league,round,indices),conditions,game,random,true);}
AiLeagueRoundOutcome simulate_observed_ai_league_fixtures(CareerSnapshot& state,const GameState& base,LeagueId league,int round,const std::vector<uint8_t>& indices,const std::vector<AiLeagueConditions>& conditions,HumanProjectionPolicy policy,GameRandom& game,WatcomRandom& random,const HumanRoundHooks& hooks){
    const auto fixtures=selected_league_fixtures(state.world,league,round,indices);for(const auto& f:fixtures)for(const auto& m:state.world.managers)if(m.club==f.home||m.club==f.away)throw FormatError("world12: observed AI group contains an actual human");
    const auto before=state;const auto gs=game.seed();const auto ws=random.seed();const auto gc=game.calls(),wc=random.calls();auto candidate=state;candidate.game_seed=gs;candidate.watcom_seed=ws;auto gr=game;auto wr=random;const auto outcome=run_ai_league_matches(candidate.world,base,fixtures,conditions,gr,wr,true,&candidate,policy,&hooks,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,true);candidate.game_seed=gr.seed();candidate.watcom_seed=wr.seed();candidate.validate();if(state!=before||game.seed()!=gs||random.seed()!=ws||game.calls()!=gc||random.calls()!=wc)throw FormatError("world12: observed AI owner changed");state=std::move(candidate);game=gr;random=wr;return outcome;
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

void validate_league_conditions(const std::vector<AiLeagueConditions>& conditions) {
    if(conditions.empty())throw FormatError("world12: empty league conditions");
    for(const auto& c:conditions)if(c.month<1||c.month>12||c.temperature<-128||c.temperature>127||c.rain<0||c.rain>7||
        c.spectators>uint32_t(std::numeric_limits<int>::max())||c.referee>=c.referee_properties.size()||c.presentation_buttons.size()<180||
        c.month!=conditions[0].month||c.date!=conditions[0].date||c.referee_properties!=conditions[0].referee_properties||
        c.presentation_buttons!=conditions[0].presentation_buttons||c.teams_prepared!=conditions[0].teams_prepared)
        throw FormatError("world12: incomplete/inconsistent league conditions");
}
HumanLeagueBatchOutcome simulate_and_settle_human_league_rounds(CareerSnapshot& state,const GameState& base,
    const std::vector<HumanLeagueRound>& rounds,HumanProjectionPolicy policy,GameRandom& game,WatcomRandom& random,const HumanRoundHooks& hooks) {
    state.validate();if(rounds.empty()||rounds.size()>4)throw FormatError("world12: human league clock needs 1-4 leagues");
    std::set<std::pair<LeagueId,int>> leagues;std::vector<Fixture> fixtures;std::vector<AiLeagueConditions> conditions;std::vector<size_t> counts;
    for(const auto& round:rounds){
        if(!leagues.emplace(round.league,round.round).second)throw FormatError("world12: repeated league/round clock group");
        if(round.match_indices.empty()&&round.ordinary){if(round.round!=state.league_rounds.at(round.league.value)+1)throw FormatError("world12: stale complete league clock");}
        else {const auto ticket=prepare_day(state);if(!ticket||std::none_of(ticket->leagues.begin(),ticket->leagues.end(),[&](const auto& p){return p.appointment.league==round.league&&p.appointment.round==round.round&&p.match_indices==round.match_indices&&p.ordinary==round.ordinary;}))throw FormatError("world12: selected league clock has no actual date ticket");}
        const auto matches=selected_league_fixtures(state.world,round.league,round.round,round.match_indices);
        if(matches.size()!=round.matches.size()||!std::any_of(state.world.managers.begin(),state.world.managers.end(),[&](const auto& manager){return std::any_of(matches.begin(),matches.end(),[&](const auto& f){return f.home==manager.club||f.away==manager.club;});}))throw FormatError("world12: incomplete or unowned human fixture group");
        for(size_t m=0;m<matches.size();++m)if(state.results.result(state.world.catalogue,round.league,round.round,round.match_indices.empty()?int(m):round.match_indices[m]).played||round.matches[m].replay_source.has_value()==round.ordinary)throw FormatError("world12: repeated fixture or changed replay type");
        validate_league_conditions(round.matches);counts.push_back(matches.size());fixtures.insert(fixtures.end(),matches.begin(),matches.end());conditions.insert(conditions.end(),round.matches.begin(),round.matches.end());
    }
    validate_league_conditions(conditions);
    const auto before=state;auto candidate=state;auto staged_game=game;auto staged_random=random;
    const auto game_seed=game.seed();const auto game_calls=game.calls();const auto random_seed=random.seed();const auto random_calls=random.calls();
    auto flat=run_ai_league_matches(candidate.world,base,fixtures,conditions,staged_game,staged_random,true,&candidate,policy,&hooks,nullptr,nullptr,nullptr,nullptr,nullptr,&counts);
    HumanLeagueBatchOutcome outcome;outcome.game_draws=flat.game_draws;outcome.watcom_draws=flat.watcom_draws;size_t offset=0;
    for(auto count:counts){AiLeagueRoundOutcome result;result.matches.insert(result.matches.end(),flat.matches.begin()+offset,flat.matches.begin()+offset+count);for(const auto& pool:flat.pools)if(std::any_of(result.matches.begin(),result.matches.end(),[&](const auto& match){return match.fixture==pool.fixture;}))result.pools.push_back(pool);offset+=count;outcome.leagues.push_back(std::move(result));}
    candidate.game_seed=staged_game.seed();candidate.watcom_seed=staged_random.seed();candidate.validate();
    if(state!=before||game.seed()!=game_seed||game.calls()!=game_calls||random.seed()!=random_seed||random.calls()!=random_calls)
        throw FormatError("world12: human league batch owner changed during play");
    state=std::move(candidate);game=staged_game;random=staged_random;return outcome;
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

EuropeanMatchBatchOutcome simulate_and_settle_european_groups(CareerSnapshot& state,const GameState& base,const std::vector<EuropeanGroup>& groups,
    const LeImage& source,const std::vector<std::vector<AiLeagueConditions>>& conditions,HumanProjectionPolicy policy,
    GameRandom& game,WatcomRandom& random,const HumanRoundHooks& hooks){
    state.validate();if(!state.world.european_campaigns||groups.empty()||groups.size()!=conditions.size())throw FormatError("world12: missing European match programme/conditions");
    std::vector<Fixture> fixtures;std::vector<AiLeagueConditions> flat_conditions;std::set<ClubId> participants;
    for(size_t i=0;i<groups.size();++i){const auto& group=groups[i];const auto id=uint8_t(group.competition);if(id<1||id>3)throw FormatError("world12: invalid European match competition");const auto owned=prepare_european_groups((*state.world.european_campaigns)[size_t(id-1)]);if(std::find(owned.begin(),owned.end(),group)==owned.end()||conditions[i].size()!=group.matches.size())throw FormatError("world12: stale/incomplete European match group");
        for(const auto& c:conditions[i])if(c.date!=state.date)throw FormatError("world12: European conditions use another date");for(const auto& f:group.matches)for(auto id:{f.home,f.away})if(!participants.insert(id).second)throw FormatError("world12: European match groups repeat a club");fixtures.insert(fixtures.end(),group.matches.begin(),group.matches.end());flat_conditions.insert(flat_conditions.end(),conditions[i].begin(),conditions[i].end());}
    const auto before=state;auto candidate=state;auto staged_game=game;auto staged_random=random;const auto game_seed=game.seed();const auto random_seed=random.seed();const auto game_calls=game.calls(),random_calls=random.calls();
    CupRoundOutcome receipt;run_ai_league_matches(candidate.world,base,fixtures,flat_conditions,staged_game,staged_random,true,&candidate,policy,&hooks,nullptr,&source,&receipt,&groups.front(),&groups);
    EuropeanMatchBatchOutcome result;result.game_draws=receipt.game_draws;result.watcom_draws=receipt.watcom_draws;size_t at=0;
    for(const auto& group:groups){CupRoundOutcome out;const auto count=group.matches.size();out.scores.assign(receipt.scores.begin()+ptrdiff_t(at),receipt.scores.begin()+ptrdiff_t(at+count));out.end_minutes.assign(receipt.end_minutes.begin()+ptrdiff_t(at),receipt.end_minutes.begin()+ptrdiff_t(at+count));for(const auto& pool:receipt.pools)if(std::find(group.matches.begin(),group.matches.end(),pool.fixture)!=group.matches.end())out.pools.push_back(pool);result.groups.push_back(std::move(out));at+=count;}
    candidate.game_seed=staged_game.seed();candidate.watcom_seed=staged_random.seed();candidate.validate();
    if(state!=before||game.seed()!=game_seed||random.seed()!=random_seed||game.calls()!=game_calls||random.calls()!=random_calls)throw FormatError("world12: European match snapshot/random owner changed");
    state=std::move(candidate);game=staged_game;random=staged_random;return result;
}
CupRoundOutcome simulate_and_settle_european_group(CareerSnapshot& state,const GameState& base,const EuropeanGroup& group,
    const LeImage& source,const std::vector<AiLeagueConditions>& conditions,HumanProjectionPolicy policy,
    GameRandom& game,WatcomRandom& random,const HumanRoundHooks& hooks){auto result=simulate_and_settle_european_groups(state,base,{group},source,{conditions},policy,game,random,hooks);auto out=std::move(result.groups.front());out.game_draws=result.game_draws;out.watcom_draws=result.watcom_draws;return out;}
}

namespace bmh::world12 {
CupRoundOutcome simulate_and_settle_regional_playoff_group(CareerSnapshot& state,const GameState& base,
    const RegionalPlayoffGroup& group,const LeImage& source,const std::vector<AiLeagueConditions>& conditions,
    HumanProjectionPolicy policy,GameRandom& game,WatcomRandom& random,const HumanRoundHooks& hooks) {
    state.validate();const auto expected=prepare_regional_playoff_group(state);
    if(!expected||*expected!=group)throw FormatError("world12: stale regional match/date owner");
    const auto before=state;auto candidate=state;auto staged_game=game;auto staged_random=random;
    const auto game_seed=game.seed();const auto game_calls=game.calls();const auto random_seed=random.seed();const auto random_calls=random.calls();
    CupRoundOutcome outcome;run_ai_league_matches(candidate.world,base,group.matches,conditions,staged_game,staged_random,true,&candidate,policy,&hooks,nullptr,&source,&outcome,nullptr,nullptr,nullptr,&group);
    candidate.game_seed=staged_game.seed();candidate.watcom_seed=staged_random.seed();candidate.validate();
    if(state!=before||game.seed()!=game_seed||game.calls()!=game_calls||random.seed()!=random_seed||random.calls()!=random_calls)
        throw FormatError("world12: regional match snapshot/RNG owner changed");
    state=std::move(candidate);game=staged_game;random=staged_random;return outcome;
}
}

namespace bmh::world12 {
NationalMatchBatchOutcome simulate_and_settle_national_groups(CareerSnapshot& state,const GameState& base,const std::vector<NationalGroup>& groups,
    const LeImage& source,const std::vector<std::vector<AiLeagueConditions>>& conditions,HumanProjectionPolicy policy,
    GameRandom& game,WatcomRandom& random,const HumanRoundHooks& hooks){
    state.validate();if(!state.world.national_campaigns||!state.world.national_campaigns->seasons.contains(state.season)||groups.empty()||groups.size()!=conditions.size()||prepare_national_groups(state.world.national_campaigns->seasons.at(state.season))!=groups)throw FormatError("world12: missing/stale/incomplete national parallel programme");
    std::vector<Fixture> fixtures;std::vector<AiLeagueConditions> flat;std::set<ClubId> participants;
    for(size_t i=0;i<groups.size();++i){const auto& group=groups[i];if(group.matches.size()!=conditions[i].size())throw FormatError("world12: incomplete national conditions");for(const auto& c:conditions[i])if(c.date!=state.date)throw FormatError("world12: national conditions use another date");for(const auto& match:group.matches)for(auto id:{match.home,match.away})if(!participants.insert(id).second)throw FormatError("world12: parallel national groups repeat a nation");fixtures.insert(fixtures.end(),group.matches.begin(),group.matches.end());flat.insert(flat.end(),conditions[i].begin(),conditions[i].end());}
    const auto before=state;auto candidate=state;auto staged_game=game;auto staged_random=random;const auto gs=game.seed();const auto ws=random.seed();const auto gc=game.calls(),wc=random.calls();CupRoundOutcome receipt;
    run_ai_league_matches(candidate.world,base,fixtures,flat,staged_game,staged_random,true,&candidate,policy,&hooks,nullptr,&source,&receipt,nullptr,nullptr,nullptr,nullptr,&groups);
    NationalMatchBatchOutcome out;out.game_draws=receipt.game_draws;out.watcom_draws=receipt.watcom_draws;size_t first=0;for(const auto& group:groups){CupRoundOutcome result;const auto count=group.matches.size();result.scores.assign(receipt.scores.begin()+ptrdiff_t(first),receipt.scores.begin()+ptrdiff_t(first+count));result.end_minutes.assign(receipt.end_minutes.begin()+ptrdiff_t(first),receipt.end_minutes.begin()+ptrdiff_t(first+count));for(const auto& pool:receipt.pools)if(std::find(group.matches.begin(),group.matches.end(),pool.fixture)!=group.matches.end())result.pools.push_back(pool);out.groups.push_back(std::move(result));first+=count;}
    candidate.game_seed=staged_game.seed();candidate.watcom_seed=staged_random.seed();candidate.validate();if(state!=before||game.seed()!=gs||game.calls()!=gc||random.seed()!=ws||random.calls()!=wc)throw FormatError("world12: national match snapshot/RNG owner changed");state=std::move(candidate);game=staged_game;random=staged_random;return out;
}
FriendlyRoundOutcome simulate_and_settle_friendly_group(CareerSnapshot& state,const GameState& reference,const FriendlyGroup& group,const LeImage& source,const std::vector<AiLeagueConditions>& conditions,HumanProjectionPolicy policy,GameRandom& game,WatcomRandom& random,const HumanRoundHooks& hooks){
    state.validate();const auto groups=prepare_friendly_groups(state);if(std::find(groups.begin(),groups.end(),group)==groups.end()||group.day!=state.calendar->next_day||group.matches.size()!=conditions.size())throw FormatError("world12: stale/incomplete friendly day group");const auto before=state;auto candidate=state;auto staged_game=game;auto staged_random=random;FriendlyRoundOutcome receipt;run_ai_league_matches(candidate.world,reference,group.matches,conditions,staged_game,staged_random,true,&candidate,policy,&hooks,nullptr,&source,&receipt,nullptr,nullptr,nullptr,nullptr,nullptr,&group,&receipt.kickoff_dispositions);candidate.game_seed=staged_game.seed();candidate.watcom_seed=staged_random.seed();candidate.validate();if(state!=before)throw FormatError("world12: friendly day changed during actual clock");state=std::move(candidate);game=staged_game;random=staged_random;return receipt;
}

}
