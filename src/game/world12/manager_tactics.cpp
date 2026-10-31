// SPDX-License-Identifier: GPL-3.0-only
#include "game/world12/manager_tactics.hpp"
#include "game/lineup.hpp"
#include "game/career.hpp"
#include "game/national.hpp"
#include "game/world12/manager_finance.hpp"
#include <algorithm>
#include <stdexcept>
namespace bmh::world12 {
namespace {
constexpr size_t board_size=0x840,row_size=0x58,record_size=0xb0c;
std::vector<PlayerId> mapped_players(const World& world,const Manager& manager) {
    std::vector<PlayerId> result;
    for(const auto& slot:world.clubs.at(manager.club).roster)if(!slot.empty())result.push_back(slot.player);
    if(manager.tactics->backing)for(const auto* refs:{&manager.tactics->backing->marks,&manager.tactics->backing->marked_by})for(auto id:*refs)
        if(id.valid()&&std::find(result.begin(),result.end(),id)==result.end())result.push_back(id);
    return result;
}
void put_rows(GameState& state,const ManagerTactics& input,const std::vector<PlayerId>& mapping) {
    if(input.backing) {
        for(size_t row=0;row<24;++row)std::copy(input.backing->scalar_rows[row].begin(),input.backing->scalar_rows[row].end(),state.tactics.begin()+row*row_size);
        const auto put=[&](size_t row,size_t offset,PlayerId id){uint16_t local=0xfff;
            if(id.valid()){const auto found=std::find(mapping.begin(),mapping.end(),id);if(found==mapping.end())throw std::invalid_argument("world12: missing mapped tactical reference");local=uint16_t(found-mapping.begin());}
            state.tactics[row*row_size+offset]=uint8_t(local);state.tactics[row*row_size+offset+1]=uint8_t(local>>8);};
        for(size_t row=0;row<24;++row){put(row,0x44,input.backing->marks[row]);put(row,0x46,input.backing->marked_by[row]);}
    }
    for(size_t i=0;i<24;++i){const auto at=i*row_size;const auto& r=input.rows[i];
        std::copy(r.orders.begin(),r.orders.end(),state.tactics.begin()+at+2);
        state.tactics[at+0x3f]=r.partner;state.tactics[at+0x41]=r.depth;state.tactics[at+0x42]=r.lane;}
}
GameState workspace(const World& world,const Manager& manager,TacticalPolicy policy) {
    if(runtime_dimensions.big_squad_teams<1 || squad_range(0).count!=24)
        throw std::invalid_argument("world12: human tactical workspace requires a 24-player squad");
    const auto& club=world.clubs.at(manager.club);GameState state;
    state.indoor=policy.indoor;state.lineups_frozen=policy.frozen;state.manager_teams={0};state.level=policy.difficulty;
    state.projected_foreign_rule=policy.foreign_rule_enabled;
    const auto& league=world.catalogue.league(club.league);state.projected_match_clubs={{league.local_index,int(club.country),3}};
    state.club_ids={0};state.team_flags={club.match_flags};state.club_heap_78={0,uint8_t(club.lineup_indoor?2:0)};
    state.manager_records.assign(record_size,0);state.manager_records[0x288]=manager.tactics->automatic_weights;
    state.manager_options.assign(5,0);state.manager_options[0]=manager.tactics->automatic_formation;
    const auto mapping=mapped_players(world,manager);
    state.formations.resize(0x580);state.tactics.resize(board_size);
    if(manager.tactics->backing) {
        const auto& backing=*manager.tactics->backing;
        std::copy(backing.formations.begin(),backing.formations.end(),state.formations.begin());
        std::copy(backing.formation_tail.begin(),backing.formation_tail.end(),state.formations.begin()+0x160);
        std::copy(backing.enabled_formations.begin(),backing.enabled_formations.end(),state.manager_records.begin()+0x218);
        std::copy(backing.automation.begin(),backing.automation.end(),state.manager_options.begin()+1);
    }
    put_rows(state,*manager.tactics,mapping);
    state.memory.add_block(layout::ActiveManager,1);state.memory.set8(layout::ActiveManager,0);
    state.memory.add_block(layout::GameMode,1);state.memory.set8(layout::GameMode,uint8_t(policy.mode));
    state.squad_slots.assign(24,0xfff);
    for(auto id:mapping){const auto& p=world.players.at(id);state.players.push_back(p.attributes);state.player_cards.push_back(p.cards);state.player_bonus.push_back(p.bonus);
        if(state.player_names.size()+p.attributes.name.size()+1>65535)throw std::invalid_argument("world12: tactical name pool overflow");
        state.player_name_offsets.push_back(uint16_t(state.player_names.size()));state.player_names.insert(state.player_names.end(),p.attributes.name.begin(),p.attributes.name.end());state.player_names.push_back(0);}
    state.counts.players=uint32_t(state.players.size());
    for(size_t row=0;row<24;++row)if(!club.roster[row].empty()) {
        const auto& slot=club.roster[row];const auto local=uint16_t(std::find(mapping.begin(),mapping.end(),slot.player)-mapping.begin());
        state.squad_slots[row]=uint16_t(local|((slot.flags&1)?0x8000:0)|((slot.flags&2)?0x4000:0)|((slot.flags&4)?0x2000:0)|((slot.flags&8)?0x1000:0));
    }
    return state;
}
}
ManagerTactics manager_tactics_from_legacy(const GameState& state,int manager) {
    if(manager<0 || manager>3 || state.tactics.size()<size_t(manager+1)*board_size ||
        state.manager_records.size()<size_t(manager+1)*record_size || state.manager_options.size()<size_t(manager+1)*5)
        throw std::invalid_argument("world12: missing initialized tactical manager fields");
    ManagerTactics result;result.automatic_weights=state.manager_records[size_t(manager)*record_size+0x288];
    result.automatic_formation=state.manager_options[size_t(manager)*5];
    for(size_t row=0;row<24;++row){const auto at=size_t(manager)*board_size+row*row_size;auto& r=result.rows[row];
        std::copy_n(state.tactics.begin()+at+2,4,r.orders.begin());r.partner=state.tactics[at+0x3f];r.depth=state.tactics[at+0x41];r.lane=state.tactics[at+0x42];}
    validate_manager_tactics(result);return result;
}
ManagerTactics manager_full_tactics_from_legacy(const GameState& state,int manager,const std::vector<PlayerId>& mapping) {
    auto result=manager_tactics_from_legacy(state,manager);
    if(state.formations.size()<0x580)throw std::invalid_argument("world12: full Original formation table is missing");
    ManagerTacticalBacking backing;
    for(size_t row=0;row<24;++row) {
        const auto at=size_t(manager)*board_size+row*row_size;
        std::copy_n(state.tactics.begin()+at,row_size,backing.scalar_rows[row].begin());
        for(size_t offset:{2u,3u,4u,5u,0x3fu,0x41u,0x42u,0x44u,0x45u,0x46u,0x47u})backing.scalar_rows[row][offset]=0;
        const auto player=[&](size_t offset){const auto local=uint16_t(state.tactics[at+offset]|uint16_t(state.tactics[at+offset+1])<<8);
            if(local==0xfff)return PlayerId{};
            if(local>=mapping.size()||!mapping[local].valid())throw std::invalid_argument("world12: unresolved tactical player reference");return mapping[local];};
        backing.marks[row]=player(0x44);backing.marked_by[row]=player(0x46);
    }
    const auto form=size_t(manager)*0x160;
    std::copy_n(state.formations.begin()+form,0x160,backing.formations.begin());
    for(size_t i=0;i<13;++i){const auto at=form+0x160+i;backing.formation_tail[i]=at<0x580?state.formations.at(at):state.tactics.at(at-0x580);}
    std::copy_n(state.manager_records.begin()+size_t(manager)*record_size+0x218,2,backing.enabled_formations.begin());
    std::copy_n(state.manager_options.begin()+size_t(manager)*5+1,4,backing.automation.begin());
    result.backing=backing;validate_manager_tactics(result);return result;
}
TacticalOutcome calculate_manager_tactical_action(const World& world,uint8_t seat,const TacticalAction& action,TacticalPolicy policy) {
    world.validate();if(policy.mode<1 || policy.mode>3 || policy.difficulty<0 || policy.difficulty>9)throw std::invalid_argument("world12: invalid tactical game mode");
    const auto manager=std::find_if(world.managers.begin(),world.managers.end(),[&](const auto& row){return row.seat==seat;});
    if(manager==world.managers.end() || !manager->tactics)throw std::invalid_argument("world12: manager tactics have not been initialized by the career owner");
    const auto& before=*manager->tactics;TacticalOutcome result{false,false,before};if(policy.frozen)return result;
    if(action.kind==TacticalActionKind::order || action.kind==TacticalActionKind::move || action.kind==TacticalActionKind::swap) {
        if(action.row<0 || action.row>=24 || !action.player.valid() ||
            world.clubs.at(manager->club).roster[size_t(action.row)].player!=action.player)return result;
    }
    auto local=workspace(world,*manager,policy);bool allowed=false;
    switch(action.kind) {
    case TacticalActionKind::order:
        if(action.aspect<0 || action.aspect>3 || before.rows[size_t(action.row)].orders[size_t(action.aspect)]!=action.expected_order)return result;
        allowed=set_player_order_source(local,0,0,action.row,local.squad_slots[size_t(action.row)]&0xfff,action.aspect,action.value);break;
    case TacticalActionKind::move:
        allowed=move_pitch_player_source(local,0,0,action.row,local.squad_slots[size_t(action.row)]&0xfff,local.squad_slots[size_t(action.row)],
            action.old_depth,action.old_lane,action.depth,action.lane);break;
    case TacticalActionKind::weights:
        allowed=set_pitch_automatic_weight_source(local,0,0,action.aspect,action.old_weights,action.new_weights);break;
    case TacticalActionKind::swap: {
        const auto& roster=world.clubs.at(manager->club).roster;
        if(action.second_row<0 || action.second_row>=24 || roster[size_t(action.row)]!=action.first_slot ||
            roster[size_t(action.second_row)]!=action.second_slot)return result;
        allowed=swap_pitch_roster_rows_source(local,0,0,action.row,action.second_row,local.squad_slots[size_t(action.row)],
            local.squad_slots[size_t(action.second_row)],policy.foreign_rule_enabled);
        break;
    }
    case TacticalActionKind::apply_formation:
    case TacticalActionKind::save_formation:
    case TacticalActionKind::prepare:
        if(!before.backing)throw std::invalid_argument("world12: complete tactical backing is required for formation/entry actions");
        if(action.kind==TacticalActionKind::apply_formation)allowed=apply_pitch_formation_source(local,0,0,action.formation,action.old_weights);
        else if(action.kind==TacticalActionKind::save_formation)allowed=save_pitch_formation_source(local,0,0,action.formation,action.old_weights,action.formation_positions);
        else {auto_lineup(local,0,0xe4);prepare_lineup_entry(local,0);allowed=true;}
        break;
    default:throw std::invalid_argument("world12: unsupported tactical action");
    }
    if(allowed){result.allowed=true;result.tactics=before.backing?manager_full_tactics_from_legacy(local,0,mapped_players(world,*manager)):manager_tactics_from_legacy(local,0);
        if(action.kind==TacticalActionKind::swap || action.kind==TacticalActionKind::apply_formation || action.kind==TacticalActionKind::save_formation || action.kind==TacticalActionKind::prepare) {
            TacticalLineup lineup;lineup.match_flags=local.team_flags.at(0);lineup.indoor=(local.club_heap_78.at(1)&2)!=0;const auto mapping=mapped_players(world,*manager);
            for(size_t row=0;row<24;++row){const auto raw=local.squad_slots[row];if((raw&0xfff)==0xfff)continue;const auto id=raw&0xfff;
                lineup.roster[row]={mapping.at(id),uint16_t(((raw&0x8000)?1:0)|((raw&0x4000)?2:0)|((raw&0x2000)?4:0)|((raw&0x1000)?8:0))};lineup.cards_by_row[row]=local.player_cards.at(id);}
            result.lineup=lineup;
        }
        result.changed=result.tactics!=before;
        if(result.lineup){const auto& roster=world.clubs.at(manager->club).roster;result.changed|=result.lineup->roster!=roster;const auto& club=world.clubs.at(manager->club);result.changed|=result.lineup->match_flags!=club.match_flags || result.lineup->indoor!=club.lineup_indoor;
            for(size_t row=0;row<24;++row)if(!result.lineup->roster[row].empty())result.changed|=world.players.at(result.lineup->roster[row].player).cards!=result.lineup->cards_by_row[row];}}
    return result;
}
uint64_t initialize_manager_contracts(World& world,uint8_t seat,const GameState& rules,TacticalPolicy policy,WatcomRandom& random) {
    world.validate();
    if(!world.players_initialized || policy.mode<1 || policy.mode>3 || policy.difficulty<0 || policy.difficulty>9 || policy.indoor || policy.frozen ||
        world.development_balance || world.trend_rotation || !world.loans.empty())
        throw std::invalid_argument("world12: initial manager contracts require the fresh outdoor career phase");
    for(const auto& [id,club]:world.clubs)if(club.season_stats!=ClubSeasonStats{})throw std::invalid_argument("world12: played career cannot initialize manager contracts");
    const auto found=std::find_if(world.managers.begin(),world.managers.end(),[&](const auto& manager){return manager.seat==seat;});
    if(found==world.managers.end() || found->finance || found->tactics)throw std::invalid_argument("world12: manager contract/account state already exists or seat is missing");
    if(rules.manager_options.size()<size_t(seat+1)*5 || rules.manager_options[size_t(seat)*5]!=1 || rules.formations.size()<0x580 ||
        rules.tactics.size()<size_t(seat+1)*board_size || rules.manager_records.size()<size_t(seat+1)*record_size)
        throw std::invalid_argument("world12: initial manager formation/automation rules missing");
    const auto before=world;auto candidate=world;const auto old_seed=random.seed();const auto old_calls=random.calls();auto staged_random=random;
    const auto index=size_t(found-world.managers.begin());auto& manager=candidate.managers[index];
    auto initial=rules;
    initial.manager_records[size_t(seat)*record_size+0x288]=0x16;
    initial.manager_records[size_t(seat)*record_size+0x218]=initial.manager_records[size_t(seat)*record_size+0x219]=15;
    // Unoccupied initial rows have no player to mark. Do not reinterpret the
    // zeroed Original BSS words as references to the first global player.
    for(size_t row=0;row<24;++row)for(size_t offset:{0x44u,0x46u}){
        const auto at=size_t(seat)*board_size+row*row_size+offset;initial.tactics[at]=0xff;initial.tactics[at+1]=0x0f;}
    manager.tactics=manager_full_tactics_from_legacy(initial,seat,{});
    auto local=workspace(candidate,manager,policy);const auto mapping=mapped_players(candidate,manager);
    for(size_t row=0;row<24;++row) {
        const auto raw=local.squad_slots[row];if((raw&0xfff)==0xfff)break;
        auto& p=local.players.at(raw&0xfff);p.w364=(p.w364&~(0x1fu<<21))|(uint32_t(staged_random.range(4,9))<<21);
        bmh::new_board_entry(local,0,int(row),int(row),staged_random);local.tactics[row*row_size+0xd]=100;
    }
    int tier=candidate.catalogue.league(candidate.clubs.at(manager.club).league).local_index;
    if(tier>2)--tier;if(tier>2)tier=2;
    bmh::initialize_career_manager_funds(local,0,policy.difficulty,tier);
    bmh::auto_lineup(local,0,100);
    manager.tactics=manager_full_tactics_from_legacy(local,0,mapping);
    manager.finance=manager_initial_finance_from_legacy(local,0);
    auto& club=candidate.clubs.at(manager.club);club.match_flags=local.team_flags[0];club.lineup_indoor=(local.club_heap_78[1]&2)!=0;
    for(size_t row=0;row<24;++row) {
        const auto raw=local.squad_slots[row];if((raw&0xfff)==0xfff){club.roster[row]={};continue;}
        const auto local_id=raw&0xfff;const auto id=mapping.at(local_id);
        club.roster[row]={id,uint16_t(((raw&0x8000)?1:0)|((raw&0x4000)?2:0)|((raw&0x2000)?4:0)|((raw&0x1000)?8:0))};
        candidate.players.at(id).attributes=local.players.at(local_id);candidate.players.at(id).cards=local.player_cards.at(local_id);
    }
    candidate.validate();
    if(world!=before || random.seed()!=old_seed || random.calls()!=old_calls)throw std::invalid_argument("world12: manager initialization owner/RNG changed");
    const auto draws=staged_random.calls()-old_calls;world=std::move(candidate);random=staged_random;return draws;
}

}
