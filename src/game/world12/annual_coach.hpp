// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/world.hpp"
namespace bmh::world12 {
// Core season_end_day hands a club coach back before awards. Canonical club and
// national financial records already have distinct owners; retain both and the
// actual nominees. Unknown old role history stays unknown, reported as false.
// The no-club career's continuous national role belongs to its separate owner.
inline bool close_annual_national_roles(World& world) {
    world.validate();if(!world.catalogue.scope.international)return true;
    if(!world.catalogue.scope.club_play())throw FormatError("world12: national-only annual role needs its continuous owner");
    if(!world.national_coaches)return false;
    auto candidate=world;
    for(auto& record:*candidate.national_coaches){record.coach.reset();record.active=false;for(auto& squad:candidate.national_squads)if(squad.team==record.team)squad.coach.reset();}
    candidate.validate();world=std::move(candidate);return true;
}
}
