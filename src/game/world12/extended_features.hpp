// SPDX-License-Identifier: GPL-3.0-only
// State of the features OpenBMH-Extended adds to the shared world and the original game never had:
//   - two additional desk icons (Almanach, Wechsel) that live outside the 54 original layout records,
//   - the almanac: final league tables and top scorers of every completed season,
//   - which club a human manager led in which season,
//   - the job offers a manager holds at the first desk of a new season.
// Everything here is one optional member of World (schema 45). A world of an older schema has none of it:
// the desk icons then stand at their default places and earlier seasons are simply unknown.
// The JSON form is in extended_features.cpp; the rules that fill it are in almanac.cpp and job_offers.cpp.
#pragma once
#include "cli/json.hpp"
#include "game/world12/identity.hpp"
#include "game/world12/sorted_map.hpp"
#include <array>
#include <optional>
#include <string>
#include <vector>
namespace bmh::world12 {
struct World;

// Upper left corner of an additional desk icon on the 320x240 desk. The icons are 32x25 like the original ones.
struct DeskIconPlace {
    uint16_t x=0;
    uint8_t y=0;
    bool operator==(const DeskIconPlace&)const=default;
};
inline constexpr int kDeskIconAlmanac=0,kDeskIconChange=1;
// Default places: the only free area of the original desk layout, below the lowest icon row.
inline constexpr std::array<DeskIconPlace,2> kDefaultDeskIcons{{{283,203},{249,203}}};

// One row of a final league table, in final order.
struct AlmanacTableRow {
    ClubId club;
    uint8_t games=0,wins=0,draws=0,defeats=0,goals_for=0,goals_against=0,points=0;
    bool operator==(const AlmanacTableRow&)const=default;
};
// A scorer of the season. The name is kept as text (original character set): the player may retire or be
// replaced later, and the almanac must still name him.
struct AlmanacScorer {
    std::string name;
    ClubId club;
    uint8_t goals=0;
    bool operator==(const AlmanacScorer&)const=default;
};
struct AlmanacLeagueSeason {
    LeagueId league;
    std::vector<AlmanacTableRow> table;   // every club of the league, champion first
    std::vector<AlmanacScorer> scorers;   // at most kAlmanacScorers, best first; empty if nobody scored
    bool operator==(const AlmanacLeagueSeason&)const=default;
};
inline constexpr size_t kAlmanacScorers=5;
struct AlmanacSeason {
    uint32_t season=0;
    std::vector<AlmanacLeagueSeason> leagues;   // the active leagues, in catalogue order
    bool operator==(const AlmanacSeason&)const=default;
};
// The club and league a manager led during one season (recorded when the season is closed).
struct ManagerClubSeason {
    uint32_t season=0;
    ClubId club;
    LeagueId league;
    bool operator==(const ManagerClubSeason&)const=default;
};
struct JobOffer {
    ClubId club;
    bool operator==(const JobOffer&)const=default;
};
inline constexpr size_t kMaximumJobOffers=3;

struct ExtendedFeatures {
    // Seat -> places of the two icons (index kDeskIconAlmanac, kDeskIconChange). A seat without an entry
    // uses kDefaultDeskIcons.
    SortedMap<uint8_t,std::array<DeskIconPlace,2>> desk_icons;
    // Completed seasons in ascending order without gaps from the first recorded one.
    std::vector<AlmanacSeason> almanac;
    // Seat -> seasons in ascending order.
    SortedMap<uint8_t,std::vector<ManagerClubSeason>> manager_clubs;
    // Offers made at the change into season job_offer_season. They can only be accepted at the first desk
    // of that season and are removed when the first day passes.
    uint32_t job_offer_season=0;
    SortedMap<uint8_t,std::vector<JobOffer>> job_offers;
    bool operator==(const ExtendedFeatures&)const=default;
};

// Places of the additional icons of a seat (the defaults if the world or the seat has none).
std::array<DeskIconPlace,2> desk_icon_places(const World&,uint8_t seat);
// Throws FormatError if the record contradicts the world (unknown clubs, leagues, seats, places off the desk).
void validate_extended_features(const World&);
cli::Json encode_extended_features(const std::optional<ExtendedFeatures>&);
std::optional<ExtendedFeatures> decode_extended_features(const cli::Json&);
}
