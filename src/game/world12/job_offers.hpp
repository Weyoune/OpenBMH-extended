// SPDX-License-Identifier: GPL-3.0-only
// Job offers: a rule of OpenBMH-Extended. The original game has no job offers, no dismissal and no change
// of club; this rule is not derived from it.
//
// When a season is closed, every human club manager is judged by one number:
//     performance = expected place - final place
// where the expected place is the rank of the club's team strength within its league (strongest = 1).
// From that follows one of three classes:
//   - none:     point deduction pending for the new season, relegated, or performance <= -6
//   - top:      champion, promoted, domestic cup winner, or performance >= +4
//   - ordinary: everything else
// Offers come only from clubs without a human manager in leagues a manager may lead:
//   - top:      up to three offers. Up to two come from "above": from the weaker half of the next higher
//               division, or - in the highest division - from stronger clubs of the own division.
//               The rest are peers (see below).
//   - ordinary: one or two offers from peers: clubs of the own division (after promotion and relegation)
//               whose strength rank lies within four places of the own club's.
//   - With both countries in the game, in one season out of three the last offer is replaced by a club of
//     the other country at the same level of its league pyramid and of similar strength.
// No club is offered to two managers. The choice among equally suitable clubs is made by a fixed mixing
// function of season, seat and club: it draws nothing from the two random streams of the game, so the
// course of a career does not depend on whether the rule exists.
// OpenBMH deviates from the original: at the change of season a human club manager can receive job offers of other clubs, judged by his final place against the strength of his squad.
#pragma once
#include "game/world12/world.hpp"
namespace bmh::world12 {
// Team strength as the rule sees it: the mean of form, technique and condition of the eleven best players
// of the squad (0..127). A club without a squad (a foreign club) has its stored rating instead.
int club_strength(const World&,ClubId);
// Level of a league in the pyramid of its country (0 = highest). Leagues of the same level share a number.
uint8_t league_level(const World&,LeagueId);
// The offers lapse when the first desk round of the season is over.
inline void expire_job_offers(World& world){if(world.extended)world.extended->job_offers.clear();}
struct JobOfferFacts {
    uint8_t seat=0;
    ClubId club;
    int expected_place=0,final_place=0;   // 1-based
    bool champion=false,cup_winner=false,promoted=false,relegated=false,sanctioned=false;
    enum class Class:uint8_t { none,ordinary,top } offer_class=Class::none;
    bool operator==(const JobOfferFacts&)const=default;
};
// The facts the rule sees for one manager. `closing` is the world of the completed season (strengths and
// league membership as played), `table` its almanac record, `titles` its title rows, `opening` the world
// after promotion and relegation, `sanctioned` whether the manager starts the new season with a deduction.
JobOfferFacts judge_manager(const World& closing,const AlmanacSeason& table,const AnnualSeasonTitles& titles,
    const World& opening,uint8_t seat,bool sanctioned);
// The offers of all human club managers for the season `target`, by seat. Seats without an offer are absent.
SortedMap<uint8_t,std::vector<JobOffer>> make_job_offers(const World& closing,const AlmanacSeason& table,
    const AnnualSeasonTitles& titles,const World& opening,uint32_t target,const std::array<bool,4>& sanctioned);
}
