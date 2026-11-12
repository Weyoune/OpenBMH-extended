// SPDX-License-Identifier: GPL-3.0-only
#include "game/world12/indoor.hpp"
#include "game/world12/world.hpp"
#include "game/world12/save.hpp"
#include "game/world12/standings.hpp"
#include "game/world12/economy.hpp"
#include "game/world12/manager_finance.hpp"
#include "game/cup.hpp"
#include "game/matchday_setup.hpp"
#include "game/matchday.hpp"
#include "game/driver.hpp"
#include "game/schedule.hpp"
#include "game/messages.hpp"
#include "formats/le.hpp"
#include <algorithm>
#include <limits>
#include <set>
namespace bmh::world12 {
namespace {
void require(bool ok,const char* why){if(!ok)throw FormatError(std::string("world12: indoor ")+why);}
std::vector<ClubId> selection_slots(const World& world){
    std::vector<ClubId> out;require(world.catalogue.scope.germany,"selection requires the German ladder");
    for(uint8_t local=0;local<5;++local){const auto league=world.catalogue.starting_league(Country::germany,local);const auto& clubs=world.membership[league.value].clubs;out.insert(out.end(),clubs.begin(),clubs.end());}
    // This is the Original guest namespace, not all playable English divisions.
    for(uint32_t league:{10u,5u,11u,12u}){const auto& clubs=world.membership[league].clubs;out.insert(out.end(),clubs.begin(),clubs.end());}
    require(out.size()==164,"shared Source guest dimensions are unavailable");return out;
}
uint8_t active_stage(const IndoorTournament& t){if(t.played[0]<3)return 25;if(t.played[2]<1)return 27;return 28;}
std::string name(const GameState& g){const auto end=std::find(g.tournament_name.begin(),g.tournament_name.end(),uint8_t(0));return std::string(g.tournament_name.begin(),end);}
struct Workspace {
    AiMatchProjection projection;
    std::vector<ClubId> map;
    GameState& game(){return projection.state;}
    const GameState& game()const{return projection.state;}
    uint16_t local(ClubId id)const{if(!id.valid())return 65535;const auto found=std::find(map.begin(),map.end(),id);require(found!=map.end(),"club is outside its owned physical namespace");return uint16_t(found-map.begin());}
    ClubId global(uint16_t id)const{if(id==65535)return {};require(id<map.size(),"Core returned an unknown physical club");return map[id];}
    Workspace(const CareerSnapshot& state,const GameState& reference):map(state.world.indoor->selection_slots){
        require(runtime_dimensions.domestic_teams==90&&runtime_dimensions.league_teams==164&&!installed_league_system().english_competitions,"common shared numeric reference profile is required");
        const auto& world=state.world;if(map.empty()){map=selection_slots(world);for(auto id:world.indoor->previous_holders)if(std::find(map.begin(),map.end(),id)==map.end())map.push_back(id);for(auto id:world.indoor->masters_scratch)if(id.valid()&&std::find(map.begin(),map.end(),id)==map.end())map.push_back(id);}
        std::vector<ClubId> humans;for(const auto& manager:world.managers)humans.push_back(manager.club);
        const auto& settings=*world.settings;
        projection=humans.empty()?AiMatchProjection::create(world,{map.front()},reference):AiMatchProjection::create_for_humans(state,humans,reference,{settings.mode,settings.difficulty,settings.foreign_rule,true});
        if(world.economy)install_career_economy(projection,world);else require(humans.empty(),"invitation/prize booking needs canonical finance/mailbox state");
        auto& g=game();g.indoor=true;g.date=state.date;g.season=int(state.season);g.club_names={0};g.projected_match_clubs.clear();
        for(auto id:humans)if(std::find(map.begin(),map.end(),id)==map.end())map.push_back(id);
        require(map.size()<=g.club_ids.size(),"physical reference space is too small");g.projected_match_clubs.resize(map.size());
        std::map<LeagueId,std::map<ClubId,int>> ranks;
        for(size_t i=0;i<map.size();++i){const auto& club=world.clubs.at(map[i]);require(g.club_names.size()+club.name.size()+1<65536,"club-name reference overflow");const auto at=uint16_t(g.club_names.size());g.club_ids[i]=at;g.club_names.insert(g.club_names.end(),club.name.begin(),club.name.end());g.club_names.push_back(0);
            auto& context=g.projected_match_clubs[i];context.league_class=team_league(int(i));context.climate_region=int(club.country);context.foreign_player_limit=3;context.table_rank=-1;
            if(i<36){const auto league=club.league;const auto cursor=state.league_rounds[league.value];if(cursor){if(!ranks.contains(league)){TableQuery query;for(const auto& manager:world.managers)if(manager.match_state&&world.clubs.at(manager.club).league==league&&(manager.match_state->point_deduction>>5)==3)query.point_deductions[manager.club]=manager.match_state->point_deduction&31;for(const auto& row:compute_standings(world,state.results,league,cursor,query))ranks[league][row.club]=int(row.rank)-1;}context.table_rank=ranks.at(league).at(club.id);}}
        }
        for(size_t m=0;m<humans.size();++m)g.manager_teams[m]=local(humans[m]);
        auto& memory=g.memory;memory.add_block(kCupTable,kCupEntries*kCupEntrySize);for(int e=0;e<kCupEntries;++e)memory.set16(cup_entry_address(e)+0x18,65535);
        memory.add_block(layout::Teams,32);memory.add_block(layout::MatchdayGroupWorkspace,32);memory.add_block(layout::Cup_AdvanceCup_ExecutableField,80);memory.add_block(layout::MatchdaySetup_IndoorTournamentDraw_RuntimeField,80);memory.add_block(layout::Cup_MastersQualification_RuntimeField,8);memory.add_block(layout::LiveMatchday_FormatError_RuntimeField14,5);memory.add_block(layout::Cup_AdvanceCup_RuntimeField6,6);memory.add_block(layout::Cup_AdvanceCup_RuntimeField3,1);memory.add_block(layout::Cup_AdvanceCup_RuntimeField4,1);memory.add_block(layout::Cup_AdvanceCup_RuntimeField,1);memory.set8(layout::Cup_AdvanceCup_RuntimeField,1);memory.add_block(layout::ActiveSelectors,8);for(uint32_t i=0;i<8;++i)memory.set8(layout::ActiveSelectors+i,0x14);memory.set8(layout::ActiveSelectors,11);
        memory.add_block(layout::Titles,900);for(uint32_t i=0;i<900;++i)memory.set8(layout::Titles+i,255);
        const auto& indoor=*world.indoor;
        for(size_t i=0;i<40;++i){memory.set16(layout::Cup_AdvanceCup_ExecutableField+uint32_t(i*2),indoor.points[i]);memory.set16(layout::MatchdaySetup_IndoorTournamentDraw_RuntimeField+uint32_t(i*2),indoor.invitations[i]);}
        for(size_t i=0;i<5;++i)memory.set8(layout::LiveMatchday_FormatError_RuntimeField14+uint32_t(i),indoor.hosts[i].valid()?uint8_t(local(indoor.hosts[i])):0);
        for(size_t i=0;i<3;++i)memory.set16(layout::Cup_AdvanceCup_RuntimeField6+uint32_t(i*2),local(indoor.previous_holders[i]));
        for(size_t i=0;i<4;++i){const auto raw=indoor.selection_slots.empty()?indoor.initial_scratch_slots[i]:local(indoor.masters_scratch[i]);require(raw<map.size(),"Masters scratch Source position is invalid");memory.set16(layout::Cup_MastersQualification_RuntimeField+uint32_t(i*2),raw);}
        for(const auto& r:indoor.rules){const auto at=cup_entry_address(r.entry);memory.set8(at+0x14,r.flags);memory.set8(at+0x15,r.previous);memory.set8(at+0x16,r.next);memory.set8(at+0x17,r.parallel);memory.set16(at+0x18,11);memory.set8(at+0x1a,r.teams);memory.set8(at+0x1b,r.rounds);memory.set8(at+0x1e,uint8_t(r.groups|(r.group_index<<4)));memory.set8(at+0x1f,r.quota);memory.set_pointer(at,layout::Teams+r.team_offset*2);memory.set_pointer(at+8,layout::CareerSession_RuntimeError_RuntimeField);memory.set_pointer(at+12,layout::MatchdayGroupWorkspace+r.result_offset*2);memory.set_pointer(at+16,0);}
        if(indoor.tournaments.empty()){
            memory.set8(layout::Cup_AdvanceCup_RuntimeField4,0);memory.set8(layout::Cup_AdvanceCup_RuntimeField3,0);for(uint32_t i=0;i<16;++i){memory.set16(layout::Teams+i*2,0);memory.set16(layout::MatchdayGroupWorkspace+i*2,0xcccc);}for(const auto& r:indoor.rules)memory.set8(cup_entry_address(r.entry)+0x1d,0);
        }else install(indoor.tournaments.back());
    }
    void install(const IndoorTournament& t){auto& g=game();auto& m=g.memory;m.set8(layout::Cup_AdvanceCup_RuntimeField4,t.ordinal);m.set8(layout::Cup_AdvanceCup_RuntimeField3,t.strength);g.tournament_name.assign(t.name.begin(),t.name.end());g.tournament_name.resize(36);for(size_t i=0;i<16;++i){m.set16(layout::Teams+uint32_t(i*2),local(t.teams[i]));m.set16(layout::MatchdayGroupWorkspace+uint32_t(i*2),t.scores[i]);}for(size_t i=0;i<5;++i)m.set8(cup_entry_address(int(25+i))+0x1d,t.played[i]);}
    IndoorTournament capture(uint8_t ordinal)const{const auto& g=game();const auto& m=g.memory;IndoorTournament t;t.ordinal=ordinal;t.strength=m.u8(layout::Cup_AdvanceCup_RuntimeField3);t.name=name(g);if(ordinal<5)t.host=global(m.u8(layout::LiveMatchday_FormatError_RuntimeField14+ordinal));for(size_t i=0;i<16;++i){t.teams[i]=global(m.u16(layout::Teams+uint32_t(i*2)));t.scores[i]=m.u16(layout::MatchdayGroupWorkspace+uint32_t(i*2));}for(size_t i=0;i<5;++i)t.played[i]=m.u8(cup_entry_address(int(25+i))+0x1d);return t;}
    void publish_economy(CareerSnapshot& state)const{for(uint8_t m=0;m<projection.human_managers();++m){const auto seat=projection.global_manager(m);auto& actor=*std::find_if(state.world.managers.begin(),state.world.managers.end(),[&](const auto& actor){return actor.seat==seat;});actor.finance=manager_initial_finance_from_legacy(game(),m);}if(state.world.economy)capture_career_economy(state.world,projection);}
    void capture_season(IndoorSeason& out)const{const auto& m=game().memory;for(size_t i=0;i<40;++i){out.points[i]=m.u16(layout::Cup_AdvanceCup_ExecutableField+uint32_t(i*2));out.invitations[i]=m.u16(layout::MatchdaySetup_IndoorTournamentDraw_RuntimeField+uint32_t(i*2));}for(size_t i=0;i<5;++i)if(i<out.tournaments.size()&&i<5)out.hosts[i]=global(m.u8(layout::LiveMatchday_FormatError_RuntimeField14+uint32_t(i)));for(size_t i=0;i<4;++i)out.masters_scratch[i]=global(m.u16(layout::Cup_MastersQualification_RuntimeField+uint32_t(i*2)));}
};
}
bool german_indoor_appointment(const OtherAppointment& a){return a.domain==CalendarDomain::germany&&a.source_entry==0x4002;}
std::array<IndoorStageRules,5> indoor_programme_rules(const LeImage& source){
    std::array<IndoorStageRules,5> result;for(size_t i=0;i<5;++i){const auto entry=uint8_t(25+i);const auto at=cup_entry_address(entry);require(source.contains(at+0x27)&&source.u16(at+0x18)==11,"Source stage belongs to another competition");auto& r=result[i];r.entry=entry;r.flags=source.u8(at+0x14);r.previous=source.u8(at+0x15);r.next=source.u8(at+0x16);r.parallel=source.u8(at+0x17);r.teams=source.u8(at+0x1a);r.rounds=source.u8(at+0x1b);r.groups=source.u8(at+0x1e)&15;r.group_index=source.u8(at+0x1e)>>4;r.quota=source.u8(at+0x1f);r.team_offset=source.u8(layout::Career_CompetitionPointers_RuntimeField5+uint32_t(i));r.result_offset=source.u8(layout::Career_CompetitionPointers_RuntimeField4+uint32_t(i));
        const uint8_t team_offsets[]={0,4,8,12,14},result_offsets[]={0,6,12,14,15};require(r.team_offset==team_offsets[i]&&r.result_offset==result_offsets[i]&&r.teams==(i<2||i==2?4:2)&&r.rounds==(i<2?3:1),"Source field/result geometry is unsupported");}
    require(result[0].next==27&&result[0].parallel==26&&result[1].next==27&&result[2].next==28&&result[3].parallel==29&&!result[3].next&&!result[4].next,"Source programme links are unsupported");return result;
}
IndoorSeason initial_indoor_season(const World& world,const LeImage& source,const GameState& reference){
    require(world.catalogue.scope.germany&&world.initial_competitions&&!world.indoor,"fresh German Source holder state is required");IndoorSeason out;out.rules=indoor_programme_rules(source);const auto slots=selection_slots(world);const auto find=[&](const auto& rows){const auto row=std::find_if(rows.begin(),rows.end(),[](const auto& row){return row.first==Country::germany;});require(row!=rows.end(),"German title holder is unknown");return row->second;};out.previous_holders[0]=find(world.initial_competitions->champions);out.previous_holders[1]=find(world.initial_competitions->domestic_holders);require(reference.memory.has(layout::Cup_AdvanceCup_RuntimeField7+1),"previous Masters Source slot is unknown");const auto previous=reference.memory.u16(layout::Cup_AdvanceCup_RuntimeField7);require(previous<slots.size(),"previous Masters Source slot is invalid");out.previous_holders[2]=slots[previous];for(size_t i=0;i<4;++i)out.initial_scratch_slots[i]=reference.memory.has(layout::Cup_MastersQualification_RuntimeField+uint32_t(i*2+1))?reference.memory.u16(layout::Cup_MastersQualification_RuntimeField+uint32_t(i*2)):0;return out;
}
std::vector<IndoorGroup> prepare_indoor_groups(const IndoorSeason& indoor){
    require(!indoor.tournaments.empty()&&!indoor.masters_winner.valid(),"current field is unavailable or complete");const auto& t=indoor.tournaments.back();const auto first=active_stage(t);std::vector<IndoorGroup> out;for(const auto& r:indoor.rules)if(r.entry==first||r.entry==first+1&&(first==25||first==28)){
        IndoorGroup group;group.revision=indoor.revision;group.tournament=t.ordinal;group.source_entry=r.entry;group.played=t.played[r.entry-25];group.flags=r.flags;
        if(r.flags&4){for(size_t i=0;i<r.teams;i+=2)group.matches.push_back({t.teams[r.team_offset+i],t.teams[r.team_offset+i+1]});}
        else if(r.teams==2)group.matches.push_back({t.teams[r.team_offset],t.teams[r.team_offset+1]});
        else for(const auto& f:bmh::generate_fixtures(4,group.played+1,3,0))group.matches.push_back({t.teams.at(r.team_offset+size_t(f.home)),t.teams.at(r.team_offset+size_t(f.away))});out.push_back(std::move(group));}
    return out;
}
namespace {
void validate_rules(const std::array<IndoorStageRules,5>& rules){
    const uint8_t teams[]={4,4,4,2,2},rounds[]={3,3,1,1,1},next[]={27,27,28,0,0},parallel[]={26,0,0,29,0},groups[]={2,2,1,2,2},group_index[]={0,1,0,0,1},team_offset[]={0,4,8,12,14},result_offset[]={0,6,12,14,15};
    for(size_t i=0;i<5;++i){const auto& r=rules[i];require(r.entry==25+i&&r.teams==teams[i]&&r.rounds==rounds[i]&&r.next==next[i]&&r.parallel==parallel[i]&&r.groups==groups[i]&&r.group_index==group_index[i]&&r.quota==4&&r.team_offset==team_offset[i]&&r.result_offset==result_offset[i]&&bool(r.flags&4)==(i==2)&&!(r.flags&1)&&bool(r.flags&8)==(i>=2),"saved Source programme geometry/flags are invalid");}
}
int winning_side(uint16_t score,uint8_t flags){MatchGroup group;group.desc.assign(0xd0,0);group.desc[1]=flags;group.cup_results={score};const auto side=cup_tie(group,0);require(side<2,"saved knockout/classification tie is unresolved");return side;}
}
void validate_indoor_season(const World& world){
    if(!world.indoor)return;const auto& s=*world.indoor;validate_rules(s.rules);require(world.catalogue.scope.germany&&world.initial_competitions&&s.tournaments.size()<=6,"season scope/history is invalid");for(auto id:s.previous_holders)require(world.clubs.contains(id)&&world.clubs.at(id).kind==ClubKind::club,"previous holder is missing");if(s.selection_slots.empty()){require(s.tournaments.empty()&&s.revision==0&&!s.date_pass&&!s.masters_winner.valid(),"undrawn season owns played state");return;}
    require(s.selection_slots.size()>=164&&s.selection_slots.size()<=171,"physical Source namespace is invalid");std::set<ClubId> slots;for(auto id:s.selection_slots)require(world.clubs.contains(id)&&world.clubs.at(id).kind==ClubKind::club&&slots.insert(id).second,"physical Source club is missing/repeated");const auto current=selection_slots(world);require(std::equal(current.begin(),current.end(),s.selection_slots.begin()),"season Source physical order changed");for(auto id:s.masters_scratch)require(slots.contains(id),"Masters scratch identity is unknown");uint64_t passes=0;std::array<uint16_t,40> expected_invitations{},expected_points{};std::set<ClubId> hosts;
    for(size_t n=0;n<s.tournaments.size();++n){const auto& t=s.tournaments[n];require(t.ordinal==n&&t.name.size()<=36&&t.strength<=255,"tournament identity/name is invalid");for(auto id:t.teams)require(!id.valid()||slots.contains(id),"tournament refers outside its Source namespace");for(size_t i=0;i<5;++i)require(t.played[i]<=s.rules[i].rounds,"stage cursor is invalid");require(t.played[0]==t.played[1]&&t.played[3]==t.played[4]&&(!t.played[2]||t.played[0]==3)&&(!t.played[3]||t.played[2]==1),"stage chronology is invalid");passes+=t.played[0]+t.played[2]+t.played[3];if(n+1<s.tournaments.size())require(t.complete,"unfinished earlier tournament");if(t.complete){require(t.played[3]==1,"completed tournament lacks its classification");std::set<ClubId> classified;for(auto id:t.places)require(slots.contains(id)&&classified.insert(id).second,"classification is missing/repeated");}else {require(t.played[3]==0,"unpublished tournament completion");}
        for(auto score:t.scores)require(score==0xcccc||(score>>10)<=2,"invalid score phase");
        std::set<ClubId> field;for(size_t i=0;i<8;++i)require(slots.contains(t.teams[i])&&field.insert(t.teams[i]).second,"drawn field is missing/repeated");
        if(n<5){require(t.host==s.hosts[n]&&field.contains(t.host)&&hosts.insert(t.host).second,"Source host is missing/repeated or outside its field");for(auto club:field){const auto slot=size_t(std::find(s.selection_slots.begin(),s.selection_slots.end(),club)-s.selection_slots.begin());if(slot<40)++expected_invitations[slot];}}
        for(size_t i=0;i<5;++i){const auto& r=s.rules[i];for(size_t match=0;match<size_t(r.teams)*r.rounds/2;++match){const auto score=t.scores[r.result_offset+match];const bool played=match<size_t(t.played[i])*r.teams/2;require((score!=0xcccc)==played,"score arrays differ from their Source stage cursors");if(played)require(!(score>>10)||(r.flags&8),"group match has an extra-time/shootout phase");}}
        if(t.played[2]){std::array<ClubId,2> winners,losers;for(size_t m=0;m<2;++m){const auto side=winning_side(t.scores[12+m],s.rules[2].flags);winners[m]=t.teams[8+m*2+size_t(side)];losers[m]=t.teams[8+m*2+size_t(1-side)];}require(t.teams[12]==winners[0]&&t.teams[13]==winners[1]&&t.teams[14]==losers[0]&&t.teams[15]==losers[1],"classification field differs from its semifinal results");}
        if(t.complete){const auto first=winning_side(t.scores[14],s.rules[3].flags),third=winning_side(t.scores[15],s.rules[4].flags);require(t.places==std::array<ClubId,4>{t.teams[12+size_t(first)],t.teams[12+size_t(1-first)],t.teams[14+size_t(third)],t.teams[14+size_t(1-third)]},"saved places differ from actual classification results");if(n<5){constexpr uint16_t points[]={6,4,2,1};for(size_t place=0;place<4;++place){const auto slot=size_t(std::find(s.selection_slots.begin(),s.selection_slots.end(),t.places[place])-s.selection_slots.begin());if(slot<36)expected_points[slot]=uint16_t(expected_points[slot]+points[place]*t.strength);}}}
}
    require(s.invitations==expected_invitations&&s.points==expected_points,"invitation/point totals differ from their Source history");if(s.revision!=passes+1)throw FormatError("world12: indoor season revision "+std::to_string(s.revision)+" differs from Source pass history "+std::to_string(passes+1));if(s.masters_winner.valid())require(s.tournaments.size()==6&&s.tournaments.back().complete&&s.tournaments.back().places[0]==s.masters_winner,"Masters award differs from its completed final");if(s.date_pass)require(s.date_pass->total>=2&&s.date_pass->total<=3&&s.date_pass->completed<s.date_pass->total,"saved date pass count is invalid");
}
IndoorTransitionOutcome prepare_indoor_draw(CareerSnapshot& state,const GameState& reference,const LeImage& source){
    if(!state.world.catalogue.scope.germany||state.league_rounds[0]!=17)return {};require(state.world.economy&&state.world.economy->presentation_options,"saved Q5 policy is unknown");if((*state.world.economy->presentation_options)[16*9+4])return {};require(state.world.indoor.has_value(),"season owner is unknown in this older snapshot");if(!state.world.indoor->tournaments.empty())return {};state.validate();const auto before=state;auto candidate=state;auto& indoor=*candidate.world.indoor;require(indoor.rules==indoor_programme_rules(source),"Source rules changed before first draw");Workspace workspace(candidate,reference);indoor.selection_slots=selection_slots(candidate.world);for(auto id:indoor.previous_holders)if(std::find(indoor.selection_slots.begin(),indoor.selection_slots.end(),id)==indoor.selection_slots.end())indoor.selection_slots.push_back(id);for(auto id:indoor.masters_scratch)if(id.valid()&&std::find(indoor.selection_slots.begin(),indoor.selection_slots.end(),id)==indoor.selection_slots.end())indoor.selection_slots.push_back(id);install_message_texts(source);GameRandom game(state.game_seed);WatcomRandom random(state.watcom_seed);bmh::indoor_tournament_draw(workspace.game(),game,random);indoor.tournaments.push_back(workspace.capture(0));workspace.capture_season(indoor);workspace.publish_economy(candidate);indoor.revision=1;candidate.game_seed=game.seed();candidate.watcom_seed=random.seed();candidate.validate();require(state==before,"first draw lost its World/seed ownership");state=std::move(candidate);return {true,game.calls(),random.calls()};
}
IndoorTransitionOutcome commit_indoor_results(CareerSnapshot& state,const IndoorResults& result,const GameState& reference,const LeImage& source){
    state.validate();require(state.world.indoor.has_value(),"result owner is unknown");const auto& original=*state.world.indoor;require(original.revision==result.revision&&result.revision!=std::numeric_limits<uint64_t>::max()&&!original.tournaments.empty()&&original.tournaments.back().ordinal==result.tournament&&!original.masters_winner.valid(),"stale result owner");require(original.rules==indoor_programme_rules(source),"Source programme changed");const auto groups=prepare_indoor_groups(original);require(result.groups.size()==groups.size(),"parallel result batch is incomplete");const auto before=state;auto candidate=state;auto& indoor=*candidate.world.indoor;Workspace workspace(candidate,reference);auto& memory=workspace.game().memory;
    for(size_t i=0;i<groups.size();++i){const auto& plan=groups[i];const auto& got=result.groups[i];require(got.source_entry==plan.source_entry&&got.played==plan.played&&got.scores.size()==plan.matches.size(),"result belongs to another stage/pass");const auto& rule=indoor.rules[plan.source_entry-25];for(size_t m=0;m<got.scores.size();++m){require((got.scores[m]>>10)<=2&&((plan.flags&8)||!(got.scores[m]>>10)),"score phase is invalid");memory.set16(layout::MatchdayGroupWorkspace+uint32_t((rule.result_offset+plan.played*(rule.teams/2)+m)*2),got.scores[m]);}if(plan.flags&8){MatchGroup tie;tie.desc.assign(0xd0,0);tie.desc[1]=plan.flags;tie.cup_results=got.scores;for(size_t m=0;m<got.scores.size();++m)require(cup_tie(tie,int(m))!=2,"classification tie is unresolved");}}
    GameRandom game(state.game_seed);WatcomRandom random(state.watcom_seed);MinuteReport report;std::optional<IndoorTournament> completed;
    bmh::advance_cup(workspace.game(),11,random,report,&game,{}, {},[&](const GameState&,const IndoorTournamentCompletion& receipt){completed=workspace.capture(receipt.ordinal);completed->complete=true;for(size_t i=0;i<4;++i)completed->places[i]=workspace.global(receipt.places[i]);});require(!report.unsupported,"Core result/draw transition is unsupported");
    if(completed){indoor.tournaments.back()=*completed;if(completed->ordinal<5)indoor.tournaments.push_back(workspace.capture(uint8_t(completed->ordinal+1)));else indoor.masters_winner=completed->places[0];}else indoor.tournaments.back()=workspace.capture(result.tournament);
    workspace.capture_season(indoor);workspace.publish_economy(candidate);++indoor.revision;candidate.game_seed=game.seed();candidate.watcom_seed=random.seed();candidate.world.validate();require(state==before,"result/draw callback lost its career ownership");state=std::move(candidate);return {true,game.calls(),random.calls()};
}
}
