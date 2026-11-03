// SPDX-License-Identifier: GPL-3.0-only
#include "app/preparation_actions.hpp"
#include "game/national_identity.hpp"
#include <algorithm>
#include <iostream>
using namespace bmh;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
}
int main()try{
    auto rules=LeImage::from_bytes(layout::GermanDataSegmentBegin,Bytes(0x6000));GameState game;
    game.manager_teams={kNoTeam,kNoTeam+2};game.manager_records.resize(2*0xb0c);game.manager_options.resize(10);game.tactics.resize(2*0x840);
    game.memory.add_block(layout::ManagerCount,1);game.memory.set8(layout::ManagerCount,2);game.memory.add_block(layout::ActiveManager,1);game.memory.set8(layout::ActiveManager,1);game.memory.add_block(layout::GameMode,1);game.memory.set8(layout::GameMode,3);game.memory.add_block(layout::PlainBackground,1);game.memory.add_block(layout::TextGradientDisabled,1);
    game.projected_national_teams={{kNoTeam,0},{kNoTeam+2,24}};game.squad_slots.assign(48,0xfff);game.players.resize(4);game.player_names={'A',0,'B',0,'C',0,'D',0};game.player_name_offsets={0,2,4,6};game.club_names={'N','a','t','i','o','n',0};game.club_ids.resize(size_t(kNoTeam)+62);
    for(size_t manager=0;manager<2;++manager){std::fill_n(game.manager_records.begin()+ptrdiff_t(manager*0xb0c+0xc5),13,2);game.manager_options[manager*5+4]=0x80;for(size_t row=0;row<2;++row){game.squad_slots[manager*24+row]=uint16_t(manager*2+row);game.players[manager*2+row].w364=uint32_t(row);game.tactics[manager*0x840+row*0x58+0xa]=2;}}
    const auto before=game;const auto view=ui::training_from_game(game,rules,7);check(view.runtime_club==kNoTeam+2&&view.manager_slot==1&&view.squad_capacity==24&&view.players.size()==2&&view.players[0].player_id==2,"national training borrowed a primary/club squad range");
    ui::TrainingRequest request;request.kind=ui::TrainingRequestKind::set_player_focus;request.manager_slot=1;request.runtime_club=kNoTeam+2;request.revision=7;request.index=0;request.player_id=2;request.value=1;request.desired_types=view.types;request.desired_areas=view.areas;request.desired_automation=view.automation;uint64_t revision=7;std::string error;
    check(app::apply_training_request_source(game,rules,request,revision,error)&&revision==8,"actual secondary national player focus was rejected");check(game.tactics[0x840+0xa]==6&&std::equal(game.tactics.begin(),game.tactics.begin()+0x840,before.tactics.begin())&&game.squad_slots==before.squad_slots,"national training changed another coach or nominee membership");
    const auto accepted=game.tactics;request.revision=revision;request.player_id=0;check(!app::apply_training_request_source(game,rules,request,revision,error)&&game.tactics==accepted&&revision==8,"national training accepted another coach's player reference");
    auto legacy=before;legacy.projected_national_teams.clear();bool rejected=false;try{(void)ui::training_from_game(legacy,rules,7);}catch(const FormatError&){rejected=true;}check(rejected,"projected national training silently changed legacy national admission");std::cout<<"Authored two-national actual ranges, secondary coach player edit, stale/foreign actor rejection and unchanged legacy admission passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
