// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/world.hpp"
#include "game/state.hpp"
#include <algorithm>
#include <bit>
namespace bmh::world12 {
inline ManagerBalancePlan balance_plan_from_game(const GameState& game,int manager) {
    if(manager<0||manager>3||game.manager_records.size()<size_t(manager+1)*0xb0c)throw FormatError("world12: balance plan has no manager record");
    const auto base=size_t(manager)*0xb0c;const auto read=[&](size_t at){uint32_t value=0;for(size_t i=0;i<4;++i)value|=uint32_t(game.manager_records.at(base+at+i))<<(i*8);return std::bit_cast<int32_t>(value);};
    ManagerBalancePlan plan;for(size_t i=0;i<18;++i)plan.adjustments[i]=read(0x2b0+i*4);
    plan.total=read(0x308);plan.other_home_games=read(0x2f8);plan.average_gate=read(0x2fc);
    plan.view=uint8_t(std::clamp(int(int8_t(game.manager_records.at(base+0x310))),0,2));
    const auto ticket=layout::PlanTicketPrice+uint32_t(manager);if(game.memory.has(ticket))plan.ticket_price=game.memory.u8(ticket);return plan;
}
inline void install_balance_plan(GameState& game,int manager,const ManagerBalancePlan& plan) {
    if(manager<0||manager>3||game.manager_records.size()<size_t(manager+1)*0xb0c)throw FormatError("world12: balance plan has no manager workspace");
    const auto base=size_t(manager)*0xb0c;const auto put=[&](size_t at,int32_t value){const auto raw=std::bit_cast<uint32_t>(value);for(size_t i=0;i<4;++i)game.manager_records.at(base+at+i)=uint8_t(raw>>(i*8));};
    for(size_t i=0;i<18;++i)put(0x2b0+i*4,plan.adjustments[i]);put(0x308,plan.total);put(0x2f8,plan.other_home_games);put(0x2fc,plan.average_gate);
    // The shared view word belongs to ManagerFinance.baseline. Only the
    // explicit planning close updates both canonical representations; ordinary
    // economy installation must not normalize that separate raw record.
    game.memory.add_block(layout::PlanTicketPrice+uint32_t(manager),1);game.memory.set8(layout::PlanTicketPrice+uint32_t(manager),plan.ticket_price);
}
}
