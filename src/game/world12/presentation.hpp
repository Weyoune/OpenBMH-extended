// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/progression.hpp"
namespace bmh::world12 {
std::vector<std::string> matchday_competition_labels(const CareerSnapshot&,const DayTicket&);
struct TitlePresentation {ClubId club;uint8_t artwork=0;std::string competition;};
std::vector<TitlePresentation> newly_completed_titles(const CareerSnapshot&,const CareerSnapshot&,const LeImage&,const LeImage&);
}
