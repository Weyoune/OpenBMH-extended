// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "world12_fixture.hpp"
namespace bmh::test::world12 {
using namespace bmh;using namespace bmh::world12;
inline LeImage national_rules(){auto source=LeImage::from_bytes(layout::LeagueSizes,Bytes(0x1000));const auto put8=[&](uint32_t at,uint8_t v){source.patch(at,&v,1);};const auto put16=[&](uint32_t at,uint16_t v){const uint8_t bytes[]={uint8_t(v),uint8_t(v>>8)};source.patch(at,bytes,2);};for(int e=0;e<kCupEntries;++e)put16(cup_entry_address(e)+0x18,65535);
 const auto stage=[&](int entry,int type,int flags,int teams,int rounds,int previous,int next,int parallel,int groups,int index,int quota){const auto at=cup_entry_address(entry);put16(at+0x18,uint16_t(type));for(const auto [offset,value]:std::initializer_list<std::pair<int,int>>{{0x14,flags},{0x15,previous},{0x16,next},{0x17,parallel},{0x1a,teams},{0x1b,rounds},{0x1e,groups|(index<<4)},{0x1f,quota}})put8(at+uint32_t(offset),uint8_t(value));};
 for(int group=0;group<6;++group)stage(group+1,4,192,4,3,255,7,group<5?group+2:0,6,group,16);
 stage(7,4,140,16,3,1,8,0,1,0,4);stage(8,4,200,2,1,7,0,9,2,0,4);stage(9,4,200,2,1,7,0,0,2,1,4);
 stage(10,6,192,4,3,255,12,11,2,0,4);stage(11,6,192,4,3,255,12,0,2,1,4);stage(12,6,204,4,2,10,0,0,1,0,3);
 stage(13,5,195,6,10,255,0,0,1,0,2);stage(14,7,195,4,6,255,0,0,1,0,1);return source;}
}
