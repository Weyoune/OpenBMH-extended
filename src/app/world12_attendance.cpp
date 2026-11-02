// SPDX-License-Identifier: GPL-3.0-only
#include "app/world12_attendance.hpp"
#include "game/world12/economy.hpp"
#include "game/matchday_setup.hpp"
#include "game/match.hpp"
#include "game/lineup.hpp"
#include "game/cup.hpp"
#include "game/week.hpp"
#include <algorithm>
#include <limits>
namespace bmh::app::world12 {
using namespace bmh::world12;
AttendanceOutcome book_event_attendance(CareerSnapshot& state,ManagerActionOwner& owner,
    ManagerActionOwner expected,const GameState& reference,const TextDb& texts,const CupSources& sources) {
    state.validate();
    if(!owner.scope||owner.scope!=expected.scope||owner.revision!=expected.revision||
       owner.revision==std::numeric_limits<uint64_t>::max()||!state.prepared_event||state.prepared_event->gates_booked||
       !state.world.settings||!state.world.economy||!state.progress||
       state.prepared_event->desk_phase==PreparedEvent::DeskPhase::desks)
        throw FormatError("world12: missing or stale gate-income owner");
    for(const auto& manager:state.world.managers)
        if(!manager.career||!manager.career->season_services||!manager.match_state||!manager.match_state->attendance)
            throw FormatError("world12: gate history is unknown; a current fresh career is required");
    const auto before=state;auto candidate=state;const auto settings=*state.world.settings;
    for(const auto& [id,club]:candidate.world.clubs)for(const auto& row:club.roster)
        if(!row.empty())candidate.world.players.at(row.player).attributes.w364&=~0x1000u;
    GameRandom game(candidate.game_seed);WatcomRandom random(candidate.watcom_seed);AttendanceOutcome out;
    for(const auto& group:candidate.prepared_event->groups)for(const auto& match:group.matches) {
        std::vector<ClubId> clubs;for(const auto& manager:candidate.world.managers)clubs.push_back(manager.club);
        for(auto club:{match.fixture.home,match.fixture.away})if(std::find(clubs.begin(),clubs.end(),club)==clubs.end())clubs.push_back(club);
        auto p=group.european?AiMatchProjection::create_for_european(candidate,clubs,reference,
            {settings.mode,settings.difficulty,settings.foreign_rule,true}):AiMatchProjection::create_for_humans(candidate,clubs,reference,
            {settings.mode,settings.difficulty,settings.foreign_rule,true});
        install_career_economy(p,candidate.world);auto& local=p.state;local.texts=&texts;
        local.season=int(candidate.season);local.season_start=candidate.progress->season_start;
        const auto home=int(p.local_club(match.fixture.home)),away=int(p.local_club(match.fixture.away));
        MatchGroup descriptor;descriptor.desc.assign(0xd0,0);descriptor.desc[1]=group.flags;descriptor.desc[2]=1;
        MatchdayContext context;context.month=month_code(candidate.date);context.weekday=weekday_code(candidate.date);
        context.season_day=int(candidate.date-candidate.progress->season_start);
        uint32_t competition=uint32_t(match_team_league(local,home));uint8_t type=0;
        if(group.league){context.projected_matchdays=candidate.world.catalogue.league(*group.league).rounds();
            context.projected_completed_rounds=candidate.league_rounds[group.league->value];}
        else if(group.european){
            competition=cup_entry_address(group.source_entry);type=uint8_t(*group.european)+1;const auto& campaign=(*candidate.world.european_campaigns)[size_t(uint8_t(*group.european)-1)];
            for(const auto& stage:campaign.stages){const auto at=cup_entry_address(stage.rules.entry);local.memory.add_block(at,0x28);for(uint32_t b=0;b<0x28;++b)local.memory.set8(at+b,sources.german.u8(at+b));local.memory.set8(at+0x1d,stage.played);}
        }
        else {
            const auto& source=cup_country(*group.cup)==Country::germany?sources.german:sources.english;
            competition=cup_entry_address(group.source_entry);type=uint8_t(source.u16(competition+0x18)+1);
            local.memory.add_block(competition,0x28);
            for(uint32_t b=0;b<0x28;++b)local.memory.set8(competition+b,source.u8(competition+b));
            const auto stage=std::find_if(candidate.cups.rbegin(),candidate.cups.rend(),[&](const auto& cup){return cup.cup==*group.cup;});
            const auto played=(stage->rounds.size()-1)*(stage->two_legs()?2u:1u)+(group.leg==2?1u:0u);
            local.memory.set8(competition+0x1d,uint8_t(played));
        }
        descriptor.desc[0]=type;for(size_t b=0;b<4;++b)descriptor.desc[0xcc+b]=uint8_t(competition>>(8*b));
        if(candidate.prepared_event->desk_phase==PreparedEvent::DeskPhase::unstarted) {
            prepare_team(local,home,match_competition_class(type),0);prepare_team(local,away,match_competition_class(type),1);
            auto_lineup(local,home,local.manager_of(home)>=0?0xe4:100);auto_lineup(local,away,local.manager_of(away)>=0?0xe4:100);
        }
        MinuteReport report;const auto bits=match_mode_bits(local,descriptor,context,report);
        if(report.unsupported)throw FormatError(report.reason);
        const auto amount=attendance(local,home,away,bits,match.temperature,match.rain,type,game,random,report);
        if(report.unsupported||amount<0)throw FormatError(report.reason.empty()?"world12: invalid calculated attendance":report.reason);
        p.commit_match_state(candidate.world,true);capture_career_economy(candidate.world,p);
        out.spectators.push_back(uint32_t(amount));
    }
    size_t index=0;for(auto& group:candidate.prepared_event->groups)for(auto& match:group.matches)match.spectators=out.spectators.at(index++);
    candidate.prepared_event->gates_booked=true;candidate.game_seed=game.seed();candidate.watcom_seed=random.seed();candidate.validate();
    if(state!=before||owner.scope!=expected.scope||owner.revision!=expected.revision)
        throw FormatError("world12: gate-income state changed during calculation");
    out.game_draws=game.calls();out.watcom_draws=random.calls();state=std::move(candidate);++owner.revision;return out;
}
}
