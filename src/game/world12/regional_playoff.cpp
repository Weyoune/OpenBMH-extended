// SPDX-License-Identifier: GPL-3.0-only
#include "game/world12/regional_playoff.hpp"
#include "game/world12/save.hpp"
#include "game/world12/standings.hpp"
#include "game/cup.hpp"
#include "game/driver.hpp"
#include "game/schedule.hpp"
#include "formats/le.hpp"
#include <algorithm>
#include <limits>
#include <set>
namespace bmh::world12 {
namespace {
void require(bool value,const char* message){if(!value)throw FormatError(std::string("world12: regional playoff ")+message);}
std::array<uint16_t,6> dates(const SeasonCalendar& calendar){
    std::vector<uint16_t> values;for(const auto& appointment:calendar.others)if(regional_playoff_appointment(appointment))values.push_back(appointment.day);
    require(values.size()==6,"requires six actual German calendar dates");std::array<uint16_t,6> result;
    for(size_t i=0;i<6;++i){require(!i||values[i]>values[i-1],"dates are repeated or out of order");result[i]=values[i];}
    return result;
}
std::map<ClubId,uint8_t> deductions(const World& world){
    std::map<ClubId,uint8_t> result;for(const auto& manager:world.managers)if(manager.match_state){const auto league=world.clubs.at(manager.club).league;const auto value=manager.match_state->point_deduction;if((league.value==2||league.value==3)&&(value>>5)==3)result[manager.club]=value&31;}return result;
}
std::array<ClubId,4> entrants(const CareerSnapshot& state,const auto& cuts){
    std::array<ClubId,4> result;size_t count=0;
    for(auto league:{LeagueId{2},LeagueId{3}}){
        const auto& definition=state.world.catalogue.league(league);
        require(definition.active&&state.league_rounds[league.value]==definition.rounds(),"activation needs completed regional rounds");
        for(const auto& score:state.results.leagues[league.value])require(score.played,"activation needs every regional result");
        TableQuery query;for(const auto& [club,value]:cuts)if(state.world.clubs.at(club).league==league)query.point_deductions[club]=value;
        const auto table=compute_standings(state.world,state.results,league,definition.rounds(),query);
        for(auto club:state.world.membership[league.value].clubs){const auto row=std::find_if(table.begin(),table.end(),[&](const auto& row){return row.club==club;});require(row!=table.end(),"activation table lost a physical club");if(row->rank==2||row->rank==3){require(count<4,"activation selected too many clubs");result[count++]=club;}}
    }
    require(count==4,"activation did not select four actual clubs");return result;
}
struct Workspace {
    GameState state;
    explicit Workspace(const RegionalPlayoffSeason& campaign){
        auto& memory=state.memory;memory.add_block(kCupTable,kCupEntries*kCupEntrySize);
        for(int entry=0;entry<kCupEntries;++entry)memory.set16(cup_entry_address(entry)+0x18,65535);
        const auto entry=cup_entry_address(35);memory.set_pointer(entry,layout::Teams);memory.add_block(layout::Teams,8);
        memory.set_pointer(entry+8,layout::CareerSession_RuntimeError_RuntimeField);
        memory.set_pointer(entry+12,layout::MatchdayGroupWorkspace);memory.add_block(layout::MatchdayGroupWorkspace,32);
        memory.set8(entry+0x14,campaign.rules.flags);memory.set8(entry+0x15,255);memory.set16(entry+0x18,13);
        memory.set8(entry+0x1a,4);memory.set8(entry+0x1b,6);memory.set8(entry+0x1d,campaign.played);memory.set8(entry+0x1e,1);memory.set8(entry+0x1f,2);
        for(uint32_t i=0;i<4;++i)memory.set16(layout::Teams+i*2,uint16_t(i));
        for(uint32_t i=0;i<16;++i)memory.set16(layout::MatchdayGroupWorkspace+i*2,i<12?campaign.scores[i]:0xcccc);
        memory.set16(entry+0x22,65535);memory.set16(entry+0x24,65535);
    }
    std::array<ClubId,2> qualifiers(const RegionalPlayoffSeason& campaign)const{
        const auto entry=cup_entry_address(35);const auto runner=state.memory.u16(entry+0x24),winner=state.memory.u16(entry+0x22);
        require(runner<4&&winner<4&&runner!=winner,"Source returned invalid qualification fields");return {campaign.entrants[runner],campaign.entrants[winner]};
    }
};
std::array<ClubId,2> ranked_qualifiers(const RegionalPlayoffSeason& campaign){
    Workspace workspace(campaign);const auto table=bmh::cup_group_table(workspace.state.memory,cup_entry_address(35));std::array<ClubId,2> result;
    for(int i=0;i<4;++i){if(table.rank(i)==1)result[0]=campaign.entrants[i];if(table.rank(i)==0)result[1]=campaign.entrants[i];}
    require(result[0].valid()&&result[1].valid()&&result[0]!=result[1],"Source group ranks are incomplete");return result;
}
}
bool regional_playoff_appointment(const OtherAppointment& appointment){return appointment.domain==CalendarDomain::germany&&appointment.source_entry==0x4004;}
RegionalPlayoffRules regional_playoff_rules(const LeImage& source){
    const auto entry=cup_entry_address(35);require(source.contains(entry+0x27),"Source entry35 is unavailable");
    require(source.u16(entry+0x18)==13&&source.u8(entry+0x1a)==4&&source.u8(entry+0x1b)==6&&source.u8(entry+0x1e)==1&&source.u8(entry+0x1f)==2&&
        source.u8(entry+0x15)==255&&!source.u8(entry+0x16)&&!source.u8(entry+0x17)&&!source.u8(entry+0x20)&&
        (source.u8(entry+0x14)&5)==1,"Source group programme differs from four-team double group/quota2");
    return {source.u8(entry+0x14)};
}
RegionalPlayoffSeason make_regional_playoff_season(const World& world,const LeImage& source,const SeasonCalendar& calendar,uint32_t season){
    require(world.catalogue.scope.germany,"country is disabled");calendar.validate(world.catalogue);RegionalPlayoffSeason result;result.season=season;result.rules=regional_playoff_rules(source);result.dates=dates(calendar);result.scores.fill(0xcccc);
    uint16_t final=0;for(const auto& appointment:calendar.leagues)if((appointment.league.value==2||appointment.league.value==3)&&appointment.round==world.catalogue.league(appointment.league).rounds())final=std::max(final,appointment.day);
    require(final<result.dates[0],"first date precedes the real regional closing boundary");return result;
}
void validate_regional_playoff(const World& world){
    if(!world.regional_playoff)return;const auto& campaign=*world.regional_playoff;
    require(world.catalogue.scope.germany&&(campaign.rules.flags&5)==1&&campaign.played<=6&&campaign.revision==campaign.played,"saved rule/cursor/revision is invalid");
    for(size_t i=0;i<6;++i)require(campaign.dates[i]<366&&(!i||campaign.dates[i]>campaign.dates[i-1]),"saved dates are invalid");
    for(size_t i=0;i<12;++i)require(i<size_t(campaign.played)*2?(campaign.scores[i]>>10)==0:campaign.scores[i]==0xcccc,"score history disagrees with its cursor");
    std::set<ClubId> clubs;for(auto club:campaign.entrants){require(club.valid()==campaign.activation_day.has_value(),"inactive field contains a club");if(club.valid())require(world.clubs.contains(club)&&world.clubs.at(club).kind==ClubKind::club&&world.clubs.at(club).country==Country::germany&&clubs.insert(club).second,"field contains a repeated or foreign/nonclub identity");}
    for(const auto& [club,value]:campaign.activation_deductions)require(campaign.activation_day&&world.clubs.contains(club)&&world.clubs.at(club).kind==ClubKind::club&&world.clubs.at(club).country==Country::germany&&value<=31,"activation deduction identity/value is invalid");
    require(campaign.activation_deductions.size()<=4,"activation has more than four human deductions");
    if(campaign.activation_day)require(*campaign.activation_day<campaign.dates[0],"activation date is too late");else require(!campaign.played,"inactive field owns results");
    if(campaign.complete())require(campaign.qualifiers==ranked_qualifiers(campaign),"qualifiers differ from actual Source scores/ranks");else require(!campaign.qualifiers[0].valid()&&!campaign.qualifiers[1].valid(),"unfinished campaign owns qualifiers");
}
void validate_regional_playoff_snapshot(const CareerSnapshot& state){
    if(!state.world.regional_playoff)return;const auto& campaign=*state.world.regional_playoff;require(campaign.season==state.season,"saved season owner changed");
    for(const auto& [club,value]:campaign.activation_deductions){(void)value;const auto league=state.world.clubs.at(club).league;require(league.value==2||league.value==3,"activation deduction is outside the retained regional membership");}
    if(campaign.activation_day)require(campaign.entrants==entrants(state,campaign.activation_deductions),"field differs from completed physical regional standings");
    if(state.calendar){require(campaign.dates==dates(state.calendar->schedule),"saved campaign dates differ from calendar");size_t played=0;for(auto day:campaign.dates)if(day<state.calendar->next_day||(day==state.calendar->next_day&&state.world.indoor&&state.world.indoor->date_pass&&state.world.indoor->date_pass->ordinary_completed))++played;require(played==campaign.played,"calendar has skipped or repeated a playoff round");}
}
bool activate_regional_playoff(CareerSnapshot& staged,const CareerSnapshot& before){
    if(!staged.world.catalogue.scope.germany)return false;
    const auto final=staged.world.catalogue.league({2}).rounds();
    if(before.league_rounds[2]+1!=final||staged.league_rounds[2]!=final)return false;
    before.validate();staged.world.validate();
    require(staged.world.regional_playoff.has_value()&&staged.world.regional_playoff==before.world.regional_playoff,"activation lacks its unchanged known season owner");auto& campaign=*staged.world.regional_playoff;
    require(!campaign.activation_day&&campaign.season==staged.season&&before.season==staged.season&&before.date==staged.date&&staged.calendar&&before.calendar&&staged.calendar==before.calendar,"activation boundary is stale or already consumed");
    require(before.league_rounds[3]+1==final&&staged.league_rounds[3]==final,"both regional final rounds must finish together");
    const auto day=staged.calendar->next_day;
    for(auto league:{LeagueId{2},LeagueId{3}}){const auto planned=staged.calendar->schedule.leagues_on(day);require(std::any_of(planned.begin(),planned.end(),[&](const auto& a){return a.league==league&&a.round==final;}),"activation is outside the actual final appointment");
        for(int round=1;round<final;++round)for(int match=0;match<staged.world.catalogue.league(league).clubs/2;++match)require(staged.results.result(staged.world.catalogue,league,round,match)==before.results.result(before.world.catalogue,league,round,match),"activation changed earlier league history");}
    const auto cuts=deductions(staged.world);const auto selected=entrants(staged,cuts);require(day<campaign.dates[0],"activation cannot skip a playoff date");campaign.activation_day=day;campaign.entrants=selected;campaign.activation_deductions=cuts;validate_regional_playoff(staged.world);return true;
}
std::optional<RegionalPlayoffGroup> prepare_regional_playoff_group(const CareerSnapshot& state){
    require(state.calendar.has_value(),"calendar owner is missing");const auto day=state.calendar->next_day;const auto appointments=state.calendar->schedule.others_on(day);
    if(std::none_of(appointments.begin(),appointments.end(),regional_playoff_appointment))return {};
    require(state.world.regional_playoff.has_value(),"campaign is unknown in this older snapshot");const auto& campaign=*state.world.regional_playoff;
    require(campaign.season==state.season&&campaign.activation_day&&campaign.played<6&&campaign.dates[campaign.played]==day,"round/date owner is inactive, stale or completed");
    RegionalPlayoffGroup result;result.day=day;result.season=state.season;result.played=campaign.played;result.flags=campaign.rules.flags;result.revision=campaign.revision;
    for(const auto& fixture:bmh::generate_fixtures(4,campaign.played+1,3,0))result.matches.push_back({campaign.entrants[fixture.home],campaign.entrants[fixture.away]});return result;
}
void commit_regional_playoff_results(CareerSnapshot& state,const RegionalPlayoffResults& result,const LeImage& source,WatcomRandom& random){
    const auto expected=prepare_regional_playoff_group(state);require(expected&&*expected==result.group&&result.scores.size()==2,"result owner/fixtures are stale or incomplete");
    require(regional_playoff_rules(source)==state.world.regional_playoff->rules,"Source rules changed before completion");const auto before=state;auto candidate=state;auto staged_random=random;const auto seed=random.seed();const auto calls=random.calls();auto& campaign=*candidate.world.regional_playoff;
    for(size_t i=0;i<2;++i){require((result.scores[i]>>10)==0,"result has an unplayed/extra-time phase");campaign.scores[campaign.played*2+i]=result.scores[i];}
    Workspace workspace(campaign);MinuteReport report;bmh::advance_cup(workspace.state,13,staged_random,report);require(!report.unsupported,"Source group transition failed");campaign.played=workspace.state.memory.u8(cup_entry_address(35)+0x1d);++campaign.revision;
    if(campaign.complete())campaign.qualifiers=workspace.qualifiers(campaign);candidate.world.validate();
    require(state==before&&random.seed()==seed&&random.calls()==calls,"state/RNG ownership changed during completion");state=std::move(candidate);random=staged_random;
}
std::array<ClubId,2> annual_regional_qualifiers(const CareerSnapshot& state){
    state.validate();require(state.world.regional_playoff&&state.world.regional_playoff->activation_day&&state.world.regional_playoff->complete(),"annual requires all six actual playoff results");return state.world.regional_playoff->qualifiers;
}
}
