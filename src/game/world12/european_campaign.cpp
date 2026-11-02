// SPDX-License-Identifier: GPL-3.0-only
#include "game/world12/european_campaign.hpp"
#include "game/world12/world.hpp"
#include "game/world12/calendar.hpp"
#include "game/cup.hpp"
#include "game/driver.hpp"
#include "game/schedule.hpp"
#include "formats/le.hpp"
#include <algorithm>
#include <limits>
#include <set>
#include <functional>
#include <stdexcept>
namespace bmh::world12 {
namespace {
void require(bool ok,const char* reason){if(!ok)throw FormatError(std::string("world12: European ")+reason);}
size_t index(EuropeanCompetition id){require(uint8_t(id)>=1&&uint8_t(id)<=3,"competition identity is invalid");return size_t(uint8_t(id)-1);}
size_t slots(const EuropeanStageRules& r){if(!r.knockout())return r.teams;size_t count=0;for(int i=0;i<r.rounds;++i)count+=r.teams>>i;return count;}
size_t offset(const EuropeanStage& s){size_t count=0;const int round=s.played/((s.rules.flags&1)+1);for(int i=0;i<round;++i)count+=s.rules.teams>>i;return count;}
EuropeanStage& stage(EuropeanCampaign& c,uint8_t e){auto it=std::find_if(c.stages.begin(),c.stages.end(),[&](const auto& s){return s.rules.entry==e;});require(it!=c.stages.end(),"programme link is missing");return *it;}
const EuropeanStage& stage(const EuropeanCampaign& c,uint8_t e){return stage(const_cast<EuropeanCampaign&>(c),e);}
bool started(const EuropeanStage& s){return !s.teams.empty()&&s.teams.front().valid();}
EuropeanStageRules read_rules(const LeImage& source,int entry,EuropeanCompetition id){
    require(entry>=0&&entry<kCupEntries,"source entry is outside its table");const auto at=cup_entry_address(entry);
    require(source.contains(at+0x27)&&source.u16(at+0x18)==uint8_t(id),"source stage belongs to another competition");
    EuropeanStageRules r;r.entry=uint8_t(entry);r.flags=source.u8(at+0x14);r.previous=source.u8(at+0x15);r.next=source.u8(at+0x16);r.parallel=source.u8(at+0x17);r.teams=source.u8(at+0x1a);r.rounds=source.u8(at+0x1b);r.group_count=source.u8(at+0x1e)&15;r.group_index=source.u8(at+0x1e)>>4;r.quota=source.u8(at+0x1f);r.seeds=source.u8(at+0x20);
    require(r.teams>=2&&r.teams<=64&&r.rounds>=1&&r.rounds<=6&&r.group_count>=1&&r.group_count<=4&&r.group_index<r.group_count,"source dimensions are unsupported");
    if(r.knockout())require((r.teams&(r.teams-1))==0&&(r.teams>>r.rounds)>=1&&r.group_count==1,"knockout dimensions are unsupported");
    else require(r.teams==4&&r.rounds==6&&(r.flags&1)&&r.group_count==4&&r.quota==8,"group programme is unsupported");
    return r;
}
void validate_links(const std::vector<EuropeanStageRules>& rules){
    require(!rules.empty()&&rules.size()<=8,"programme size is invalid");std::map<uint8_t,EuropeanStageRules> entries;uint8_t previous=0;bool first=true;for(const auto& r:rules){require((first||r.entry>previous)&&entries.emplace(r.entry,r).second,"programme entries are not canonical");first=false;previous=r.entry;}
    std::map<uint8_t,int> colour;std::function<void(uint8_t)> visit=[&](uint8_t id){require(entries.contains(id)&&colour[id]!=1,"programme has a missing/cyclic link");if(colour[id]==2)return;colour[id]=1;const auto& r=entries.at(id);for(auto link:{r.next,r.parallel})if(link)visit(link);colour[id]=2;};visit(rules.front().entry);require(colour.size()==rules.size(),"programme contains unreachable stages");
    for(const auto& r:rules){if(r.parallel){const auto& next=entries.at(r.parallel);require(!r.knockout()&&!next.knockout()&&next.group_count==r.group_count&&next.group_index==r.group_index+1&&next.next==r.next,"parallel group metadata is inconsistent");}else if(!r.knockout())require(r.group_index+1==r.group_count,"parallel group chain is incomplete");}
}
void validate_stage_storage(const EuropeanStage& s){
    const auto& r=s.rules;const bool begun=started(s);const size_t legs=(r.flags&1)+1;const size_t finished=r.knockout()?s.played/legs:s.played;
    if(!begun)require(s.played==0&&!s.winner.valid()&&!s.runner_up.valid(),"unstarted stage owns played results/awards");
    if(r.knockout()){
        size_t start=0;for(size_t round=0;round<r.rounds;++round){const size_t count=r.teams>>round;const bool drawn=begun&&round<=finished;std::set<ClubId> unique;
            for(size_t i=0;i<count;++i){const auto club=s.teams.at(start+i);require(club.valid()==drawn&&(!drawn||unique.insert(club).second),"knockout field is incomplete/repeated or drawn early");}
            for(size_t leg=0;leg<2;++leg)for(size_t i=0;i<count/2;++i){const bool played=begun&&(round<finished&&(leg==0||legs==2)||round==finished&&legs==2&&s.played%2&&leg==1);require((s.scores[leg].at(start/2+i)!=0xcccc)==played,"knockout results disagree with the saved leg cursor");}
            start+=count;
        }
    }else {std::set<ClubId> unique;for(auto club:s.teams)require(club.valid()==begun&&(!begun||unique.insert(club).second),"group field is incomplete/repeated");for(size_t i=0;i<s.scores[0].size();++i){require((s.scores[0][i]!=0xcccc)==(i<size_t(s.played)*r.teams/2),"group results disagree with the saved round cursor");require(s.scores[1][i]==0xcccc,"group stage has an unexpected knockout return-leg array");}}
}
struct Workspace {
    GameState game;
    std::vector<ClubId> ids;
    explicit Workspace(const EuropeanCampaign& c,const World& world,uint32_t season):ids(c.entrants){
        game.season=int(std::min(season,49u));
        for(const auto& manager:world.managers)if(std::find(ids.begin(),ids.end(),manager.club)!=ids.end()){require(manager.career&&manager.career->season_records&&manager.career->cup_titles,"manager round/title history is unknown");const auto at=game.manager_records.size();game.manager_teams.push_back(local(manager.club));game.manager_records.resize(at+0xb0c);std::copy(manager.career->season_records->begin(),manager.career->season_records->end(),game.manager_records.begin()+ptrdiff_t(at+0xda));for(size_t i=0;i<2;++i){game.manager_records[at+0x8a6+i*2]=uint8_t((*manager.career->cup_titles)[i]);game.manager_records[at+0x8a7+i*2]=uint8_t((*manager.career->cup_titles)[i]>>8);}}
        auto& m=game.memory;m.add_block(kCupTable,kCupEntries*kCupEntrySize);
        for(int e=0;e<kCupEntries;++e)m.set16(cup_entry_address(e)+0x18,65535);
        m.add_block(layout::Cup_AdvanceCup_RuntimeField,1);m.set8(layout::Cup_AdvanceCup_RuntimeField,1);
        m.add_block(layout::Titles,900);for(uint32_t i=0;i<900;++i)m.set8(layout::Titles+i,255);
        game.club_ids.resize(ids.size());game.club_names={0};game.projected_match_clubs.resize(ids.size());
        uint32_t cursor=0x1000000;
        for(const auto& s:c.stages){const auto& r=s.rules;const auto at=cup_entry_address(r.entry);
            m.set8(at+0x14,r.flags);m.set8(at+0x15,r.previous);m.set8(at+0x16,r.next);m.set8(at+0x17,r.parallel);m.set16(at+0x18,uint8_t(c.competition));m.set8(at+0x1a,r.teams);m.set8(at+0x1b,r.rounds);m.set8(at+0x1d,s.played);m.set8(at+0x1e,uint8_t(r.group_count|(r.group_index<<4)));m.set8(at+0x1f,r.quota);m.set8(at+0x20,r.seeds);
            const auto allocate=[&](size_t words){const auto address=cursor;cursor+=uint32_t(words*2+16);m.add_block(address,words*2+16);return address;};
            const auto list=allocate(r.seeds+slots(r))+uint32_t(r.seeds)*2;m.set_pointer(at,list);
            for(size_t i=0;i<s.teams.size();++i)m.set16(list+uint32_t(i*2),local(s.teams[i]));
            for(size_t i=0;i<s.seeds.size();++i)m.set16(list-uint32_t((i+1)*2),local(s.seeds[i]));
            for(size_t leg=0;leg<2;++leg){const auto results=allocate(s.scores[leg].size());m.set_pointer(at+uint32_t(12+leg*4),leg==1&&(!(r.flags&1)||!r.knockout())?0:results);for(size_t i=0;i<s.scores[leg].size();++i)m.set16(results+uint32_t(i*2),s.scores[leg][i]);}
            if(!r.knockout())m.set_pointer(at+8,layout::CareerSession_RuntimeError_RuntimeField);
            m.set16(at+0x22,local(s.winner));m.set16(at+0x24,local(s.runner_up));
        }
    }
    uint16_t local(ClubId id)const{if(!id.valid())return 65535;const auto it=std::find(ids.begin(),ids.end(),id);require(it!=ids.end(),"stage contains a non-entrant");return uint16_t(it-ids.begin());}
    ClubId global(uint16_t id)const{if(id==65535)return {};require(id<ids.size(),"Core returned an unknown participant");return ids[id];}
    void capture(EuropeanCampaign& c)const{const auto& m=game.memory;for(auto& s:c.stages){const auto at=cup_entry_address(s.rules.entry);s.played=m.u8(at+0x1d);for(size_t i=0;i<s.teams.size();++i)s.teams[i]=global(m.u16(m.pointer(at)+uint32_t(i*2)));for(size_t i=0;i<s.seeds.size();++i)s.seeds[i]=global(m.u16(m.pointer(at)-uint32_t((i+1)*2)));for(size_t leg=0;leg<2;++leg)for(size_t i=0;i<s.scores[leg].size();++i)if(m.pointer(at+uint32_t(12+leg*4)))s.scores[leg][i]=m.u16(m.pointer(at+uint32_t(12+leg*4))+uint32_t(i*2));s.winner=global(m.u16(at+0x22));s.runner_up=global(m.u16(at+0x24));}}
};
}
std::optional<std::vector<EuropeanCompetition>> european_cups_on(const OtherAppointment& appointment){
    if(appointment.domain!=CalendarDomain::shared||!appointment.source_entry||(appointment.source_entry&~0x1c0u))return std::nullopt;std::vector<EuropeanCompetition> result;for(uint8_t k=0;k<3;++k)if(appointment.source_entry&(0x40u<<k))result.push_back(EuropeanCompetition(k+1));return result;
}
std::vector<EuropeanStageRules> european_programme_rules(const LeImage& source,EuropeanCompetition competition){
    index(competition);int first=-1;for(int e=0;e<kCupEntries;++e){const auto at=cup_entry_address(e);require(source.contains(at+0x27),"source table is truncated");if(source.u16(at+0x18)==uint8_t(competition)){first=e;break;}}
    require(first>=0,"source competition is missing");std::vector<uint8_t> pending{uint8_t(first)};std::vector<EuropeanStageRules> result;std::set<uint8_t> seen;
    for(size_t i=0;i<pending.size();++i){const auto e=pending[i];if(!seen.insert(e).second)continue;auto r=read_rules(source,e,competition);result.push_back(r);for(auto linked:{r.next,r.parallel})if(linked)pending.push_back(linked);}
    std::sort(result.begin(),result.end(),[](const auto& a,const auto& b){return a.entry<b.entry;});validate_links(result);return result;
}
EuropeanCampaigns initial_european_campaigns(const World& world,const LeImage& source){
    world.validate();require(world.initial_competitions.has_value()&&world.catalogue.scope.club_play(),"initial fields are unavailable");require(!world.european_campaigns,"campaign owner is already installed");EuropeanCampaigns result;
    for(size_t k=0;k<3;++k){auto& c=result[k];c.competition=EuropeanCompetition(k+1);c.entrants=world.initial_competitions->fields[k];const auto rules=european_programme_rules(source,c.competition);c.active_entry=rules.front().entry;
        for(const auto& r:rules){EuropeanStage s;s.rules=r;s.teams.resize(slots(r));s.seeds.resize(r.seeds);for(auto& scores:s.scores)scores.assign(r.knockout()?slots(r)/2:size_t(r.rounds)*r.teams/2,0xcccc);c.stages.push_back(std::move(s));}
        auto& first=stage(c,c.active_entry);require(c.entrants.size()==size_t(first.rules.teams)+first.rules.seeds,"initial field does not match its Source programme");
        std::copy(c.entrants.begin()+first.rules.seeds,c.entrants.end(),first.teams.begin());for(size_t i=0;i<first.seeds.size();++i)first.seeds[i]=c.entrants[first.seeds.size()-1-i];
    }return result;
}
std::vector<EuropeanGroup> prepare_european_groups(const EuropeanCampaign& c){
    if(c.complete)return {};std::vector<EuropeanGroup> groups;auto entry=c.active_entry;std::set<uint8_t> seen;
    do{require(seen.insert(entry).second,"parallel group cycle");const auto& s=stage(c,entry);const auto& r=s.rules;require(started(s),"active field is missing");const auto legs=(r.flags&1)+1;require(s.played<r.rounds*(r.knockout()?legs:1),"active stage is already complete");
        EuropeanGroup g;g.competition=c.competition;g.source_entry=entry;g.played=s.played;g.revision=c.revision;g.flags=r.flags;g.round=uint8_t((r.knockout()?s.played/legs:s.played)+1);g.leg=uint8_t(r.knockout()?s.played%legs+1:1);
        if(r.knockout()){if(legs==2&&g.leg==1)g.flags&=0xf7;const auto start=offset(s),count=size_t(r.teams)>>(s.played/legs);for(size_t i=0;i<count;i+=2){g.matches.push_back({s.teams.at(start+i),s.teams.at(start+i+1)});if(g.leg==2)g.other_leg.push_back(s.scores[1].at((start+i)/2));}}
        else {const auto fixtures=bmh::generate_fixtures(r.teams,s.played+1,r.rounds/legs,0);for(const auto& f:fixtures)g.matches.push_back({s.teams.at(size_t(f.home)),s.teams.at(size_t(f.away))});}
        groups.push_back(std::move(g));entry=r.parallel;
    }while(entry);return groups;
}
std::vector<ClubId> current_european_members(const EuropeanCampaign& c){
    if(c.complete)return {c.winner};std::vector<ClubId> ids;uint8_t entry=c.active_entry;std::set<uint8_t> seen;
    do{require(seen.insert(entry).second,"parallel membership cycle");const auto& s=stage(c,entry);require(started(s),"current membership has no field");const auto start=s.rules.knockout()?offset(s):0u;const auto count=s.rules.knockout()?size_t(s.rules.teams)>>(s.played/((s.rules.flags&1)+1)):size_t(s.rules.teams);require(start+count<=s.teams.size()&&count>=2,"current membership cursor is past the stage");ids.insert(ids.end(),s.teams.begin()+ptrdiff_t(start),s.teams.begin()+ptrdiff_t(start+count));for(auto id:s.seeds)if(id.valid())ids.push_back(id);entry=s.rules.parallel;}while(entry);return ids;
}
std::optional<EuropeanClubStatus> european_club_status(const World& world,ClubId club){
    require(world.clubs.contains(club),"club identity is missing");if(!world.european_campaigns)return std::nullopt;
    EuropeanClubStatus result;for(size_t k=0;k<3;++k){result.held_titles[k]=world.initial_competitions&&world.initial_competitions->holders[k]==club;result.holder|=result.held_titles[k];}
    for(const auto& c:*world.european_campaigns)if(std::find(c.entrants.begin(),c.entrants.end(),club)!=c.entrants.end()){
        const auto members=current_european_members(c);result.competition=c.competition;result.entered=true;result.active=std::find(members.begin(),members.end(),club)!=members.end();result.eliminated=!result.active;result.winner=c.complete&&c.winner==club;break;}
    return result;
}
void validate_european_campaigns(const World& world){
    if(!world.european_campaigns)return;require(world.initial_competitions.has_value()&&world.catalogue.scope.club_play(),"campaigns have no initial ownership");std::set<ClubId> all;
    for(size_t k=0;k<3;++k){const auto& c=(*world.european_campaigns)[k];require(index(c.competition)==k&&c.entrants==world.initial_competitions->fields[k],"campaign identity/initial field changed");std::set<ClubId> entrants(c.entrants.begin(),c.entrants.end());require(entrants.size()==c.entrants.size(),"entrant repeats");for(auto id:entrants)require(id.valid()&&world.clubs.contains(id)&&world.clubs.at(id).kind==ClubKind::club&&all.insert(id).second,"participant identity is invalid");require(!c.stages.empty()&&c.stages.size()<=8,"stage count is invalid");std::set<uint8_t> entries;
        for(const auto& s:c.stages){const auto& r=s.rules;require(r.entry<kCupEntries&&entries.insert(r.entry).second&&r.teams>=2&&r.teams<=64&&r.rounds>=1&&r.rounds<=6&&r.group_count>=1&&r.group_count<=4&&r.group_index<r.group_count,"stage descriptor is invalid");require(r.knockout()?((r.teams&(r.teams-1))==0&&(r.teams>>r.rounds)>=1&&r.group_count==1):(r.teams==4&&r.rounds==6&&(r.flags&1)&&r.group_count==4&&r.quota==8),"stage dimensions are invalid");require(s.teams.size()==slots(r)&&s.seeds.size()==r.seeds&&s.played<=r.rounds*(r.knockout()?((r.flags&1)+1):1),"stage storage/cursor is invalid");const auto scores=r.knockout()?slots(r)/2:size_t(r.rounds)*r.teams/2;for(const auto& leg:s.scores)require(leg.size()==scores,"score storage is invalid");for(const auto& leg:s.scores)for(auto word:leg)require(word==0xcccc||(word>>10)<=2,"stored score phase is invalid");for(auto id:s.teams)require(!id.valid()||entrants.contains(id),"stage refers outside its field");for(auto id:s.seeds)require(!id.valid()||entrants.contains(id),"seed refers outside its field");for(auto id:{s.winner,s.runner_up})require(!id.valid()||entrants.contains(id),"stage award is invalid");
        }
        std::vector<EuropeanStageRules> rules;for(const auto& s:c.stages){rules.push_back(s.rules);validate_stage_storage(s);}validate_links(rules);
        uint64_t revisions=0;for(const auto& s:c.stages){const auto& r=s.rules;if(!r.group_index)revisions+=s.played;
            if(started(s)&&r.previous!=255){const auto& before=stage(c,r.previous);require(before.played==before.rules.rounds*(before.rules.knockout()?((before.rules.flags&1)+1):1),"successor started before its predecessor completed");}
            if(!r.knockout()&&!r.group_index){std::set<ClubId> group_clubs;for(const auto& other:c.stages)if(!other.rules.knockout()&&other.rules.previous==r.previous){require(other.played==s.played&&started(other)==started(s),"parallel group cursors/fields disagree");if(started(other))for(auto id:other.teams)require(group_clubs.insert(id).second,"parallel groups repeat a club");}}
        }require(revisions==c.revision,"campaign revision disagrees with its complete Source leg history");
        for(const auto& s:c.stages){for(auto link:{s.rules.next,s.rules.parallel})if(link)require(entries.contains(link),"stage link is missing");}
        require(entries.contains(c.active_entry)&&!stage(c,c.active_entry).rules.group_index,"active primary stage is absent/parallel");if(c.complete){const auto& final=stage(c,c.active_entry);require(c.winner.valid()&&c.runner_up.valid()&&c.winner!=c.runner_up&&entrants.contains(c.winner)&&entrants.contains(c.runner_up)&&!final.rules.next&&final.played==final.rules.rounds*((final.rules.flags&1)+1)&&final.winner==c.winner&&final.runner_up==c.runner_up,"terminal award is invalid or differs from the completed final");}else {require(!c.winner.valid()&&!c.runner_up.valid(),"premature winner");std::set<ClubId> active;for(auto id:current_european_members(c))require(id.valid()&&entrants.contains(id)&&active.insert(id).second,"current field repeats or is missing");}
    }
}
void commit_european_results(World& world,const EuropeanResults& result,const LeImage& source,WatcomRandom& random,uint32_t season){
    world.validate();const auto before=world;const auto seed=random.seed();const auto calls=random.calls();require(world.european_campaigns.has_value(),"campaign owner is unknown");const auto k=index(result.competition);const auto& original=(*world.european_campaigns)[k];require(original.revision==result.revision&&result.revision!=std::numeric_limits<uint64_t>::max()&&!original.complete,"result owner is stale/complete");const auto rules=european_programme_rules(source,result.competition);require(rules.size()==original.stages.size(),"source programme changed");for(size_t i=0;i<rules.size();++i)require(rules[i]==original.stages[i].rules,"source stage changed");const auto groups=prepare_european_groups(original);require(result.groups.size()==groups.size(),"parallel result batch is incomplete");auto candidate=world;auto& c=(*candidate.european_campaigns)[k];Workspace workspace(c,world,season);auto& m=workspace.game.memory;
    for(size_t i=0;i<groups.size();++i){const auto& expected=groups[i];const auto& got=result.groups[i];require(got.source_entry==expected.source_entry&&got.played==expected.played&&got.scores.size()==expected.matches.size(),"result batch belongs to another leg");const auto& s=stage(c,got.source_entry);const auto start=s.rules.knockout()?offset(s)/2:size_t(s.played)*s.rules.teams/2;const auto at=cup_entry_address(got.source_entry);for(size_t j=0;j<got.scores.size();++j){require((got.scores[j]>>10)<=2&&(!(expected.leg==1&&(s.rules.flags&1))||!(got.scores[j]>>10))&& (s.rules.knockout()||!(got.scores[j]>>10)),"match score is unplayed or has an invalid phase");m.set16(m.pointer(at+12)+uint32_t((start+j)*2),got.scores[j]);}
        if(s.rules.knockout()&&(expected.flags&8)){MatchGroup tie;tie.desc.assign(0xd0,0);tie.desc[1]=expected.flags;tie.cup_results=got.scores;tie.cup_other_leg=expected.other_leg;for(size_t j=0;j<got.scores.size();++j)require(cup_tie(tie,int(j))!=2,"knockout tie is unresolved");}
    }
    auto staged_random=random;MinuteReport report;bmh::advance_cup(workspace.game,uint8_t(c.competition),staged_random,report);require(!report.unsupported,"Core stage transition is unsupported");workspace.capture(c);const auto& finished=stage(c,c.active_entry);const auto rounds=finished.rules.rounds*(finished.rules.knockout()?((finished.rules.flags&1)+1):1);if(finished.played==rounds){if(finished.rules.next)c.active_entry=finished.rules.next;else {c.complete=true;c.winner=finished.winner;c.runner_up=finished.runner_up;}}
    for(size_t i=0;i<workspace.game.manager_teams.size();++i){const auto id=workspace.global(uint16_t(workspace.game.manager_teams[i]));auto& manager=*std::find_if(candidate.managers.begin(),candidate.managers.end(),[&](const auto& manager){return manager.club==id;});const auto at=i*0xb0c;std::copy_n(workspace.game.manager_records.begin()+ptrdiff_t(at+0xda),150,manager.career->season_records->begin());for(size_t word=0;word<2;++word)(*manager.career->cup_titles)[word]=uint16_t(workspace.game.manager_records[at+0x8a6+word*2]|uint16_t(workspace.game.manager_records[at+0x8a7+word*2])<<8);}
    ++c.revision;candidate.validate();require(world==before&&random.seed()==seed&&random.calls()==calls,"world/random ownership changed during the Source draw");world=std::move(candidate);random=staged_random;
}
}
