// SPDX-License-Identifier: GPL-3.0-only
#include "game/world12/national.hpp"
#include "game/world12/manager_tactics.hpp"
#include "game/world12/manager_match_state.hpp"
#include "game/national.hpp"
#include "game/national_selection.hpp"
#include "game/career.hpp"
#include "game/lineup.hpp"
#include <algorithm>
#include <set>

namespace bmh::world12 {
namespace {
constexpr size_t record_size=0xb0c,board_size=0x840;
uint16_t native_flags(uint16_t value) {
    return uint16_t(((value&1)?0x8000:0)|((value&2)?0x4000:0)|
                    ((value&4)?0x2000:0)|((value&8)?0x1000:0));
}
uint16_t typed_flags(uint16_t value) {
    return uint16_t(((value&0x8000)?1:0)|((value&0x4000)?2:0)|
                    ((value&0x2000)?4:0)|((value&0x1000)?8:0));
}
ClubId national_team(const World& world,Country country) {
    ClubId result;
    for(const auto& [id,club]:world.clubs)
        if(club.kind==ClubKind::national_team&&club.country==country) {
            if(result.valid())throw FormatError("world12: ambiguous national team identity");
            result=id;
        }
    if(!result.valid())throw FormatError("world12: missing national team identity");
    return result;
}
}

std::array<NationalCandidate,20> national_nomination(const World& world,Country country) {
    world.validate();
    if(!world.catalogue.scope.international||(country!=Country::germany&&country!=Country::england))
        throw FormatError("world12: unavailable national nomination country");
    (void)national_team(world,country);
    std::vector<NationalCandidate> candidates;
    NationalSquadSelection selection;
    const auto scan=[&](const ClubRecord& club,bool abroad){
        if((club.country!=country)!=abroad)return;
        for(size_t row=0;row<club.roster.size();++row){const auto& slot=club.roster[row];if(slot.empty())continue;const auto& person=world.players.at(slot.player);
            if(person.attributes.nationality()+((person.bonus&128)?32:0)!=int(country)||!national_nomination_available(person.attributes))continue;
            const auto token=uint32_t(candidates.size());candidates.push_back({slot.player,club.id,uint8_t(row),slot.flags});selection.consider(person.attributes.position(),national_nomination_strength(person.attributes,abroad),token);}
    };
    // Original league/row order remains authoritative even when that league
    // has no club simulation. Retained provenance is not an active club entity.
    const auto catalogue=Catalogue::dual_classic();
    for(bool abroad:{false,true})for(auto group:{TableGroup::germany,TableGroup::england,TableGroup::europe})for(auto league:catalogue.simulation_leagues(group)){
        const auto& current=world.membership[league.value];
        if(!current.clubs.empty())for(auto id:current.clubs)scan(world.clubs.at(id),abroad);
        else if(world.national_origins){std::vector<const NationalRosterOrigin*> origins;for(const auto& origin:*world.national_origins)if(origin.club.league==league)origins.push_back(&origin);std::sort(origins.begin(),origins.end(),[](auto a,auto b){return a->rank<b->rank;});for(auto origin:origins)scan(origin->club,abroad);}
    }
    if(!selection.complete())throw FormatError("world12: insufficient available players for national positional quotas");
    std::array<NationalCandidate,20> result;size_t out=0;
    for(size_t position=0;position<4;++position)
        for(size_t rank=0;rank<NationalSquadSelection::quotas[position];++rank)
            result[out++]=candidates.at(selection.token(position,rank));
    return result;
}

void validate_national_squads(const World& world) {
    if(!world.national_squads.empty()&&(!world.catalogue.scope.international||!world.settings||!world.players_initialized))
        throw std::invalid_argument("world12: national squads lack an active career");
    std::set<ClubId> teams;std::set<uint8_t> coaches;
    for(const auto& squad:world.national_squads) {
        const auto found=world.clubs.find(squad.team);
        if(found==world.clubs.end()||found->second.kind!=ClubKind::national_team||
           (found->second.country!=Country::germany&&found->second.country!=Country::england)||
           !teams.insert(squad.team).second||!squad.tactics.backing)
            throw std::invalid_argument("world12: invalid national squad identity/backing");
        if(squad.coach) {
            const auto manager=std::find_if(world.managers.begin(),world.managers.end(),[&](const auto& m){return m.seat==*squad.coach;});
            if(manager==world.managers.end()||world.manager_country(*squad.coach)!=found->second.country||!coaches.insert(*squad.coach).second)
                throw std::invalid_argument("world12: invalid national coach identity/country");
        }
        std::set<PlayerId> players;bool empty=false;size_t occupied=0;
        for(const auto& row:squad.roster) {
            if(row.empty()){if(row.flags)throw std::invalid_argument("world12: flags on empty national slot");empty=true;continue;}
            const auto person=world.players.find(row.player);
            if(empty||row.flags&~15u||person==world.players.end()||!players.insert(row.player).second||
               person->second.attributes.nationality()+((person->second.bonus&128)?32:0)!=int(found->second.country))
                throw std::invalid_argument("world12: invalid national roster reference");
            ++occupied;
        }
        if(occupied!=20)throw std::invalid_argument("world12: national nomination requires twenty players");
        validate_manager_tactics(squad.tactics);
        validate_manager_attendance(world,squad.match_state);
        for(const auto* refs:{&squad.tactics.backing->marks,&squad.tactics.backing->marked_by})
            for(auto player:*refs)if(player.valid()&&!world.players.contains(player))
                throw std::invalid_argument("world12: missing national tactical reference");
        bool ended=false;
        for(size_t i=0;i<12;i+=2) {
            const auto a=squad.match_state.recent_fixtures[i],b=squad.match_state.recent_fixtures[i+1];
            if(a.valid()!=b.valid()||(a.valid()&&(ended||a==b||!world.clubs.contains(a)||!world.clubs.contains(b))))
                throw std::invalid_argument("world12: invalid national match history reference");
            if(!a.valid())ended=true;
        }
        for(const auto& [club,scores]:squad.match_state.head_to_head)
            if(!world.clubs.contains(club))throw std::invalid_argument("world12: missing national head-to-head opponent");
        for(auto player:squad.match_state.takers)if(player.valid()&&(!squad.match_state.designated_takers||!world.players.contains(player)))
            throw std::invalid_argument("world12: missing national designated player");
    }
}

ManagerTactics initial_national_tactics(const GameState& reference,bool coached){
    if(reference.manager_records.size()<5*record_size||reference.manager_options.size()<5||reference.formations.size()<0x580)throw FormatError("world12: missing Source national preference template");auto local=reference;if(coached)initialize_career_manager_formations(local,0);ManagerTactics result;auto& b=result.backing.emplace();std::copy_n(reference.formations.begin(),0x160,b.formations.begin());std::copy_n(reference.formations.begin()+0x160,13,b.formation_tail.begin());b.enabled_formations={local.manager_records[coached?0x218:4*record_size+0x218],local.manager_records[4*record_size+0x219]};result.automatic_formation=reference.manager_options[0];std::copy_n(reference.manager_options.begin()+1,4,b.automation.begin());return result;
}
NationalNominationOutcome nominate_national_squad(World& world,Country country,uint32_t date,
    const GameState& reference,WatcomRandom& random) {
    const auto selected=national_nomination(world,country);
    if(!world.settings||!world.players_initialized||reference.manager_records.size()<5*record_size||
       reference.tactics.size()<5*board_size||reference.formations.size()<0x580)
        throw FormatError("world12: national nomination needs initialized career rules");
    const auto anchor=std::find_if(world.managers.begin(),world.managers.end(),[&](const auto& m){return world.manager_country(m.seat)==country;});
    if(!world.national_coaches&&(anchor==world.managers.end()||!anchor->tactics||!anchor->tactics->backing))
        throw FormatError("world12: national nomination lacks its country manager context");
    const auto team=national_team(world,country);
    const auto prior=std::find_if(world.national_squads.begin(),world.national_squads.end(),[&](const auto& s){return s.team==team;});
    if(prior!=world.national_squads.end()&&prior->nominated_on>=date)
        throw FormatError("world12: national nomination is stale or already completed");
    const auto before=world;auto candidate=world;auto staged=random;
    const auto previous_calls=staged.calls();
    NationalSquad squad;if(prior!=world.national_squads.end())squad=*prior;
    squad.team=team;squad.nominated_on=date;
    if(world.national_coaches){const auto record=std::find_if(world.national_coaches->begin(),world.national_coaches->end(),[&](const auto& value){return value.team==team;});if(record==world.national_coaches->end())throw FormatError("world12: national nomination account is unavailable");squad.coach=record->coach;}
    else squad.coach=world.managers.size()==1?std::optional<uint8_t>{anchor->seat}:std::nullopt;
    auto local=reference;initialize_national_manager_record(local);
    std::copy_n(local.manager_records.begin()+4*record_size,record_size,local.manager_records.begin());
    std::copy_n(local.tactics.begin()+4*board_size,board_size,local.tactics.begin());
    local.manager_teams={kNoTeam};local.manager_options.resize(5);local.manager_records.resize(record_size);
    local.tactics.resize(board_size);local.manager_ratings.resize(60);local.manager_results.assign(size_t(runtime_dimensions.record_teams)*10,255);
    local.level=world.settings->difficulty;local.date=date;local.indoor=false;local.lineups_frozen=false;
    local.projected_foreign_rule=false;local.player_statistics.clear();local.title_history.clear();
    const auto put=[&](uint32_t at,uint8_t value){local.memory.add_block(at,1);local.memory.set8(at,value);};
    put(layout::ManagerCount,1);put(layout::ActiveManager,0);put(layout::GameMode,world.settings->mode);
    put(layout::CoachActive,squad.coach?1:0);
    local.manager_records[0x290]=uint8_t(kNoTeam);local.manager_records[0x291]=uint8_t(kNoTeam>>8);
    local.squad_slots.assign(size_t(runtime_dimensions.total_slots),0xfff);
    local.players.clear();local.player_bonus.clear();local.player_cards.clear();local.player_names.clear();local.player_name_offsets.clear();
    std::vector<PlayerId> mapping;
    const auto include=[&](PlayerId id){if(!id.valid()||std::find(mapping.begin(),mapping.end(),id)!=mapping.end())return;
        const auto& p=world.players.at(id);mapping.push_back(id);local.players.push_back(p.attributes);local.player_bonus.push_back(p.bonus);local.player_cards.push_back(p.cards);
        if(local.player_names.size()+p.attributes.name.size()+1>65535)throw FormatError("world12: national player name pool exceeded");
        local.player_name_offsets.push_back(uint16_t(local.player_names.size()));local.player_names.insert(local.player_names.end(),p.attributes.name.begin(),p.attributes.name.end());local.player_names.push_back(0);};
    for(const auto& row:selected)include(row.player);
    if(prior!=world.national_squads.end()) {
        for(const auto* refs:{&squad.tactics.backing->marks,&squad.tactics.backing->marked_by})for(auto id:*refs)include(id);
        for(auto id:squad.match_state.takers)include(id);
        install_manager_tactics(local,0,squad.tactics,mapping);
    }else for(size_t row=0;row<24;++row)for(size_t offset:{0x44u,0x46u}) {
        local.tactics[row*0x58+offset]=255;local.tactics[row*0x58+offset+1]=15;
    }
    const auto coach=squad.coach?std::find_if(world.managers.begin(),world.managers.end(),[&](const auto& manager){return manager.seat==*squad.coach;}):anchor;
    const auto defaults=initial_national_tactics(reference,squad.coach.has_value());
    const auto& preferences=coach!=world.managers.end()&&coach->tactics?*coach->tactics:prior!=world.national_squads.end()?prior->tactics:defaults;
    if(!preferences.backing)throw FormatError("world12: missing actual national tactical preferences");
    const auto& backing=*preferences.backing;
    std::copy(backing.formations.begin(),backing.formations.end(),local.formations.begin());
    local.projected_formation_tails={backing.formation_tail};
    local.manager_options[0]=preferences.automatic_formation;
    std::copy(backing.automation.begin(),backing.automation.end(),local.manager_options.begin()+1);
    if(squad.coach)local.manager_records[0x218]=backing.enabled_formations[0];
    local.team_flags.at(size_t(kNoTeam))=world.clubs.at(team).match_flags;
    local.team_morale.at(size_t(kNoTeam))=world.clubs.at(team).morale;
    local.club_heap_78.resize(local.club_ids.size()*2);
    local.counts.players=uint32_t(mapping.size());
    if(squad.coach)auto_lineup(local,kNoTeam,0xe4);
    const auto national=squad_range(kNoTeam);
    for(size_t row=0;row<selected.size();++row) {
        local.squad_slots[row]=uint16_t(row)|native_flags(selected[row].flags);
        if(add_to_squad(local,0,int(row),int(national.first),staged)<0)
            throw FormatError("world12: national nomination did not fit its squad");
    }
    const auto option=local.manager_options[0];local.manager_options[0]=1;
    auto_lineup(local,kNoTeam,0xe4);local.manager_options[0]=option;
    for(size_t row=0;row<24;++row) {
        const auto word=local.squad_slots.at(national.first+row);
        squad.roster[row]=(word&0xfff)==0xfff?RosterSlot{}:RosterSlot{mapping.at(word&0xfff),typed_flags(word)};
    }
    squad.tactics=manager_full_tactics_from_legacy(local,0,mapping);
    if(prior==world.national_squads.end())squad.match_state=manager_match_state_from_legacy(local,0,{},mapping);
    if(prior==world.national_squads.end())candidate.national_squads.push_back(squad);
    else *std::find_if(candidate.national_squads.begin(),candidate.national_squads.end(),[&](const auto& s){return s.team==team;})=squad;
    auto& nation=candidate.clubs.at(team);nation.match_flags=local.team_flags.at(size_t(kNoTeam));
    nation.morale=local.team_morale.at(size_t(kNoTeam));nation.lineup_indoor=false;
    candidate.validate();if(world!=before)throw FormatError("world12: national nomination changed while preparing");
    world=std::move(candidate);random=staged;
    return {team,squad.coach,staged.calls()-previous_calls};
}
}
