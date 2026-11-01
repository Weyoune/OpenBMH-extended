// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/economy.hpp"
#include "game/weekly.hpp"
#include "formats/textd.hpp"
namespace bmh::world12 {
struct WeeklyClubRelations {
    ClubId club;
    ClubAttractionTitles advertising;
};
struct WeeklyIncidentContext {
    uint32_t date=0,season=0;
    // Current competition ownership, not historic starting fields. Exactly one
    // qualified relation record for every active human club is required.
    std::vector<WeeklyClubRelations> clubs;
};
struct WeeklyIncidentHooks {
    std::function<std::optional<bool>(const GameState&,uint8_t global_seat,const WeeklyOfferQuestion&)> answer;
};
struct WeeklyIncidentOutcome {
    uint64_t game_draws=0,watcom_draws=0;
    size_t managers=0,questions=0;
};
WeeklyIncidentOutcome begin_weekly_managers(CareerSnapshot&,const GameState& reference,
    const TextDb&,const WeeklyIncidentContext&,const WeeklyIncidentHooks&);
}
