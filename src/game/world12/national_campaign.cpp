// SPDX-License-Identifier: GPL-3.0-only
#include "game/world12/national_campaign.hpp"
#include "game/world12/national_final_facts.hpp"
#include "game/world12/world.hpp"
#include "game/world12/calendar.hpp"
#include "game/national_draw.hpp"
#include "game/cup.hpp"
#include "game/driver.hpp"
#include "game/schedule.hpp"
#include "formats/le.hpp"
#include <algorithm>
#include <functional>
#include <limits>
#include <set>

namespace bmh::world12 {
namespace {
void require(bool ok,const char* why){if(!ok)throw FormatError(std::string("world12: national ")+why);}
std::array<ClubId,62> nations(const World& world){
    std::array<ClubId,62> out;
    for(const auto& [id,club]:world.clubs)if(club.kind==ClubKind::national_team){const auto n=size_t(club.country);require(n<62&&!out[n].valid(),"country identity is missing or ambiguous");out[n]=id;}
    for(auto id:out)require(id.valid(),"Source country pool is incomplete");return out;
}
size_t slots(const NationalStageRules& r){if(!r.knockout())return r.teams;size_t count=0;for(int i=0;i<r.rounds;++i)count+=r.teams>>i;return count;}
size_t offset(const NationalStage& s){size_t count=0;for(int i=0;i<s.played/((s.rules.flags&1)+1);++i)count+=s.rules.teams>>i;return count;}
NationalStage& stage(NationalCampaign& c,uint8_t entry){auto i=std::find_if(c.stages.begin(),c.stages.end(),[&](const auto& s){return s.rules.entry==entry;});require(i!=c.stages.end(),"programme link is missing");return *i;}
const NationalStage& stage(const NationalCampaign& c,uint8_t entry){return stage(const_cast<NationalCampaign&>(c),entry);}
bool started(const NationalStage& s){return !s.teams.empty()&&s.teams.front().valid();}
uint8_t duration(const NationalStageRules& r){return uint8_t(r.rounds*(r.knockout()?((r.flags&1)+1):1));}
void validate_rules(const std::vector<NationalStageRules>& rules,uint32_t season){
    const int cycle=int(season%4);const size_t count=cycle==0||cycle==2?1:cycle==1?3:9;
    require(rules.size()==count,"programme has the wrong number of stages");
    std::map<uint8_t,NationalStageRules> entries;
    for(size_t i=0;i<rules.size();++i){const auto& r=rules[i];const uint8_t expected=uint8_t(cycle==0?14:cycle==1?10+i:cycle==2?13:1+i);
        require(r.entry==expected&&entries.emplace(r.entry,r).second&&!r.seeds,"Source entry/seed geometry is unsupported");
        if(cycle==0||cycle==2)require(!r.knockout()&&r.flags==195&&r.teams==(cycle==0?4:6)&&r.rounds==(cycle==0?6:10)&&r.groups==1&&!r.group_index&&r.quota==(cycle==0?1:2)&&r.previous==255&&!r.next&&!r.parallel,"league tournament geometry is unsupported");
        else if(i<(cycle==1?2u:6u)){const auto groups=uint8_t(cycle==1?2:6),next=uint8_t(cycle==1?12:7);
            require(r.flags==192&&r.teams==4&&r.rounds==3&&r.groups==groups&&r.group_index==i&&r.quota==(cycle==1?4:16)&&r.previous==255&&r.next==next&&r.parallel==(i+1<groups?r.entry+1:0),"group programme geometry is unsupported");
        }else if(cycle==1)require(r.flags==204&&r.teams==4&&r.rounds==2&&r.groups==1&&!r.group_index&&r.quota==3&&r.previous==10&&!r.next&&!r.parallel,"four-team knockout geometry is unsupported");
        else if(i==6)require(r.flags==140&&r.teams==16&&r.rounds==3&&r.groups==1&&!r.group_index&&r.quota==4&&r.previous==1&&r.next==8&&!r.parallel,"World-Cup knockout geometry is unsupported");
        else require(r.flags==200&&r.teams==2&&r.rounds==1&&r.groups==2&&r.group_index==i-7&&r.quota==4&&r.previous==7&&!r.next&&r.parallel==(i==7?9:0),"final/third-place geometry is unsupported");
    }
}
void validate_storage(const NationalStage& s,const std::set<ClubId>& entrants){
    const auto& r=s.rules;require(s.teams.size()==slots(r)&&s.played<=duration(r),"stage storage/cursor is invalid");
    const auto scores=r.knockout()?slots(r)/2:size_t(r.rounds)*r.teams/2;
    for(const auto& leg:s.scores){require(leg.size()==scores,"score storage dimension is invalid");for(auto score:leg)require(score==0xcccc||((score>>10)<=2&&(!(score>>10)||(r.flags&8))),"score phase is invalid");}
    for(auto id:s.teams)require(!id.valid()||entrants.contains(id),"stage contains a non-entrant");
    for(auto id:{s.winner,s.runner_up})require(!id.valid()||entrants.contains(id),"stage award contains a non-entrant");
    const bool begun=started(s);if(!begun)require(!s.played&&!s.winner.valid()&&!s.runner_up.valid(),"unstarted stage owns results/awards");
    if(r.knockout()){size_t first=0;const auto legs=(r.flags&1)+1,finished=s.played/legs;
        for(int round=0;round<r.rounds;++round){const auto count=size_t(r.teams)>>round;const bool drawn=begun&&round<=finished;std::set<ClubId> unique;
            for(size_t i=0;i<count;++i){const auto id=s.teams[first+i];require(id.valid()==drawn&&(!drawn||unique.insert(id).second),"knockout field is premature, repeated or incomplete");}
            for(size_t leg=0;leg<2;++leg)for(size_t i=0;i<count/2;++i){const bool played=begun&&(round<finished&&(leg==0||legs==2)||round==finished&&legs==2&&s.played%2&&leg==1);require((s.scores[leg][first/2+i]!=0xcccc)==played,"knockout score/cursor disagrees");}
            first+=count;
        }
    }else {std::set<ClubId> unique;for(auto id:s.teams)require(id.valid()==begun&&(!begun||unique.insert(id).second),"group field is incomplete or repeated");
        for(size_t i=0;i<s.scores[0].size();++i){require((s.scores[0][i]!=0xcccc)==(begun&&i<size_t(s.played)*r.teams/2),"group score/cursor disagrees");require(s.scores[1][i]==0xcccc,"group stage owns knockout return-leg scores");}}
}
std::pair<ClubId,ClubId> source_stage_awards(const NationalStage& s){
    const auto& r=s.rules;require(s.played==duration(r),"stage facts require complete scores");
    for(auto word:s.scores[0])require(word!=0xcccc&&(word>>10)<=2&&(!(word>>10)||(r.flags&8)),"final facts require actual played scores");
    if(r.knockout()){
        if((r.teams>>r.rounds)!=1)return {};
        size_t first=0;for(int round=0;round<int(r.rounds)-1;++round)first+=size_t(r.teams)>>round;
        MatchGroup group;group.desc.assign(0xd0,0);group.desc[1]=r.flags;group.cup_results=s.scores[0];if(r.flags&1)group.cup_other_leg=s.scores[1];
        const auto winner=cup_tie(group,int(first/2));require(winner!=2,"final score is unresolved");
        return {s.teams.at(first+size_t(winner)),s.teams.at(first+size_t(1-winner))};
    }
    StaticMemory memory;const auto entry=cup_entry_address(r.entry);memory.add_block(entry,0x28);
    memory.set8(entry+0x14,r.flags);memory.set8(entry+0x1a,r.teams);memory.set8(entry+0x1b,r.rounds);memory.set8(entry+0x1d,s.played);
    memory.set_pointer(entry+8,r.teams==6?layout::CareerSession_RuntimeError_RuntimeField2:layout::CareerSession_RuntimeError_RuntimeField);
    constexpr uint32_t results=0x1e00000;memory.add_block(results,s.scores[0].size()*2);memory.set_pointer(entry+12,results);
    for(size_t i=0;i<s.scores[0].size();++i)memory.set16(results+uint32_t(i*2),s.scores[0][i]);
    const auto table=cup_group_table(memory,entry);ClubId winner,runner;
    for(int row=0;row<r.teams;++row){if(table.rank(row)==0)winner=s.teams.at(size_t(row));if(table.rank(row)==1&&r.quota>1)runner=s.teams.at(size_t(row));}
    require(winner.valid()&&(r.quota==1||runner.valid()),"Source ranking has no final places");return {winner,runner};
}
struct Workspace {
    GameState game;std::array<ClubId,62> ids;Country primary=Country::germany;
    Workspace(const NationalCampaign& c,const World& world):ids(nations(world)),primary(world.national_campaigns->primary_country){
        game.season=int(std::min(c.season,49u));game.club_ids.resize(size_t(kNoTeam)+62);game.club_names={0};
        for(size_t i=0;i<ids.size();++i){const auto& name=world.clubs.at(ids[i]).name;require(game.club_names.size()+name.size()+1<65536,"name reference space exceeded");game.club_ids[size_t(kNoTeam)+i]=uint16_t(game.club_names.size());game.club_names.insert(game.club_names.end(),name.begin(),name.end());game.club_names.push_back(0);}
        auto& m=game.memory;m.add_block(kCupTable,kCupEntries*kCupEntrySize);for(int e=0;e<kCupEntries;++e)m.set16(cup_entry_address(e)+0x18,65535);
        m.add_block(layout::Cup_AdvanceCup_RuntimeField,1);m.set8(layout::Cup_AdvanceCup_RuntimeField,1);
        m.add_block(layout::Titles,900);for(uint32_t i=0;i<900;++i)m.set8(layout::Titles+i,255);
        m.add_block(layout::Placements,80);
        if(world.national_campaigns)for(size_t row=0;row<world.national_campaigns->honours.size();++row)for(size_t place=0;place<4;++place)m.set8(layout::Placements+uint32_t(row*4+place),uint8_t(source_country(country(world.national_campaigns->honours[row][place]))|0x80));
        uint32_t cursor=0x1000000;
        for(const auto& s:c.stages){const auto& r=s.rules;const auto at=cup_entry_address(r.entry);
            m.set8(at+0x14,r.flags);m.set8(at+0x15,r.previous);m.set8(at+0x16,r.next);m.set8(at+0x17,r.parallel);m.set16(at+0x18,c.competition);m.set8(at+0x1a,r.teams);m.set8(at+0x1b,r.rounds);m.set8(at+0x1d,s.played);m.set8(at+0x1e,uint8_t(r.groups|(r.group_index<<4)));m.set8(at+0x1f,r.quota);m.set8(at+0x20,0);
            const auto allocate=[&](size_t words){const auto address=cursor;cursor+=uint32_t(words*2+16);m.add_block(address,words*2+16);return address;};
            const auto list=allocate(s.teams.size());m.set_pointer(at,list);for(size_t i=0;i<s.teams.size();++i)m.set16(list+uint32_t(i*2),local(s.teams[i]));
            for(size_t leg=0;leg<2;++leg){const auto results=allocate(s.scores[leg].size());const bool used=leg==0||(r.knockout()&&(r.flags&1));m.set_pointer(at+uint32_t(12+4*leg),used?results:0);for(size_t i=0;i<s.scores[leg].size();++i)m.set16(results+uint32_t(i*2),s.scores[leg][i]);}
            if(!r.knockout())m.set_pointer(at+8,r.teams==6?layout::CareerSession_RuntimeError_RuntimeField2:layout::CareerSession_RuntimeError_RuntimeField);
            m.set16(at+0x22,local(s.winner));m.set16(at+0x24,local(s.runner_up));
        }
    }
    uint8_t country(ClubId id)const{const auto i=std::find(ids.begin(),ids.end(),id);require(i!=ids.end(),"unknown country reference");return uint8_t(i-ids.begin());}
    uint8_t source_country(uint8_t country)const {return primary==Country::england?(country==0?2:country==2?0:country):country;}
    uint16_t local(ClubId id)const{return id.valid()?uint16_t(kNoTeam+source_country(country(id))):65535;}
    ClubId global(uint16_t id)const{if(id==65535)return {};require(id>=kNoTeam&&id<kNoTeam+62,"Core returned an unknown national identity");return ids[source_country(uint8_t(id-kNoTeam))];}
    void capture(NationalCampaign& c)const{const auto& m=game.memory;for(auto& s:c.stages){const auto at=cup_entry_address(s.rules.entry);s.played=m.u8(at+0x1d);for(size_t i=0;i<s.teams.size();++i)s.teams[i]=global(m.u16(m.pointer(at)+uint32_t(i*2)));for(size_t leg=0;leg<2;++leg)if(m.pointer(at+uint32_t(12+4*leg)))for(size_t i=0;i<s.scores[leg].size();++i)s.scores[leg][i]=m.u16(m.pointer(at+uint32_t(12+4*leg))+uint32_t(i*2));s.winner=global(m.u16(at+0x22));s.runner_up=global(m.u16(at+0x24));}}
    void capture_honours(NationalCampaignHistory& history)const{history.honours.clear();for(uint32_t row=0;row<20;++row){std::array<ClubId,4> places;bool complete=true;for(uint32_t p=0;p<4;++p){const auto raw=game.memory.u8(layout::Placements+row*4+p);if(!(raw&128)){complete=false;break;}require((raw&127)<62,"Core honour country is invalid");places[p]=ids[source_country(raw&127)];}if(!complete)break;history.honours.push_back(places);}}
};
}
NationalFinalFacts national_final_facts(const NationalCampaign& c){
    require(c.complete&&c.season<=32767&&c.competition==7-c.season%4,"final facts require completed known tournament");std::vector<NationalStageRules> rules;for(const auto& s:c.stages)rules.push_back(s.rules);validate_rules(rules,c.season);const std::set<ClubId> entrants(c.entrants.begin(),c.entrants.end());for(const auto& s:c.stages)validate_storage(s,entrants);const auto& final=stage(c,c.active_entry);require(!final.rules.next&&!final.rules.group_index,"final owner is not terminal");
    const auto [winner,runner]=source_stage_awards(final);NationalFinalFacts facts;facts.season=c.season;facts.competition=c.competition;facts.winner=winner;facts.runner_up=runner;
    if(c.competition==4){require(final.rules.parallel!=0,"World-Cup third-place score is missing");const auto third=source_stage_awards(stage(c,final.rules.parallel));facts.places={winner,runner,third.first,third.second};}
    require(final.winner==facts.winner&&final.runner_up==facts.runner_up&&c.winner==facts.winner&&c.runner_up==facts.runner_up&&c.places==facts.places,"stored awards disagree with actual Source final scores");
    if(c.competition==4){const auto& third=stage(c,final.rules.parallel);require(third.winner==facts.places[2]&&third.runner_up==facts.places[3],"stored third-place awards disagree with Source scores");}
    return facts;
}
uint16_t national_calendar_flag(uint32_t season){return uint16_t(1u<<(12-season%4));}
bool national_appointment(const OtherAppointment& a,uint32_t season){return a.domain==CalendarDomain::shared&&!(a.source_entry&0x4000)&&(a.source_entry&national_calendar_flag(season));}
std::vector<NationalStageRules> national_programme_rules(const LeImage& source,uint32_t season){
    const uint8_t competition=uint8_t(7-season%4);std::vector<NationalStageRules> out;
    for(int entry=0;entry<kCupEntries;++entry){const auto at=cup_entry_address(entry);require(source.contains(at+0x27),"Source table is truncated");if(source.u16(at+0x18)!=competition)continue;NationalStageRules r;
        r.entry=uint8_t(entry);r.flags=source.u8(at+0x14);r.previous=source.u8(at+0x15);r.next=source.u8(at+0x16);r.parallel=source.u8(at+0x17);r.teams=source.u8(at+0x1a);r.rounds=source.u8(at+0x1b);r.groups=source.u8(at+0x1e)&15;r.group_index=source.u8(at+0x1e)>>4;r.quota=source.u8(at+0x1f);r.seeds=source.u8(at+0x20);out.push_back(r);
    }validate_rules(out,season);return out;
}
NationalDrawOutcome prepare_national_campaign(World& world,const LeImage& source,uint32_t season,GameRandom& game,WatcomRandom& random,Country primary){
    world.validate();require(world.catalogue.scope.international,"international scope is disabled");require(season<=32767,"season exceeds Native history range");const auto before=world;const auto old_g=game;const auto old_w=random;const auto ids=nations(world);require(primary==Country::germany||primary==Country::england,"primary country is not a Source national-only choice");require(!world.catalogue.scope.club_play()||primary==Country::germany,"club world retains its established German Source draw");const auto mapped=[&](uint8_t country){return primary==Country::england?(country==0?uint8_t(2):country==2?uint8_t(0):country):country;};auto staged_g=game;auto staged_w=random;
    auto candidate=world;if(!candidate.national_campaigns){require(season==0,"previous campaign history is unknown");candidate.national_campaigns.emplace();candidate.national_campaigns->primary_country=primary;}
    auto& history=*candidate.national_campaigns;require(history.primary_country==primary,"primary country changed during a campaign history");require(!history.seasons.contains(season),"season is already initialized");if(!history.seasons.empty())require(history.seasons.rbegin()->first+1==season&&history.seasons.rbegin()->second.complete,"preceding season is incomplete or absent");else require(season==0,"first owned season must be zero");
    NationalCampaign c;c.season=season;c.competition=uint8_t(7-season%4);const auto rules=national_programme_rules(source,season);c.active_entry=rules.front().entry;
    if(season==0&&world.initial_competitions){c.entrants=world.initial_competitions->national_field;require(c.entrants.size()==4,"initial field is unknown");}
    else {std::vector<uint16_t> previous;std::optional<uint16_t> winner,runner;if(season%2){const auto& prior=history.seasons.at(season-1);(void)national_final_facts(prior);for(auto id:prior.entrants)previous.push_back(mapped(uint8_t(world.clubs.at(id).country)));if(prior.winner.valid())winner=mapped(uint8_t(world.clubs.at(prior.winner).country));if(prior.runner_up.valid())runner=mapped(uint8_t(world.clubs.at(prior.runner_up).country));require(previous.size()==(season%4==1?4u:6u),"preceding field has the wrong size");}
        for(auto country:draw_national_tournament_field(season,previous,winner,runner,staged_g,staged_w)){require(country<62,"draw returned an unowned Native scratch token");c.entrants.push_back(ids[mapped(uint8_t(country))]);}}
    for(const auto& r:rules){NationalStage s;s.rules=r;s.teams.resize(slots(r));for(auto& scores:s.scores)scores.assign(r.knockout()?slots(r)/2:size_t(r.rounds)*r.teams/2,0xcccc);if(r.previous==255){const size_t first=size_t(r.group_index)*r.teams;require(first+r.teams<=c.entrants.size(),"initial field does not fit programme");std::copy_n(c.entrants.begin()+ptrdiff_t(first),r.teams,s.teams.begin());}c.stages.push_back(std::move(s));}
    history.seasons.emplace(season,std::move(c));candidate.validate();require(world==before&&game.seed()==old_g.seed()&&game.calls()==old_g.calls()&&random.seed()==old_w.seed()&&random.calls()==old_w.calls(),"draw ownership changed");world=std::move(candidate);game=staged_g;random=staged_w;return {game.calls()-old_g.calls(),random.calls()-old_w.calls()};
}
std::vector<NationalGroup> prepare_national_groups(const NationalCampaign& c){
    if(c.complete)return {};std::vector<NationalGroup> out;uint8_t entry=c.active_entry;std::set<uint8_t> seen;
    do{require(seen.insert(entry).second,"parallel link cycle");const auto& s=stage(c,entry);const auto& r=s.rules;require(started(s)&&s.played<duration(r),"active field/cursor is invalid");NationalGroup g;g.season=c.season;g.competition=c.competition;g.source_entry=entry;g.played=s.played;g.revision=c.revision;g.flags=r.flags;const auto legs=(r.flags&1)+1;g.round=uint8_t((r.knockout()?s.played/legs:s.played)+1);g.leg=uint8_t(r.knockout()?s.played%legs+1:1);
        if(r.knockout()){if(legs==2&&g.leg==1)g.flags&=0xf7;const auto first=offset(s),count=size_t(r.teams)>>(s.played/legs);for(size_t i=0;i<count;i+=2){g.matches.push_back({s.teams[first+i],s.teams[first+i+1]});if(g.leg==2)g.other_leg.push_back(s.scores[1][(first+i)/2]);}}
        else for(const auto& fixture:bmh::generate_fixtures(std::max(4,int(r.teams)),s.played+1,r.rounds/legs,0))g.matches.push_back({s.teams.at(size_t(fixture.home)),s.teams.at(size_t(fixture.away))});
        out.push_back(std::move(g));entry=r.parallel;
    }while(entry);return out;
}
void validate_national_campaigns(const World& world){
    if(!world.national_campaigns)return;require(world.catalogue.scope.international,"campaigns belong to disabled international scope");const auto all=nations(world);const auto& history=*world.national_campaigns;require(history.primary_country==Country::germany||history.primary_country==Country::england,"primary country is invalid");require(!world.catalogue.scope.club_play()||history.primary_country==Country::germany,"club draw primary country changed");require(history.seasons.size()<=32768&&history.honours.size()<=20,"history capacity exceeded");
    uint32_t expected=0;for(const auto& [season,c]:history.seasons){require(season==expected++&&c.season==season&&c.competition==7-season%4,"history is unordered or has another competition");if(season+1<history.seasons.size())require(c.complete,"a later season started before completion");std::vector<NationalStageRules> rules;for(const auto& s:c.stages)rules.push_back(s.rules);validate_rules(rules,season);
        const size_t sizes[]={4,8,6,24};require(c.entrants.size()==sizes[season%4],"field size disagrees with season");std::set<ClubId> entrants;for(auto id:c.entrants)require(id.valid()&&std::find(all.begin(),all.end(),id)!=all.end()&&entrants.insert(id).second,"field is repeated or not a national identity");
        uint64_t revisions=0;for(const auto& s:c.stages){validate_storage(s,entrants);if(!s.rules.group_index)revisions+=s.played;if(started(s)&&s.rules.previous!=255){const auto& previous=stage(c,s.rules.previous);require(previous.played==duration(previous.rules),"successor started before predecessor completed");}}
        require(c.revision==revisions,"revision disagrees with played history");const auto& active=stage(c,c.active_entry);require(!active.rules.group_index,"active entry is not the primary group");
        for(const auto& s:c.stages)if(!s.rules.knockout()&&!s.rules.group_index){std::set<ClubId> group_ids;for(const auto& other:c.stages)if(!other.rules.knockout()&&other.rules.previous==s.rules.previous&&other.rules.groups==s.rules.groups){require(other.played==s.played&&started(other)==started(s),"parallel group cursor/field differs");if(started(other))for(auto id:other.teams)require(group_ids.insert(id).second,"parallel groups repeat a nation");}}
        if(c.complete){require(active.played==duration(active.rules)&&!active.rules.next&&c.winner==active.winner&&c.winner.valid()&&entrants.contains(c.winner)&&c.runner_up==active.runner_up,"terminal winner/cursor is invalid");if(season%4!=0)require(c.runner_up.valid()&&c.runner_up!=c.winner,"terminal runner-up is missing");}
        else {require(!c.winner.valid()&&!c.runner_up.valid(),"premature campaign awards");require(started(active)&&active.played<duration(active.rules),"active stage is not playable");}
        if(c.complete&&c.competition==4){const auto& final=stage(c,c.active_entry);const auto& third=stage(c,final.rules.parallel);require(c.places==std::array<ClubId,4>{final.winner,final.runner_up,third.winner,third.runner_up},"podium differs from completed finals");}
        else require(std::none_of(c.places.begin(),c.places.end(),[](auto id){return id.valid();}),"premature or non-World-Cup podium");
    }
    std::vector<std::array<ClubId,4>> expected_honours;
    for(auto i=history.seasons.rbegin();i!=history.seasons.rend()&&expected_honours.size()<20;++i) {
        const auto& c=i->second;
        if(c.complete&&c.competition==4){const auto& final=stage(c,c.active_entry);const auto& third=stage(c,final.rules.parallel);require(c.places==std::array<ClubId,4>{final.winner,final.runner_up,third.winner,third.runner_up},"podium differs from completed finals");expected_honours.push_back(c.places);}
        else require(std::none_of(c.places.begin(),c.places.end(),[](auto id){return id.valid();}),"premature or non-World-Cup podium");
    }
    require(history.honours==expected_honours,"Source honours differ from played World-Cup history");
    for(const auto& places:history.honours){std::set<ClubId> unique;for(auto id:places)require(std::find(all.begin(),all.end(),id)!=all.end()&&unique.insert(id).second,"honour podium is incomplete/repeated");}
}
void commit_national_results(World& world,const NationalResults& result,const LeImage& source,WatcomRandom& random){
    world.validate();require(world.national_campaigns&&world.national_campaigns->seasons.contains(result.season),"result has no campaign owner");const auto before=world;const auto old_random=random;const auto& original=world.national_campaigns->seasons.at(result.season);require(!original.complete&&original.revision==result.revision&&result.revision!=std::numeric_limits<uint64_t>::max(),"result is stale or already complete");const auto rules=national_programme_rules(source,result.season);require(rules.size()==original.stages.size(),"Source programme changed");for(size_t i=0;i<rules.size();++i)require(rules[i]==original.stages[i].rules,"Source stage changed");const auto groups=prepare_national_groups(original);require(groups.size()==result.groups.size(),"parallel result batch is incomplete");auto candidate=world;auto& c=candidate.national_campaigns->seasons.at(result.season);Workspace workspace(c,world);auto& m=workspace.game.memory;
    for(size_t i=0;i<groups.size();++i){const auto& plan=groups[i];const auto& got=result.groups[i];require(got.source_entry==plan.source_entry&&got.played==plan.played&&got.scores.size()==plan.matches.size(),"result belongs to another group/round");const auto& s=stage(c,got.source_entry);const auto first=s.rules.knockout()?offset(s)/2:size_t(s.played)*s.rules.teams/2;const auto at=cup_entry_address(got.source_entry);
        for(size_t n=0;n<got.scores.size();++n){const auto score=got.scores[n];require((score>>10)<=2&&((s.rules.flags&8)||!(score>>10)),"score is unplayed or has an invalid phase");m.set16(m.pointer(at+12)+uint32_t((first+n)*2),score);}
        if(plan.flags&8){MatchGroup tie;tie.desc.assign(0xd0,0);tie.desc[1]=plan.flags;tie.cup_results=got.scores;tie.cup_other_leg=plan.other_leg;for(size_t n=0;n<got.scores.size();++n)require(cup_tie(tie,int(n))!=2,"knockout tie is unresolved");}
    }
    auto staged=random;MinuteReport report;bmh::advance_cup(workspace.game,c.competition,staged,report);require(!report.unsupported,"Core programme transition is unsupported");workspace.capture(c);const auto& completed=stage(c,c.active_entry);if(completed.played==duration(completed.rules)){if(completed.rules.next)c.active_entry=completed.rules.next;else {c.complete=true;c.winner=completed.winner;c.runner_up=completed.runner_up;if(c.competition==4){const auto& third=stage(c,completed.rules.parallel);c.places={c.winner,c.runner_up,third.winner,third.runner_up};workspace.capture_honours(*candidate.national_campaigns);}}}
    ++c.revision;candidate.validate();require(world==before&&random.seed()==old_random.seed()&&random.calls()==old_random.calls(),"result/draw ownership changed");world=std::move(candidate);random=staged;
}
}
