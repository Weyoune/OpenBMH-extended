// SPDX-License-Identifier: GPL-3.0-only
#include "game/world12/manager_creation.hpp"
#include "game/world12/manager_tactics.hpp"
#include "game/career.hpp"
#include "formats/le.hpp"
#include <algorithm>
#include <bit>
#include <stdexcept>
namespace bmh::world12 {
namespace {
constexpr size_t record_size=0xb0c;
size_t record(const GameState& state,uint8_t seat) {
    if(seat>3||state.manager_records.size()<size_t(seat+1)*record_size)throw std::invalid_argument("world12: missing manager career record");
    return size_t(seat)*record_size;
}
uint32_t word32(const Bytes& data,size_t at){uint32_t n=0;for(size_t i=0;i<4;++i)n|=uint32_t(data.at(at+i))<<(i*8);return n;}
void put32(Bytes& data,size_t at,uint32_t n){for(size_t i=0;i<4;++i)data.at(at+i)=uint8_t(n>>(i*8));}
}
ManagerCareerState manager_career_state_from_legacy(const GameState& state,uint8_t seat,const std::vector<PlayerId>& players) {
    const auto at=record(state,seat);ManagerCareerState result;
    const auto copy=[&](size_t offset,auto& output){std::copy_n(state.manager_records.begin()+at+offset,output.size(),output.begin());};
    copy(0,result.stadium.facility_values);copy(0x16,result.stadium.stand_condition);copy(0x3c,result.stadium.facility_levels);copy(0x4c,result.stadium.stand_capacity);
    result.stadium.capacity_sum=state.manager_records.at(at+0x5c);
    result.board_patience=state.manager_records.at(at+0x26f);result.training_plan=state.manager_records.at(at+0x26e);
    result.season_records.emplace();copy(0xda,*result.season_records);
    copy(0x1f6,result.transfer_filters);
    for(size_t i=0;i<5;++i){const auto offset=at+0x8b6+i*2;const auto id=uint16_t(state.manager_records[offset]|uint16_t(state.manager_records[offset+1])<<8);
        if(id==0xffff)continue;if(id>=players.size()||!players[id].valid())throw std::invalid_argument("world12: missing mapped poaching attempt");result.poach_attempts[i]=players[id];}
    const auto youth=layout::EnvironmentYouthTeams+uint32_t(seat)*0x24;
    for(size_t i=0;i<3;++i)result.environment.youth[i]={state.memory.u8(youth+uint32_t(i)),state.memory.u8(youth+3+uint32_t(i)),state.memory.u8(youth+6+uint32_t(i))};
    for(size_t i=0;i<6;++i)result.environment.monthly[i]=std::bit_cast<int32_t>(state.memory.u32(layout::Spending+uint32_t(seat)*0x24+uint32_t(i)*4));
    const size_t sponsors[]={0xae8,0xadc,0xa34};
    for(size_t i=0;i<3;++i)result.sponsors[i]={word32(state.manager_records,at+sponsors[i]),state.manager_records.at(at+sponsors[i]+5)};
    return result;
}
void install_manager_career_state(GameState& state,uint8_t seat,const ManagerCareerState& input,const std::vector<PlayerId>& players) {
    const auto at=record(state,seat);std::array<uint16_t,5> attempts;attempts.fill(0xffff);
    for(size_t i=0;i<5;++i)if(input.poach_attempts[i].valid()){const auto found=std::find(players.begin(),players.end(),input.poach_attempts[i]);if(found==players.end()||found-players.begin()>=4095)throw std::invalid_argument("world12: unmapped poaching attempt");attempts[i]=uint16_t(found-players.begin());}
    const auto copy=[&](size_t offset,const auto& values){std::copy(values.begin(),values.end(),state.manager_records.begin()+at+offset);};
    copy(0,input.stadium.facility_values);copy(0x16,input.stadium.stand_condition);copy(0x3c,input.stadium.facility_levels);copy(0x4c,input.stadium.stand_capacity);
    state.manager_records.at(at+0x5c)=input.stadium.capacity_sum;
    state.manager_records.at(at+0x26f)=input.board_patience;state.manager_records.at(at+0x26e)=input.training_plan;
    if(input.season_records)copy(0xda,*input.season_records);
    copy(0x1f6,input.transfer_filters);for(size_t i=0;i<5;++i){state.manager_records.at(at+0x8b6+i*2)=uint8_t(attempts[i]);state.manager_records.at(at+0x8b7+i*2)=uint8_t(attempts[i]>>8);}
    const auto base=layout::EnvironmentYouthTeams;
    state.memory.add_block(base,4*0x24);const auto youth=base+uint32_t(seat)*0x24;
    if(!state.memory.has(youth+0x23))throw std::invalid_argument("world12: incomplete manager environment storage");
    for(size_t i=0;i<3;++i){const auto& value=input.environment.youth[i];state.memory.set8(youth+uint32_t(i),value.strength);state.memory.set8(youth+3+uint32_t(i),value.position);state.memory.set8(youth+6+uint32_t(i),value.league);}
    for(size_t i=0;i<6;++i)state.memory.set32(layout::Spending+uint32_t(seat)*0x24+uint32_t(i)*4,uint32_t(input.environment.monthly[i]));
    const size_t sponsors[]={0xae8,0xadc,0xa34};
    for(size_t i=0;i<3;++i){put32(state.manager_records,at+sponsors[i],input.sponsors[i].amount);state.manager_records.at(at+sponsors[i]+5)=input.sponsors[i].months;}
}
PlayerInitializationOutcome add_initial_manager(World& world,const InitialManagerChoice& choice,CareerSettings settings,
    const InitialStrengthRanges& strengths,const GameState& reference,const LeImage& country_rules,GameRandom& game,WatcomRandom& random) {
    world.validate();
    if(!world.free_players || choice.seat!=world.managers.size() || choice.seat>=settings.planned_managers ||
        settings.mode<1||settings.mode>3||settings.difficulty>9||settings.duration>5||settings.planned_managers<1||settings.planned_managers>4||
        (world.settings && *world.settings!=settings) || choice.name.empty()||choice.name.size()>20||choice.portrait>11||choice.strength>2||!world.loans.empty()||world.development_balance||world.trend_rotation)
        throw std::invalid_argument("world12: invalid fresh manager confirmation");
    for(size_t i=0;i<world.managers.size();++i){const auto& m=world.managers[i];if(m.seat!=i||m.club==choice.club||m.portrait==choice.portrait||!m.career||!m.finance||!m.tactics||!m.match_state)throw std::invalid_argument("world12: conflicting or incomplete preceding manager");}
    const auto selected=world.clubs.find(choice.club);
    if(selected==world.clubs.end()||selected->second.kind!=ClubKind::club||!world.catalogue.selectable(selected->second.country))throw std::invalid_argument("world12: unavailable manager club/country");
    const auto& target=world.catalogue.league(choice.start_league);const auto country=selected->second.country;
    if(!target.active||!target.playable||target.country!=country)throw std::invalid_argument("world12: manager placement crosses country/scope");
    for(const auto& [id,c]:world.clubs)if(c.season_stats!=ClubSeasonStats{})throw std::invalid_argument("world12: initial manager cannot enter played career");
    const auto before=world;auto candidate=world;candidate.settings=settings;auto staged_game=game;auto staged_random=random;
    const auto g_seed=game.seed();const auto w_seed=random.seed();const auto g_calls=game.calls(),w_calls=random.calls();
    const auto owned=[&](ClubId id){return id==choice.club||std::any_of(candidate.managers.begin(),candidate.managers.end(),[&](const auto& m){return m.club==id;});};
    const auto reserved=[&](LeagueId league,size_t rank){size_t ordinal=rank;for(uint8_t i=0;i<candidate.catalogue.league(league).local_index;++i)ordinal+=candidate.catalogue.league(candidate.catalogue.starting_league(country,i)).clubs;
        const auto address=layout::CompetitionCalendar+uint32_t(ordinal*2);if(!country_rules.contains(address+1))throw std::invalid_argument("world12: missing country manager-placement calendar");return (country_rules.u16(address)&0x4001)==0x4001;};
    auto local=reference;local.manager_teams.clear();local.squad_slots.assign(24,kEmptySlot);local.players.resize(24);
    const auto strength=[&](ClubId id,int mode){auto& club=candidate.clubs.at(id);if(!club.league.valid())return;const auto range=strengths.at(club.league.value);
        std::vector<PlayerId> mapping;std::fill(local.squad_slots.begin(),local.squad_slots.end(),kEmptySlot);
        for(const auto& slot:club.roster)if(!slot.empty()){const auto p=mapping.size();mapping.push_back(slot.player);local.players[p]=candidate.players.at(slot.player).attributes;local.squad_slots[p]=uint16_t(p);}
        bmh::initialize_career_club_strength(local,0,mode,{range[0],range[1]},staged_random);
        for(size_t i=0;i<mapping.size();++i)candidate.players.at(mapping[i]).attributes=local.players[i];};
    const auto exchange=[&](ClubId other){auto& club=candidate.clubs.at(choice.club);auto& donor=candidate.clubs.at(other);const auto old=club.league;const auto next=donor.league;
        auto& destination=candidate.membership[next.value].clubs;
        if(old==next)std::iter_swap(std::find(destination.begin(),destination.end(),other),std::find(destination.begin(),destination.end(),choice.club));
        else if(old.valid()){auto& previous=candidate.membership[old.value].clubs;*std::find(previous.begin(),previous.end(),choice.club)=other;*std::find(destination.begin(),destination.end(),other)=choice.club;}
        else {club.roster=donor.roster;donor.roster={};*std::find(destination.begin(),destination.end(),other)=choice.club;}
        club.league=next;donor.league=old;};
    const auto first_league=candidate.catalogue.starting_league(country,0);auto& first=candidate.membership[first_league.value].clubs;
    size_t place=(settings.duration<=1?1:9)+choice.seat;
    while(place<first.size()&&(reserved(first_league,place)||owned(first[place])))++place;
    if(place==first.size())throw std::invalid_argument("world12: no unreserved first-league manager place");
    const auto old_league=candidate.clubs.at(choice.club).league;
    if(old_league!=choice.start_league) {
        auto& members=candidate.membership[choice.start_league.value].clubs;size_t position=place;
        if(target.local_index!=0){const auto count=std::min<size_t>(13,members.size());bool room=false;
            for(size_t i=0;i<count;++i)room|=!reserved(choice.start_league,i)&&!owned(members[i]);
            if(!room)throw std::invalid_argument("world12: no unreserved lower-league manager place");
            do {position=size_t(staged_game.range(0,int(count)-1));if(position>=count)throw std::invalid_argument("world12: invalid manager placement draw");}while(owned(members[position]));
            for(size_t i=0;i<count;++i){const auto rank=(position+i)%count;if(!reserved(choice.start_league,rank)&&!owned(members[rank])){position=rank;break;}}
        }
        const auto donor=members.at(position);exchange(donor);if(old_league.valid())strength(donor,0);
    }else if(target.local_index==0)exchange(first.at(place));
    strength(choice.club,int(choice.strength)+1);
    const auto rec=size_t(choice.seat)*record_size;
    if(local.manager_records.size()<5*record_size || local.manager_options.size()<size_t(choice.seat+1)*5)throw std::invalid_argument("world12: incomplete manager startup reference");
    for(size_t i=0;i<18;++i)local.manager_records.at(rec+0x1f6+i)=1;local.manager_records.at(rec+0x1f6)=0x40;
    std::fill_n(local.manager_records.begin()+rec+0x8b6,10,uint8_t(255));local.manager_records.at(rec+0x210)=target.local_index;
    const auto put=[&](uint32_t address,uint8_t value){local.memory.add_block(address,1);local.memory.set8(address,value);};
    put(layout::GameMode,settings.mode);put(layout::Difficulty,settings.difficulty);put(layout::CareerDuration,settings.duration);local.date=0;
    local.memory.add_block(layout::Career_CareerAddManager_RuntimeField2,4*0x24);
    bmh::initialize_career_manager_facilities(local,country_rules,choice.seat,target.local_index,settings.duration,staged_random);
    Manager manager{choice.seat,choice.name,choice.club,choice.start_league,choice.portrait};manager.career=manager_career_state_from_legacy(local,choice.seat);candidate.managers.push_back(std::move(manager));
    bmh::world12::initialize_manager_contracts(candidate,choice.seat,local,{int(settings.mode),false,false,settings.foreign_rule,int(settings.difficulty)},staged_random);
    candidate.validate();
    if(world!=before||game.seed()!=g_seed||game.calls()!=g_calls||random.seed()!=w_seed||random.calls()!=w_calls)throw std::invalid_argument("world12: initial manager owner or RNG changed");
    const PlayerInitializationOutcome outcome{staged_game.calls()-g_calls,staged_random.calls()-w_calls,size_t(std::count_if(candidate.clubs.at(choice.club).roster.begin(),candidate.clubs.at(choice.club).roster.end(),[](const auto& row){return !row.empty();}))};
    world=std::move(candidate);game=staged_game;random=staged_random;return outcome;
}
}
