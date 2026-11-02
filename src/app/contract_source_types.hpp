// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <array>
#include <cstdint>
namespace bmh::app {
// Runtime-only numeric workspace identity. Typed callers retain a separate
// canonical club/player binding and remap before each Core operation.
struct ContractSourceQuote {
    uint64_t quote_id=0,request_id=0,revision=0;
    int manager_slot=-1,runtime_club=-1,row=-1,player_id=-1;
    std::array<uint32_t,4> demands{};
    bool operator==(const ContractSourceQuote&)const=default;
};
}
