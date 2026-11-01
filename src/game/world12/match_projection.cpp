// SPDX-License-Identifier: GPL-3.0-only
#include "game/world12/match_projection.hpp"
#include "game/world12/prediction.hpp"
#include "game/world12/standings.hpp"
#include "game/world12/manager_match_state.hpp"
#include "game/world12/manager_tactics.hpp"
#include "game/world12/manager_finance.hpp"
#include "game/world12/manager_creation.hpp"
#include <algorithm>
#include <set>
#include <stdexcept>
#include <utility>
namespace bmh::world12 {
namespace {
uint16_t flags(uint16_t source){return uint16_t(((source&1)?0x8000:0)|((source&2)?0x4000:0)|((source&4)?0x2000:0)|((source&8)?0x1000:0));}
}
AiMatchProjection AiMatchProjection::create(const World& world,const std::vector<ClubId>& requested,const GameState& base){
    if(requested.empty()||requested.size()>64||requested.size()>size_t(runtime_dimensions.big_squad_teams))
        throw std::invalid_argument("world12: match projection requires 1-64 clubs within the native squad workspace");
    world.validate();
    for(const auto& manager:world.managers)if(std::find(requested.begin(),requested.end(),manager.club)!=requested.end())throw std::invalid_argument("world12: human manager projection is not integrated");
    if(runtime_dimensions.big_squad_teams<32)throw std::invalid_argument("world12: legacy match workspace is unavailable");
    AiMatchProjection projection;projection.original_managers_=world.managers;projection.original_development_balance_=world.development_balance;projection.original_trend_rotation_=world.trend_rotation;projection.state=base;auto& state=projection.state;state.manager_teams.clear();state.players.clear();state.player_name_offsets.clear();state.player_names.clear();state.player_cards.clear();state.player_bonus.clear();state.live_matches.clear();state.live_scene=false;state.last_shooter=0xfff;state.projected_match_clubs.clear();state.development_balance=world.development_balance;state.trend_rotation=world.trend_rotation;
    projection.original_settings_=world.settings;
    projection.original_player_statistics_=world.player_statistics;
    state.player_statistics.clear();state.title_history.clear();
    if(world.settings){state.level=world.settings->difficulty;state.projected_foreign_rule=world.settings->foreign_rule;
        const auto put=[&](uint32_t at,uint8_t value){state.memory.add_block(at,1);state.memory.set8(at,value);};
        put(layout::GameMode,world.settings->mode);put(layout::Difficulty,world.settings->difficulty);put(layout::CareerDuration,world.settings->duration);}
    state.squad_slots.assign(size_t(runtime_dimensions.total_slots),0xfff);state.club_names.clear();state.club_ids.assign(base.club_ids.size(),0);state.club_kits.assign(base.club_ids.size(),0);state.club_short_offsets.assign(base.club_ids.size(),0);state.team_flags.assign(base.team_flags.size(),0);state.team_morale.assign(base.team_morale.size(),25);
    state.club_table.assign(base.club_ids.size()*0x4a,0);
    const auto discipline=layout::DeskRecords_Teams_RuntimeField;
    state.memory.add_block(discipline,requested.size()*2);
    if(!state.memory.has(discipline+uint32_t(requested.size()*2-1)))throw std::invalid_argument("world12: discipline workspace too small");
    std::set<ClubId> clubs;std::set<PlayerId> players;std::map<ClubId,uint16_t> record_names;projection.record_opponents_[0]=ClubId{};
    const auto append=[](Bytes& pool,const std::string& value){if(pool.size()+value.size()+1>65535)throw std::invalid_argument("world12: local match string pool overflow");auto offset=uint16_t(pool.size());pool.insert(pool.end(),value.begin(),value.end());pool.push_back(0);return offset;};
    append(state.club_names,"Unused local club");
    for(auto id:requested){const auto it=world.clubs.find(id);if(it==world.clubs.end()||!clubs.insert(id).second||!it->second.league.valid())throw std::invalid_argument("world12: missing, repeated or non-rostered match club");auto team=projection.clubs_.size();if(team>=state.club_ids.size())throw std::invalid_argument("world12: legacy club workspace too small");projection.clubs_.push_back(id);projection.original_clubs_.push_back(it->second);
        const auto& league=world.catalogue.league(it->second.league);
        const int foreign_class=league.country==Country::spain?5:league.country==Country::england?6:league.country==Country::france?7:8;
        const int league_class=league.playable?league.local_index:foreign_class;
        const bool domestic=league.playable;
        state.projected_match_clubs.push_back({league_class,int(league.country),domestic?3:2});
        state.club_kits.at(team)=it->second.kit;state.club_short_offsets.at(team)=it->second.short_span;
        state.team_flags.at(team)=it->second.match_flags;state.team_morale.at(team)=it->second.morale;state.memory.set16(discipline+uint32_t(team*2),it->second.fairplay_points);
        if(!record_names.contains(id))record_names[id]=append(state.club_names,it->second.name);
        state.club_ids[team]=record_names.at(id);projection.record_opponents_[record_names.at(id)]=id;
        const size_t record=team*0x4a;
        std::copy(it->second.season_stats.counters.begin(),it->second.season_stats.counters.end(),state.club_table.begin()+record);
        for(size_t entry=0;entry<8;++entry){
            uint16_t offset=0;const auto opponent=it->second.season_stats.opponents[entry];
            if(opponent.valid()){
                if(!record_names.contains(opponent))record_names[opponent]=append(state.club_names,world.clubs.at(opponent).name);
                offset=record_names.at(opponent);projection.record_opponents_[offset]=opponent;
            }
            const auto date=it->second.season_stats.dates[entry];
            state.club_table[record+0x2a+entry*2]=uint8_t(offset);state.club_table[record+0x2b+entry*2]=uint8_t(offset>>8);
            state.club_table[record+0x3a+entry*2]=uint8_t(date);state.club_table[record+0x3b+entry*2]=uint8_t(date>>8);
        }
        for(size_t row=0;row<24;++row){const auto& slot=it->second.roster[row];if(slot.empty())continue;const auto found=world.players.find(slot.player);if(found==world.players.end()||!players.insert(slot.player).second||projection.players_.size()>=4095)throw std::invalid_argument("world12: invalid local player mapping");auto local=uint16_t(projection.players_.size());projection.players_.push_back(slot.player);projection.originals_.push_back(found->second);state.players.push_back(found->second.attributes);state.player_name_offsets.push_back(append(state.player_names,found->second.attributes.name));state.player_cards.push_back(found->second.cards);state.player_bonus.push_back(found->second.bonus);state.squad_slots[team*24+row]=uint16_t(local|flags(slot.flags));}
    }
    projection.install_player_statistics(world,true);
    state.counts.players=uint32_t(state.players.size());return projection;
}
AiMatchProjection AiMatchProjection::create_for_humans(const CareerSnapshot& snapshot,const std::vector<ClubId>& requested,const GameState& base,HumanProjectionPolicy policy) {
    snapshot.validate();const auto& world=snapshot.world;
    if(world.settings&&(policy.mode!=world.settings->mode||policy.difficulty!=world.settings->difficulty||policy.foreign_rule!=world.settings->foreign_rule))throw std::invalid_argument("world12: human match policy differs from saved career settings");
    if(policy.mode<1||policy.mode>3||policy.difficulty<0||policy.difficulty>9||base.formations.size()<0x580)throw std::invalid_argument("world12: incomplete human projection policy/rules");
    std::vector<const Manager*> managers;
    for(const auto& manager:world.managers)if(std::find(requested.begin(),requested.end(),manager.club)!=requested.end()) {
        if(!manager.finance||!manager.finance->baseline||!manager.tactics||!manager.tactics->backing||!manager.match_state)
            throw std::invalid_argument("world12: complete human account/tactics/match state required");
        if(world.settings&&manager.career&&!manager.career->season_records)throw std::invalid_argument("world12: saved manager campaign history is unavailable");
        managers.push_back(&manager);
    }
    if(managers.empty())throw std::invalid_argument("world12: human projection has no participating manager");
    auto ai_world=world;ai_world.managers.clear();auto result=create(ai_world,requested,base);auto& local=result.state;
    result.original_managers_=world.managers;result.manager_reference_clubs_=requested;
    local.level=policy.difficulty;local.projected_foreign_rule=policy.foreign_rule;local.date=snapshot.date;
    local.projected_ai_formations.emplace();std::copy_n(base.formations.begin(),0x160,local.projected_ai_formations->begin());
    local.manager_records.assign(managers.size()*0xb0c,0);local.manager_options.assign(managers.size()*5,0);
    local.manager_ratings.assign(managers.size()*60,0);local.manager_results.assign(managers.size()*size_t(runtime_dimensions.record_teams)*10,255);
    local.projected_formation_tails.resize(managers.size());local.manager_teams.clear();local.club_heap_78.assign(base.club_ids.size()*2,0);
    const auto set=[&](uint32_t at,uint8_t value){local.memory.add_block(at,1);local.memory.set8(at,value);};
    set(layout::ActiveManager,0);set(layout::ManagerCount,uint8_t(managers.size()));set(layout::OriginalManagerCount,uint8_t(managers.size()));
    set(layout::GameMode,uint8_t(policy.mode));set(layout::Difficulty,uint8_t(policy.difficulty));set(layout::CoachActive,0);
    const auto add_player=[&](PlayerId id) {
        if(!id.valid()||std::find(result.players_.begin(),result.players_.end(),id)!=result.players_.end())return;
        if(result.players_.size()>=4095)throw std::invalid_argument("world12: human reference player workspace exceeded");
        const auto& player=world.players.at(id);result.players_.push_back(id);result.originals_.push_back(player);
        local.players.push_back(player.attributes);local.player_cards.push_back(player.cards);local.player_bonus.push_back(player.bonus);
        if(local.player_names.size()+player.attributes.name.size()+1>65535)throw std::invalid_argument("world12: human name pool exceeded");
        local.player_name_offsets.push_back(uint16_t(local.player_names.size()));local.player_names.insert(local.player_names.end(),player.attributes.name.begin(),player.attributes.name.end());local.player_names.push_back(0);
    };
    for(const auto* manager:managers) {
        if(manager->match_state->attendance) {
            const auto& attendance=*manager->match_state->attendance;
            for(const auto* refs:{&attendance.spectator_opponents,&attendance.income_opponents})for(auto id:*refs)
                if(id.valid()&&std::find(result.manager_reference_clubs_.begin(),result.manager_reference_clubs_.end(),id)==result.manager_reference_clubs_.end())
                    result.manager_reference_clubs_.push_back(id);
        }
        for(auto id:manager->match_state->recent_fixtures)if(id.valid()&&std::find(result.manager_reference_clubs_.begin(),result.manager_reference_clubs_.end(),id)==result.manager_reference_clubs_.end())result.manager_reference_clubs_.push_back(id);
        for(const auto* refs:{&manager->tactics->backing->marks,&manager->tactics->backing->marked_by})for(auto id:*refs)add_player(id);
        for(auto id:manager->match_state->takers)add_player(id);
        if(manager->career)for(auto id:manager->career->poach_attempts)add_player(id);
    }
    if(result.manager_reference_clubs_.size()>local.club_ids.size()||result.manager_reference_clubs_.size()>size_t(runtime_dimensions.record_teams))throw std::invalid_argument("world12: manager history club workspace exceeded");
    for(size_t i=requested.size();i<result.manager_reference_clubs_.size();++i) {
        const auto id=result.manager_reference_clubs_[i];auto found=std::find_if(result.record_opponents_.begin(),result.record_opponents_.end(),[&](const auto& entry){return entry.second==id;});
        uint16_t offset;
        if(found!=result.record_opponents_.end())offset=found->first;
        else {const auto& name=world.clubs.at(id).name;if(local.club_names.size()+name.size()+1>65535)throw std::invalid_argument("world12: manager history name pool exceeded");
            offset=uint16_t(local.club_names.size());local.club_names.insert(local.club_names.end(),name.begin(),name.end());local.club_names.push_back(0);result.record_opponents_[offset]=id;}
        local.club_ids[i]=offset;
    }
    std::map<ClubId,uint8_t> deductions;
    for(const auto& manager:world.managers)if(manager.match_state&&manager.match_state->point_deduction>>5==3)deductions[manager.club]=manager.match_state->point_deduction&31;
    std::map<LeagueId,std::map<ClubId,int>> ranks;
    for(size_t team=0;team<requested.size();++team) {
        const auto& club=world.clubs.at(requested[team]);const auto& league=world.catalogue.league(club.league);auto& context=local.projected_match_clubs[team];
        context.league_size=league.clubs;context.relegation_places=league.relegation;context.round_cursor=snapshot.league_rounds[club.league.value];context.league_name=league.name;context.table_rank=-1;
        if(context.round_cursor>0) {
            TableQuery query;query.three_points=policy.three_points;for(const auto& [id,value]:deductions)if(world.clubs.at(id).league==club.league)query.point_deductions[id]=value;
            if(!ranks.contains(club.league)){const auto rows=compute_standings(world,snapshot.results,club.league,context.round_cursor,query);for(const auto& row:rows)ranks[club.league][row.club]=int(row.rank)-1;}
            context.table_rank=ranks.at(club.league).at(club.id);
        }
        std::map<ClubId,uint8_t> league_deductions;for(const auto& [id,value]:deductions)if(world.clubs.at(id).league==club.league)league_deductions[id]=value;
        if(std::any_of(managers.begin(),managers.end(),[&](const auto* manager){return manager->club==club.id;}))for(int mode:{1,2,3,4,8,12}){const auto prediction=predict_league(snapshot,club.id,mode,league_deductions);context.predictions.push_back({mode,prediction.result.best_rank,prediction.result.ranks,prediction.source_size_word,prediction.result.lost_goals,prediction.result.secured_goals});}
        if(team*2+1>=local.club_heap_78.size())throw std::invalid_argument("world12: club mode storage missing");
        local.club_heap_78[team*2]=0;local.club_heap_78[team*2+1]=uint8_t(club.lineup_indoor?2:0);
    }
    for(size_t m=0;m<managers.size();++m) {
        const auto& manager=*managers[m];const auto team=result.local_club(manager.club);result.manager_seats_.push_back(manager.seat);local.manager_teams.push_back(int(team));
        const auto at=m*0xb0c;for(size_t i=0;i<manager.name.size();++i)local.manager_records[at+0xa8+i]=uint8_t(manager.name[i]);
        local.manager_records[at+0x210]=world.catalogue.league(world.clubs.at(manager.club).league).local_index;local.manager_records[at+0x221]=manager.portrait;
        local.manager_records[at+0x290]=uint8_t(team);local.manager_records[at+0x291]=uint8_t(team>>8);local.team_morale[team]|=0x80;
        install_manager_finance(local,int(m),*manager.finance);install_manager_tactics(local,int(m),*manager.tactics,result.players_);
        install_manager_match_state(local,int(m),*manager.match_state,result.manager_reference_clubs_,result.players_);
        if(manager.career)install_manager_career_state(local,uint8_t(m),*manager.career,result.players_);
    }
    result.install_player_statistics(world,true);
    local.counts.players=uint32_t(local.players.size());return result;
}
AiMatchProjection AiMatchProjection::create_for_desktop(const CareerSnapshot& snapshot,const std::vector<ClubId>& clubs,const GameState& reference,HumanProjectionPolicy policy) {
    auto result=create_for_humans(snapshot,clubs,reference,policy);auto& local=result.state;
    std::vector<size_t> order;for(size_t i=0;i<result.players_.size();++i)order.push_back(i);
    std::sort(order.begin(),order.end(),[&](size_t a,size_t b){return result.players_[a]<result.players_[b];});
    const auto ids=result.players_;const auto originals=result.originals_;const auto players=local.players;const auto cards=local.player_cards;const auto bonuses=local.player_bonus;const auto offsets=local.player_name_offsets;
    std::vector<uint16_t> remap(order.size());
    for(size_t i=0;i<order.size();++i){const auto old=order[i];remap[old]=uint16_t(i);result.players_[i]=ids[old];result.originals_[i]=originals[old];local.players[i]=players[old];local.player_cards[i]=cards[old];local.player_bonus[i]=bonuses[old];local.player_name_offsets[i]=offsets[old];}
    for(auto& slot:local.squad_slots)if((slot&0xfff)!=0xfff)slot=uint16_t((slot&0xf000)|remap.at(slot&0xfff));
    // Reinstall every known board/record reference from its canonical global ID.
    // Names and club mappings do not depend on the player ordering.
    for(size_t i=0;i<result.manager_seats_.size();++i){const auto seat=result.manager_seats_[i];const auto& manager=*std::find_if(snapshot.world.managers.begin(),snapshot.world.managers.end(),[&](const auto& m){return m.seat==seat;});
        install_manager_tactics(local,int(i),*manager.tactics,result.players_);
        install_manager_match_state(local,int(i),*manager.match_state,result.manager_reference_clubs_,result.players_);
        if(manager.career)install_manager_career_state(local,uint8_t(i),*manager.career,result.players_);
    }
    result.install_player_statistics(snapshot.world,true);
    return result;
}
void AiMatchProjection::include_players(const World& world,const std::vector<PlayerId>& requested) {
    if(world.managers!=original_managers_||world.settings!=original_settings_)throw std::invalid_argument("world12: stale extra-player workspace");
    for(auto id:requested){if(!id.valid())continue;if(std::find(players_.begin(),players_.end(),id)!=players_.end())continue;
        const auto& p=world.players.at(id);if(players_.size()>=4095||state.player_names.size()+p.attributes.name.size()+1>65535)throw std::invalid_argument("world12: extra-player workspace overflow");
        players_.push_back(id);originals_.push_back(p);state.players.push_back(p.attributes);state.player_cards.push_back(p.cards);state.player_bonus.push_back(p.bonus);
        state.player_name_offsets.push_back(uint16_t(state.player_names.size()));state.player_names.insert(state.player_names.end(),p.attributes.name.begin(),p.attributes.name.end());state.player_names.push_back(0);
    }
    install_player_statistics(world,false);
    state.counts.players=uint32_t(players_.size());
}
void AiMatchProjection::include_club_references(const World& world,const std::vector<ClubId>& requested) {
    if(world.managers!=original_managers_||world.settings!=original_settings_)throw std::invalid_argument("world12: stale extra-club workspace");
    if(manager_reference_clubs_.empty())manager_reference_clubs_=clubs_;
    for(auto id:requested){if(!id.valid())continue;if(std::find(manager_reference_clubs_.begin(),manager_reference_clubs_.end(),id)!=manager_reference_clubs_.end())continue;
        const auto& club=world.clubs.at(id);const auto local=manager_reference_clubs_.size();if(local>=state.club_ids.size()||state.club_names.size()+club.name.size()+1>65535)throw std::invalid_argument("world12: extra-club reference overflow");
        const auto offset=uint16_t(state.club_names.size());state.club_names.insert(state.club_names.end(),club.name.begin(),club.name.end());state.club_names.push_back(0);
        manager_reference_clubs_.push_back(id);state.club_ids[local]=offset;record_opponents_[offset]=id;
        if(local<size_t(runtime_dimensions.record_teams))for(size_t m=0;m<manager_seats_.size();++m){auto at=state.manager_results.begin()+ptrdiff_t((m*size_t(runtime_dimensions.record_teams)+local)*10);std::fill_n(at,10,uint8_t(255));
            const auto& manager=*std::find_if(world.managers.begin(),world.managers.end(),[&](const auto& actor){return actor.seat==manager_seats_[m];});
            const auto previous=manager.match_state->head_to_head.find(id);if(previous!=manager.match_state->head_to_head.end())std::copy(previous->second.begin(),previous->second.end(),at);
        }
    }
}
uint32_t AiMatchProjection::local_club_reference(ClubId id)const {
    const auto& refs=manager_reference_clubs_.empty()?clubs_:manager_reference_clubs_;const auto found=std::find(refs.begin(),refs.end(),id);
    if(found==refs.end())throw std::out_of_range("world12: club reference is not projected");return uint32_t(found-refs.begin());
}
ClubId AiMatchProjection::global_club(uint32_t local)const{return manager_reference_clubs_.empty()?clubs_.at(local):manager_reference_clubs_.at(local);}
PlayerId AiMatchProjection::global_player(uint32_t local)const{return players_.at(local);}
uint32_t AiMatchProjection::local_club(ClubId id)const{auto it=std::find(clubs_.begin(),clubs_.end(),id);if(it==clubs_.end())throw std::out_of_range("world12: club is not projected");return uint32_t(it-clubs_.begin());}
uint32_t AiMatchProjection::local_player(PlayerId id)const{auto it=std::find(players_.begin(),players_.end(),id);if(it==players_.end())throw std::out_of_range("world12: player is not projected");return uint32_t(it-players_.begin());}
void AiMatchProjection::commit_player_attributes(World& world)const {
    if(!manager_seats_.empty())throw std::invalid_argument("world12: human projection requires complete match-state commit");
    PlayerStatisticsArchive expected;
    for(const auto& [local,global]:statistics_bindings_)expected.emplace(local,original_player_statistics_.at(global));
    if(state.player_statistics!=expected)throw std::invalid_argument("world12: player statistics require complete match-state commit");
    if(world.managers!=original_managers_||world.settings!=original_settings_)throw std::invalid_argument("world12: manager ownership/settings changed during projection");
    if(state.players.size()!=players_.size()||state.player_bonus.size()!=players_.size())throw std::invalid_argument("world12: local player storage resized");
    for(size_t i=0;i<players_.size();++i)if(!world.players.contains(players_[i])||world.players.at(players_[i])!=originals_.at(i))throw std::invalid_argument("world12: projected player was removed or changed externally");
    auto candidate=world;
    for(size_t i=0;i<players_.size();++i){auto& target=candidate.players.at(players_[i]);target.attributes=state.players[i];target.bonus=state.player_bonus[i];}
    candidate.validate();world=std::move(candidate);
}
void AiMatchProjection::commit_match_state(World& world,bool capture_season_services)const {commit_state(world,capture_season_services,{});}
void AiMatchProjection::commit_transfer_state(World& world,PlayerId player,ClubId seller,ClubId buyer,bool moved)const {
    if(seller==buyer||!player.valid())throw std::invalid_argument("world12: invalid transfer movement");
    if(seller.valid())local_club(seller);local_club(buyer);local_player(player);
    commit_state(world,false,moved?std::vector<TransferMove>{{player,seller,buyer}}:std::vector<TransferMove>{});
}
void AiMatchProjection::commit_released_players(World& world,const std::vector<std::pair<PlayerId,ClubId>>& released)const{
    std::vector<TransferMove> moves;std::set<PlayerId> seen;
    for(const auto& [player,seller]:released){local_player(player);local_club(seller);if(!seen.insert(player).second)throw std::invalid_argument("world12: player released twice");moves.push_back({player,seller,{}});}
    commit_state(world,true,moves);
}
void AiMatchProjection::install_player_statistics(const World& world,bool reset) {
    if(reset){state.player_statistics.clear();statistics_bindings_.clear();}
    for(const auto& [identity,counters]:world.player_statistics) {
        const auto club=std::find(clubs_.begin(),clubs_.end(),identity.first);
        const auto player=std::find(players_.begin(),players_.end(),identity.second);
        if(club==clubs_.end()||player==players_.end())continue;
        const auto key=uint32_t(club-clubs_.begin())<<12|uint32_t(player-players_.begin());
        statistics_bindings_[key]=identity;
        state.player_statistics.try_emplace(key,counters);
    }
}
void AiMatchProjection::capture_player_statistics(World& candidate,const std::vector<TransferMove>& movements)const {
    const auto reset_owned=[&](const auto& identity) {
        return std::any_of(movements.begin(),movements.end(),[&](const auto& move){return move.buyer==identity.first&&move.player==identity.second;});
    };
    for(const auto& [local,global]:statistics_bindings_)
        if(!state.player_statistics.contains(local)) {
            if(!reset_owned(global))throw std::invalid_argument("world12: projected player statistics disappeared");
            candidate.player_statistics.erase(global);
        }
    for(const auto& [local,counters]:state.player_statistics) {
        const auto club=local>>12,player=local&0xfff;
        if(club>=clubs_.size()||player>=players_.size())throw std::invalid_argument("world12: unknown local statistics identity");
        const auto identity=std::pair{clubs_[club],players_[player]};
        const auto previous=original_player_statistics_.find(identity);
        if(previous==original_player_statistics_.end()) {
            const bool managed=std::find(state.manager_teams.begin(),state.manager_teams.end(),int(club))!=state.manager_teams.end();
            const auto& roster=original_clubs_[club].roster;
            const bool occupied=std::any_of(roster.begin(),roster.end(),[&](const auto& row){return row.player==identity.second;});
            if(!managed||(!occupied&&!reset_owned(identity)))throw std::invalid_argument("world12: unowned statistics addition");
        } else if(!reset_owned(identity)) {
            const auto& old=previous->second;
            if(counters.total<old.total||counters.goals<old.goals||counters.assists<old.assists||
                !std::equal(counters.appearances.begin(),counters.appearances.end(),old.appearances.begin(),[](auto next,auto before){return next>=before;}))
                throw std::invalid_argument("world12: cumulative statistics decreased without an owned arrival");
        }
        candidate.player_statistics[identity]=counters;
    }
}
void AiMatchProjection::commit_state(World& world,bool capture_season_services,const std::vector<TransferMove>& movements)const {
    if(world.player_statistics!=original_player_statistics_)
        throw std::invalid_argument("world12: global player statistics changed during projection");
    if(!manager_seats_.empty()&&(state.manager_teams.size()!=manager_seats_.size()||state.manager_records.size()!=manager_seats_.size()*0xb0c||
        state.manager_options.size()!=manager_seats_.size()*5||state.manager_ratings.size()!=manager_seats_.size()*60||
        state.manager_results.size()!=manager_seats_.size()*size_t(runtime_dimensions.record_teams)*10))
        throw std::invalid_argument("world12: human manager workspace resized");
    if(world.development_balance!=original_development_balance_ || world.trend_rotation!=original_trend_rotation_)
        throw std::invalid_argument("world12: development stream changed during projection");
    if(world.managers!=original_managers_||world.settings!=original_settings_)throw std::invalid_argument("world12: manager ownership/settings changed during projection");
    if(state.players.size()!=players_.size() || state.player_bonus.size()!=players_.size() || state.player_cards.size()!=players_.size())
        throw std::invalid_argument("world12: projected player storage resized");
    for(size_t i=0;i<players_.size();++i)if(!world.players.contains(players_[i]) || world.players.at(players_[i])!=originals_.at(i))
        throw std::invalid_argument("world12: projected player changed externally");
    for(size_t i=0;i<clubs_.size();++i)if(!world.clubs.contains(clubs_[i]) || world.clubs.at(clubs_[i])!=original_clubs_.at(i))
        throw std::invalid_argument("world12: projected club or roster changed externally");
    auto candidate=world;
    for(size_t i=0;i<players_.size();++i){auto& target=candidate.players.at(players_[i]);target.attributes=state.players[i];target.bonus=state.player_bonus[i];target.cards=state.player_cards[i];}
    for(size_t team=0;team<clubs_.size();++team) {
        auto& target=candidate.clubs.at(clubs_[team]);std::set<PlayerId> before,after;
        for(const auto& slot:target.roster)if(!slot.empty())before.insert(slot.player);
        for(size_t row=0;row<24;++row) {
            const auto raw=state.squad_slots.at(team*24+row);auto& slot=target.roster[row];
            if((raw&0xfff)==0xfff){if(raw!=0xfff)throw std::invalid_argument("world12: match flags on an empty roster slot");slot={};}
            else {slot.player=global_player(raw&0xfff);slot.flags=uint16_t(((raw&0x8000)?1:0)|((raw&0x4000)?2:0)|((raw&0x2000)?4:0)|((raw&0x1000)?8:0));after.insert(slot.player);}
        }
        for(const auto& movement:movements){if(clubs_[team]==movement.seller){if(!before.erase(movement.player))throw std::invalid_argument("world12: transferred player was not at seller");}if(clubs_[team]==movement.buyer&&!before.insert(movement.player).second)throw std::invalid_argument("world12: transferred player was already at buyer");}
        if(before!=after)throw std::invalid_argument("world12: operation changed unowned roster membership");
        target.match_flags=state.team_flags.at(team);target.fairplay_points=state.memory.u16(layout::DeskRecords_Teams_RuntimeField+uint32_t(team*2));target.morale=state.team_morale.at(team)&63;
        const size_t record=team*0x4a;
        for(size_t i=0;i<42;++i)target.season_stats.counters[i]=state.club_table.at(record+i);
        for(size_t i=0;i<8;++i){
            const auto offset=uint16_t(state.club_table.at(record+0x2a+i*2)|uint16_t(state.club_table.at(record+0x2b+i*2))<<8);
            const auto found=record_opponents_.find(offset);
            if(found==record_opponents_.end())throw std::invalid_argument("world12: unknown local statistics opponent");
            target.season_stats.opponents[i]=found->second;
            target.season_stats.dates[i]=uint16_t(state.club_table.at(record+0x3a+i*2)|uint16_t(state.club_table.at(record+0x3b+i*2))<<8);
        }
    }
    for(const auto& movement:movements)if(!movement.seller.valid()) {
        if(!candidate.free_players)throw std::invalid_argument("world12: acquired player has no free pool");
        bool found=false;const auto remove=[&](auto& pool){const auto at=std::find(pool.begin(),pool.end(),movement.player);if(at==pool.end())return;if(found)throw std::invalid_argument("world12: free player belongs to duplicate pools");*at={};found=true;};
        remove(candidate.free_players->domestic);remove(candidate.free_players->foreign);
        if(!found)throw std::invalid_argument("world12: transferred free player was not available");
    }
    for(size_t local=0;local<manager_seats_.size();++local) {
        const auto found=std::find_if(candidate.managers.begin(),candidate.managers.end(),[&](const auto& manager){return manager.seat==manager_seats_[local];});
        if(found==candidate.managers.end()||size_t(local)>=state.manager_teams.size()||global_club(uint32_t(state.manager_teams[local]))!=found->club)
            throw std::invalid_argument("world12: local manager ownership changed");
        const auto rec=local*0xb0c;const auto record_team=uint16_t(state.manager_records[rec+0x290]|uint16_t(state.manager_records[rec+0x291])<<8);
        if(record_team!=state.manager_teams[local]||state.manager_records[rec+0x210]!=candidate.catalogue.league(candidate.clubs.at(found->club).league).local_index||state.manager_records[rec+0x221]!=found->portrait)
            throw std::invalid_argument("world12: human manager record identity changed");
        for(size_t i=0;i<25;++i)if(state.manager_records[rec+0xa8+i]!=(i<found->name.size()?uint8_t(found->name[i]):0))throw std::invalid_argument("world12: human manager record name changed");
        found->finance=manager_initial_finance_from_legacy(state,int(local));
        found->tactics=manager_full_tactics_from_legacy(state,int(local),players_);
        const auto previous=found->match_state;
        found->match_state=manager_match_state_from_legacy(state,int(local),manager_reference_clubs_,players_,previous?&*previous:nullptr);
        if(found->career)found->career=manager_career_state_from_legacy(state,uint8_t(local),players_,capture_season_services||found->career->season_services.has_value());
        const auto team=local_club(found->club);candidate.clubs.at(found->club).lineup_indoor=(state.club_heap_78.at(team*2+1)&2)!=0;
    }
    capture_player_statistics(candidate,movements);
    candidate.development_balance=state.development_balance;
    if(state.trend_rotation<0 || state.trend_rotation>2)throw std::invalid_argument("world12: invalid local development rotation");
    candidate.trend_rotation=uint8_t(state.trend_rotation);candidate.validate();world=std::move(candidate);
}
}
