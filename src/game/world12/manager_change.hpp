// SPDX-License-Identifier: GPL-3.0-only
// A human manager changes club by accepting a job offer (job_offers.hpp). A rule of OpenBMH-Extended;
// the original game binds a manager to one club for the whole career.
//
// The change is a new start at the new club, exactly as for a manager who joins a running career:
// funds of the division, the club's facilities, contracts with the squad, fresh tactics.
// What stays with the person: seat, name, picture, desk layout, mailbox and pools coupon, and the career
// record (places of past seasons, title counters, player awards, best series) - so the highscore keeps
// counting the whole career.
// What is given up: cash, savings, credits, shares and funds, stadium, sponsors, youth teams, season
// tickets, the transfer history and every listing, bid and booked friendly of the old club. The old club
// is led by the computer from then on and keeps its squad.
// OpenBMH deviates from the original: a manager may accept a job offer at the first desk of a season and starts anew at that club, keeping his career record.
#pragma once
#include "game/world12/manager_join.hpp"
namespace bmh::world12 {
struct ManagerChangeOutcome {ClubId from,to;uint64_t watcom_draws=0;};
// Why the manager at the desk cannot accept an offer right now; empty if he can. The texts are German
// because the desk shows them.
std::string manager_change_refusal(const CareerSnapshot&,uint8_t seat);
// Moves the manager of `seat` to `club`. Requires his turn at the first desk of a season, an offer of that
// club (World::extended->job_offers) and no refusal above; throws FormatError otherwise and changes nothing.
// `country_rules` is the rule image of the new club's country.
ManagerChangeOutcome change_manager_club(CareerSnapshot&,uint8_t seat,ClubId club,
    const GameState& reference,const LeImage& country_rules);
}
