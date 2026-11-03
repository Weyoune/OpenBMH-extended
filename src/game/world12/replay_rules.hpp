// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/cups.hpp"
#include "game/cup.hpp"
namespace bmh::world12 {
inline uint8_t replay_source_entry(const LeImage& source){
    std::optional<uint8_t> result;
    for(uint8_t entry=0;entry<kCupEntries;++entry){const auto at=cup_entry_address(entry);if(source.contains(at+0x27)&&source.u16(at+0x18)==9){if(result)throw FormatError("world12: ambiguous Source replay programme");result=entry;}}
    if(!result)throw FormatError("world12: Source replay programme is missing");return *result;
}
}
