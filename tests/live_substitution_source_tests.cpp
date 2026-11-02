// SPDX-License-Identifier: GPL-3.0-only
#include "app/live_substitution_source.hpp"
#include <iostream>
#include <stdexcept>
using namespace bmh;
namespace {
void check(bool good,const char* error){if(!good)throw std::runtime_error(error);}
GameState fixture(){GameState g;g.manager_teams={0};g.manager_records.resize(0xb0c);g.team_flags={0};g.squad_slots.resize(24);g.players.resize(24);g.player_bonus.resize(24);g.player_cards.resize(24);g.tactics.resize(0x840);
    for(int i=0;i<24;++i){g.squad_slots[i]=uint16_t(i|(i<11?0x8000:0x4000));g.players[i].name="Player "+std::to_string(i);g.tactics[i*0x58]=uint8_t(i);g.tactics[i*0x58+0x41]=uint8_t(i%20);g.tactics[i*0x58+0x42]=uint8_t(i%9);g.player_cards[i]=uint16_t(i*13);}return g;}
ui::LineupRequest request(const GameState& g,int first=3,int second=11){ui::LineupRequest r;r.kind=ui::LineupRequestKind::swap_players;r.manager_slot=0;r.runtime_club=0;r.first_row=first;r.second_row=second;r.first_slot=g.squad_slots[first];r.second_slot=g.squad_slots[second];r.first_player=r.first_slot&0xfff;r.second_player=r.second_slot&0xfff;return r;}
void unchanged(const GameState& a,const GameState& b){check(a.squad_slots==b.squad_slots&&a.team_flags==b.team_flags&&a.tactics==b.tactics&&a.player_cards==b.player_cards,"rejected Source substitution changed owned rows");for(size_t i=0;i<a.players.size();++i)check(a.players[i].w35c==b.players[i].w35c&&a.players[i].w364==b.players[i].w364&&a.players[i].w368==b.players[i].w368,"rejected Source substitution changed player");}
}
int main()try{
    std::string error;const auto none=[](int){return false;};
    auto g=fixture();const auto before=g;auto r=request(g);check(app::apply_live_substitution_source(g,r,17,3,false,3,none,error),"valid field/bench substitution rejected");
    check(g.squad_slots[3]==uint16_t(11|0x8000)&&g.squad_slots[11]==uint16_t(3|0x4000)&&((g.team_flags[0]>>5)&7)==1&&(g.players[11].w364&0x1000),"Source substitution did not retain row flags/count/played marker");
    check(g.tactics[3*0x58]==17&&g.tactics[3*0x58+0x41]==before.tactics[3*0x58+0x41]&&g.tactics[11*0x58+0x42]==before.tactics[11*0x58+0x42]&&g.player_cards==before.player_cards,"Source substitution lost entry minute/position or remapped cards");
    auto accepted=g;check(!app::apply_live_substitution_source(g,r,17,3,false,3,none,error),"stale row slots accepted");unchanged(g,accepted);
    for(const auto pair:{std::pair{3,5},std::pair{12,14}}){auto same=fixture();const auto initial=same;auto move=request(same,pair.first,pair.second);check(app::apply_live_substitution_source(same,move,22,3,false,3,none,error),"same-zone tactical exchange rejected");check(same.team_flags==initial.team_flags&&same.player_cards==initial.player_cards&&same.tactics[pair.first*0x58+0x41]==initial.tactics[pair.first*0x58+0x41]&&(same.squad_slots[pair.first]&0xfff)==pair.second,"same-zone exchange became counted substitution or lost coordinates");}
    for(int kind=0;kind<5;++kind){auto bad=fixture();r=request(bad);const char* notice=nullptr;
        if(kind==0){bad.team_flags[0]=3<<5;notice="source_notice:0xaf150";}
        if(kind==1){bad.players[11].w364|=0x1000;notice="source_notice:0xaf14c";}
        if(kind==2){bad.players[11].w364|=1u<<16;notice="source_notice:0xaf13c";}
        if(kind==3){r.first_player=999;}
        if(kind==4){r.manager_slot=1;}
        const auto old=bad;check(!app::apply_live_substitution_source(bad,r,20,3,false,3,none,error),"invalid substitution accepted");if(notice)check(error==notice,"Source substitution notice changed");unchanged(bad,old);
    }
    auto foreign=fixture();r=request(foreign,4,11);const auto is_foreign=[](int p){return p==1||p==2||p==11;};auto old=foreign;check(!app::apply_live_substitution_source(foreign,r,30,3,true,2,is_foreign,error)&&error=="source_notice:0xaf148","English two-foreign-player limit was treated as German three");unchanged(foreign,old);check(app::apply_live_substitution_source(foreign,r,30,3,true,3,is_foreign,error),"German three-foreign-player threshold changed");
    std::cout<<"Shared Original live substitution: roles, minutes, tactical rows, counts, cards, notices, stale rollback and country limits passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
