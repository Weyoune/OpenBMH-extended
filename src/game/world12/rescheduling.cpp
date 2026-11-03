// SPDX-License-Identifier: GPL-3.0-only
#include "game/world12/rescheduling.hpp"
#include <algorithm>
#include <cstdlib>
#include <tuple>
#include <set>
#include <limits>
namespace bmh::world12 {
namespace {
const Manager& actor(const CareerSnapshot& state,uint8_t seat){
    const auto it=std::find_if(state.world.managers.begin(),state.world.managers.end(),[&](const auto& m){return m.seat==seat;});
    if(it==state.world.managers.end()||!it->career||!state.calendar||!state.world.catalogue.scope.club_play())throw FormatError("world12: fixture move has no club calendar owner");return *it;
}
bool same_fixture(const FixtureReschedule& a,const FixtureReschedule& b){return a.league==b.league&&a.round==b.round&&a.match==b.match;}
Fixture fixture(const CareerSnapshot& s,const FixtureReschedule& m){return league_fixtures(s.world,m.league,m.round).at(m.match);}
bool contains(Fixture f,ClubId c){return f.home==c||f.away==c;}
bool conflict(const CareerSnapshot& s,const FixtureReschedule& own,uint16_t day,bool friday){
    const auto f=fixture(s,own);const auto country=s.world.clubs.at(f.home).country;
    for(const auto& a:s.calendar->schedule.leagues){if(std::abs(int(a.day)-int(day))>1)continue;if(friday&&a.day==own.source_day&&a.league==own.league&&a.round==own.round)continue;
        if(a.league!=s.world.clubs.at(f.home).league&&a.league!=s.world.clubs.at(f.away).league)continue;
        const auto matches=league_fixtures(s.world,a.league,a.round);
        for(size_t i=0;i<matches.size();++i)if(contains(matches[i],f.home)||contains(matches[i],f.away)){
            const auto moved=std::find_if(s.reschedules.begin(),s.reschedules.end(),[&](const auto& m){return m.league==a.league&&m.round==a.round&&m.match==i;});
            if(moved==s.reschedules.end())return true;
        }
    }
    for(const auto& a:s.calendar->schedule.others)if(std::abs(int(a.day)-int(day))<=1){
        if(a.domain==CalendarDomain::shared||(a.domain==CalendarDomain::germany)==(country==Country::germany))return true;
    }
    for(const auto& m:s.reschedules)if(!same_fixture(m,own)&&std::abs(int(m.target_day)-int(day))<=1){const auto other=fixture(s,m);if(contains(other,f.home)||contains(other,f.away))return true;}
    if(s.friendly_plan)for(const auto& b:s.friendly_plan->pending)if(std::abs(int(b.day)-int(day))<=1&&(b.national||contains(b.fixture,f.home)||contains(b.fixture,f.away)))return true;
    return false;
}
}
std::vector<Fixture> selected_league_fixtures(const World& world,LeagueId league,int round,const std::vector<uint8_t>& indices){
    const auto all=league_fixtures(world,league,round);if(indices.empty())return all;std::set<uint8_t> seen;std::vector<Fixture> out;
    for(auto index:indices){if(index>=all.size()||!seen.insert(index).second)throw FormatError("world12: invalid/repeated original fixture index");out.push_back(all[index]);}return out;
}
bool replay_on(const CareerSnapshot& s,uint16_t day){return std::any_of(s.reschedules.begin(),s.reschedules.end(),[&](const auto& m){return !m.played&&m.target_day==day;});}
std::vector<FixtureReschedule> reschedulable_fixtures(const CareerSnapshot& s,uint8_t seat){
    const auto& m=actor(s,seat);const auto& club=s.world.clubs.at(m.club);const auto& league=s.world.catalogue.league(club.league);
    const auto visible=s.world.catalogue.player_leagues(club.country);if(!league.playable||std::find(visible.begin(),visible.end(),league.id)==visible.end())return {};std::vector<FixtureReschedule> out;
    for(const auto& a:s.calendar->schedule.leagues)if(a.league==club.league&&a.day>s.calendar->next_day&&a.round>1&&a.round+2<=league.rounds()){
        const auto games=league_fixtures(s.world,a.league,a.round);for(size_t i=0;i<games.size();++i){FixtureReschedule move{a.league,a.round,uint8_t(i),a.day,0,0,false};
            if(games[i].home==m.club&&!s.results.result(s.world.catalogue,a.league,a.round,int(i)).played&&std::none_of(s.reschedules.begin(),s.reschedules.end(),[&](const auto& old){return same_fixture(old,move);}))out.push_back(move);
        }
    }return out;
}
std::vector<uint16_t> reschedule_dates(const CareerSnapshot& s,uint8_t seat,const FixtureReschedule& move){
    const auto& manager=actor(s,seat);const auto available=reschedulable_fixtures(s,seat);
    if(std::find(available.begin(),available.end(),move)==available.end())return {};std::vector<uint16_t> out;
    for(uint16_t day=uint16_t(s.calendar->next_day+1);day<s.calendar->schedule.days&&day<=286;++day){
        if(day<20||day>=150&&day<=200||day==move.source_day)continue;
        const bool friday=manager.career->stadium.facility_values[14]!=0&&day+1==move.source_day;
        if(day+1>286&&!friday)continue;
        if(conflict(s,move,day,friday))continue;
        const auto count=std::count_if(s.reschedules.begin(),s.reschedules.end(),[&](const auto& m){return !m.played&&m.target_day==day;});
        if(count<8)out.push_back(day);
    }return out;
}
void move_fixture(CareerSnapshot& s,uint8_t seat,const FixtureReschedule& move,uint16_t target){
    s.validate();const auto dates=reschedule_dates(s,seat,move);if(std::find(dates.begin(),dates.end(),target)==dates.end()||s.calendar->revision==UINT64_MAX)throw FormatError("world12: unavailable fixture move date");
    std::array<bool,40> slots{};for(const auto& old:s.reschedules)if(!old.played)slots.at(old.slot)=true;
    const auto free=std::find(slots.begin(),slots.end(),false);if(free==slots.end())throw FormatError("world12: all forty postponed fixture slots are occupied");
    auto next=s;auto accepted=move;accepted.target_day=target;accepted.slot=uint8_t(free-slots.begin());next.reschedules.push_back(accepted);++next.calendar->revision;
    if(next.prepared_event)next.prepared_event->calendar_revision=next.calendar->revision;next.validate();s=std::move(next);
}
void undo_fixture_move(CareerSnapshot& s,uint8_t seat,const FixtureReschedule& move){
    s.validate();const auto club=actor(s,seat).club;const auto it=std::find(s.reschedules.begin(),s.reschedules.end(),move);
    if(it==s.reschedules.end()||it->played||it->target_day<=s.calendar->next_day||it->source_day<=s.calendar->next_day||!contains(fixture(s,*it),club)||s.calendar->revision==UINT64_MAX)throw FormatError("world12: postponed fixture cannot be restored");
    auto next=s;next.reschedules.erase(next.reschedules.begin()+(it-s.reschedules.begin()));++next.calendar->revision;if(next.prepared_event)next.prepared_event->calendar_revision=next.calendar->revision;next.validate();s=std::move(next);
}
void validate_fixture_moves(const CareerSnapshot& s){
    if(s.reschedules.empty())return;if(!s.calendar||s.reschedules.size()>8192)throw FormatError("world12: fixture moves lack their season calendar");
    std::set<std::tuple<LeagueId,uint8_t,uint8_t>> fixtures;std::set<uint8_t> slots;std::map<uint16_t,int> dates;std::map<uint16_t,std::set<ClubId>> clubs;
    for(const auto& m:s.reschedules){if(!m.league.valid()||m.league.value>=kLeagueSlots||m.round<2||m.round+2>s.world.catalogue.league(m.league).rounds()||m.match>=s.world.catalogue.league(m.league).clubs/2||m.slot>=40||m.target_day>=s.calendar->schedule.days||m.source_day>=s.calendar->schedule.days||m.target_day==m.source_day||m.target_day<20||m.target_day>286||(m.target_day>=150&&m.target_day<=200)||!fixtures.emplace(m.league,m.round,m.match).second)throw FormatError("world12: invalid or repeated postponed fixture identity/date");
        const auto original=std::find(s.calendar->schedule.leagues.begin(),s.calendar->schedule.leagues.end(),LeagueAppointment{m.source_day,m.league,m.round});if(original==s.calendar->schedule.leagues.end())throw FormatError("world12: postponed fixture lost its original appointment");
        if(m.played!=(m.target_day<s.calendar->next_day)||s.results.result(s.world.catalogue,m.league,m.round,m.match).played!=m.played)throw FormatError("world12: postponed score/date receipt mismatch");
        if(!m.played&&(!slots.insert(m.slot).second||++dates[m.target_day]>8))throw FormatError("world12: postponed pending slot/day capacity exceeded");
        const auto f=fixture(s,m);const auto visible=s.world.catalogue.player_leagues(s.world.clubs.at(f.home).country);if(std::find(visible.begin(),visible.end(),m.league)==visible.end()||conflict(s,m,m.target_day,m.target_day+1==m.source_day))throw FormatError("world12: postponed fixture collides with its country/club programme");for(auto club:{f.home,f.away})if(!clubs[m.target_day].insert(club).second)throw FormatError("world12: postponed fixtures repeat a club on one day");
    }
}
int visible_league_round(const CareerSnapshot& s,LeagueId league){int round=s.league_rounds.at(league.value);const auto& l=s.world.catalogue.league(league);for(int r=round+1;r<=l.rounds();++r)for(int m=0;m<l.clubs/2;++m)if(s.results.result(s.world.catalogue,league,r,m).played)round=r;return round;}
}
