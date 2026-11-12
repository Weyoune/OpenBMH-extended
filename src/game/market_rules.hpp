// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/random.hpp"
#include "game/lineup.hpp"
#include <array>
#include <optional>
#include <stdexcept>
#include <vector>
namespace bmh {
inline std::array<uint8_t,4> allocate_market_entries(std::array<uint8_t,5> count,WatcomRandom& random) {
    std::array<uint8_t,4> add{};const int shares[]={47,35,10,8};const int r=random.range(2,22);uint8_t n=uint8_t(r);
    if(count[0]+int8_t(r)>=38){n=uint8_t(37-count[0]);if(int8_t(n)<=0)return add;}
    count[1]=uint8_t(count[1]-count[3]);count[2]=uint8_t(count[2]-count[4]);
    for(int16_t u=count[1];n&&u>54;--u){--n;++add[0];}
    for(int16_t u=count[2];n&&u>14;--u){--n;++add[2];}
    for(int k=0;k<4;++k)add[k]=uint8_t(add[k]+int8_t(shares[k]*int8_t(n)/100));
    if(count[1]<add[0]){add[1]=uint8_t(add[1]+((add[0]-count[1])>>1));add[0]=count[1];}
    if(count[2]<add[2]){add[3]=uint8_t(add[3]+((add[2]-count[2])>>1));add[2]=count[2];}
    int8_t total=0;for(auto v:add)total=int8_t(total+v);if(total<4)add[1]=uint8_t(add[1]+random.range(2,4));
    if(!add[3]&&add[1]&&random.range(0,10)==5){++add[3];--add[1];}return add;
}
// players: the squad including players away on loan (the count the selection keys use); occupied: the players
// in the squad slots, among whom the row is drawn (-1: the same number).
struct MarketClubShape {int players=0,capacity=0;std::array<int8_t,4> positions{};uint16_t flags=0;int occupied=-1;};
struct MarketPlayerChoice {int position=0;bool taken=false,national=false;};
struct MarketClubPlayer {int club=-1,row=-1;};
// Original alternating scan, score arithmetic/quicksort, 35 tries and final
// attempted-row behavior. Callbacks keep packed/native and global IDs separate.
template<class Human,class Describe,class Player>
std::optional<MarketClubPlayer> select_market_club_player(int first,int last,int big,int total,int& toggle,
    WatcomRandom& random,Human human,Describe describe,Player player) {
    const int8_t big_wanted[]={2,6,7,5},small_wanted[]={2,4,5,4};const int8_t* wanted=first<big?big_wanted:small_wanted;
    for(;;){std::vector<int16_t> teams;std::vector<uint32_t> keys;toggle=toggle+1==2?0:toggle+1;int best_line=0;
        for(int team=first+toggle;team<last;team+=2){if(human(team))continue;const auto shape=describe(team);int key=0,line=0;bool add=false;
            if(random.range(0,1)+2<shape.positions[0]){key=shape.positions[0]+100;add=true;}
            else if(shape.players>13 && random.range(0,int((shape.flags>>12)&3)*2+2)!=0){int8_t most=0;
                for(int p=1;p<4;++p){const int8_t over=int8_t(shape.positions[p]-wanted[p]);if(most<over){most=over;best_line=p;}}
                line=most==0?random.range(1,3):best_line;key=most*8;const int8_t room=int8_t(shape.capacity-((team<big?1:0)+2));if(room<shape.players)key+=(shape.players-room)*4;add=true;}
            if(add){const int a=random.range(0,5),b=random.range(0,5);keys.push_back(uint32_t((key+a-b)*4+int8_t(line)));teams.push_back(int16_t(team));}
        }
        if(!teams.empty()){quicksort_by_key(teams.data(),keys.data(),int(teams.size()));MarketClubPlayer selected;
            for(size_t i=0;i<teams.size();++i){const int team=teams[i];const auto picked=describe(team);const int count=picked.occupied>=0?picked.occupied:picked.players,line=int(keys[i]&3);int tries=0;bool again;
                do{selected={team,random.range(0,count-1)};const auto p=player(team,selected.row);++tries;again=p.position!=line||p.national||p.taken;}while(tries<35&&again);
                if(tries<35)return selected;
            }
            if(int(teams.size())<total)return selected;
        }
        if(first==0)return {};
        first=0;last=big;
    }
}
template<class Swap>void shuffle_initial_positions(int first,int count,WatcomRandom& random,Swap swap) {
    for(int i=0;i<count;++i){int other;do{other=random.range(first,first+count-1);}while(uint8_t(other)==uint8_t(first+i));
        if(other<first||other>=first+count)throw std::invalid_argument("season: invalid fixture-position draw");
        swap(i,other-first);}
}
}
