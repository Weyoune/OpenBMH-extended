// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/save.hpp"
#include "game/messages.hpp"
#include "formats/textd.hpp"
namespace bmh::world12 {
struct WeeklyPlayerNotice {
    uint32_t recipients=0;
    uint16_t date=0;
    std::array<uint8_t,kMailboxText> text{};
    bool operator==(const WeeklyPlayerNotice&)const=default;
};
struct WeeklyPlayerOutcome {
    uint64_t game_draws=0,watcom_draws=0;
    size_t roster_slots=0,injured=0,recovered=0,retired=0;
    std::vector<WeeklyPlayerNotice> notices;
};
struct TrainingOutcome {
    uint64_t game_draws=0,watcom_draws=0;
    size_t managers=0;
};
// These are ordered service phases, not a replacement for the daily coordinator.
// The caller retains notices and owns weekly placement between market processing
// and manager settlement. No calendar cursor or desk order is advanced here.
WeeklyPlayerOutcome update_weekly_players(CareerSnapshot&,const GameState& reference,const TextDb&);
TrainingOutcome train_manager_squads(CareerSnapshot&,const GameState& reference);
}
