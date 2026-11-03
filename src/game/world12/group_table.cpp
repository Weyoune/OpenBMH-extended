// SPDX-License-Identifier: GPL-3.0-only
#include "game/world12/group_table.hpp"
#include "game/cup.hpp"
#include "game/state.hpp"
#include <set>
namespace bmh::world12 {
SourceGroupTable project_source_group_table(const std::vector<ClubId>& clubs,uint8_t flags,uint8_t rounds,uint8_t played,const std::vector<uint16_t>& scores){
    uint32_t fixture_table=0;switch(clubs.size()){
    case 4:fixture_table=layout::CareerSession_RuntimeError_RuntimeField;break;
    case 6:fixture_table=layout::CareerSession_RuntimeError_RuntimeField2;break;
    case 12:fixture_table=layout::CareerSession_RuntimeError_RuntimeField3;break;
    case 16:fixture_table=layout::CareerSession_RuntimeError_ExecutableField3;break;
    case 18:fixture_table=layout::CareerSession_RuntimeError_ExecutableField;break;
    case 20:fixture_table=layout::CareerSession_RuntimeError_ExecutableField2;break;
    default:throw FormatError("world12: unsupported actual Source group fixture size");}
    if((flags&4)||!rounds||played>rounds||scores.size()!=size_t(rounds)*clubs.size()/2||rounds%((flags&1)+1))throw FormatError("world12: invalid Source group table dimensions");
    std::set<ClubId> unique;for(auto club:clubs)if(!club.valid()||!unique.insert(club).second)throw FormatError("world12: group table has missing or repeated canonical clubs");
    for(auto score:scores)if(score!=0xcccc&&(score>>10))throw FormatError("world12: group standings received an invalid score phase");
    GameState source;const auto entry=cup_entry_address(0);source.memory.add_block(entry,0x28);source.memory.set_pointer(entry+8,fixture_table);source.memory.set8(entry+0x14,flags);source.memory.set8(entry+0x1a,uint8_t(clubs.size()));source.memory.set8(entry+0x1b,rounds);source.memory.set8(entry+0x1d,played);
    constexpr uint32_t results=0x1000000;source.memory.add_block(results,scores.size()*2);source.memory.set_pointer(entry+12,results);for(size_t i=0;i<scores.size();++i)source.memory.set16(results+uint32_t(i*2),scores[i]);
    return {clubs,bmh::cup_group_table(source.memory,entry)};
}
}
