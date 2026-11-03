// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/identity.hpp"
#include "game/random.hpp"
#include <array>
#include <optional>
#include <map>
#include <vector>
namespace bmh {class LeImage;}
namespace bmh::world12 {
struct World;struct CareerSnapshot;struct SeasonCalendar;struct OtherAppointment;
struct RegionalPlayoffRules {
    uint8_t flags=0;
    bool operator==(const RegionalPlayoffRules&)const=default;
};
struct RegionalPlayoffSeason {
    uint32_t season=0;
    RegionalPlayoffRules rules;
    std::array<uint16_t,6> dates{};
    std::optional<uint16_t> activation_day;
    std::array<ClubId,4> entrants;
    std::map<ClubId,uint8_t> activation_deductions;
    uint8_t played=0;
    uint64_t revision=0;
    std::array<uint16_t,12> scores{};
    // Original annual promotion reads entry35 +24 before +22: runner-up, winner.
    std::array<ClubId,2> qualifiers;
    bool complete()const{return played==6;}
    bool operator==(const RegionalPlayoffSeason&)const=default;
};
struct RegionalPlayoffGroup {
    uint16_t day=0;uint32_t season=0;
    uint8_t played=0,flags=0;
    uint64_t revision=0;
    std::vector<Fixture> matches;
    bool operator==(const RegionalPlayoffGroup&)const=default;
};
struct RegionalPlayoffResults {
    RegionalPlayoffGroup group;
    std::vector<uint16_t> scores;
    bool operator==(const RegionalPlayoffResults&)const=default;
};
bool regional_playoff_appointment(const OtherAppointment&);
RegionalPlayoffRules regional_playoff_rules(const LeImage&);
// Known inactive owner for a fresh season; no field selection or RNG occurs.
RegionalPlayoffSeason make_regional_playoff_season(const World&,const LeImage&,const SeasonCalendar&,uint32_t season);
void validate_regional_playoff(const World&);
void validate_regional_playoff_snapshot(const CareerSnapshot&);
// Called within the day transaction after writing real league results, before
// advancing the date. Both Regionalligen must cross their actual final round.
bool activate_regional_playoff(CareerSnapshot& staged,const CareerSnapshot& before);
std::optional<RegionalPlayoffGroup> prepare_regional_playoff_group(const CareerSnapshot&);
// Staged result bookkeeping; the outer day transaction publishes its date and
// revision together. Original AdvanceCup13 owns ranking and both qualifier words.
void commit_regional_playoff_results(CareerSnapshot&,const RegionalPlayoffResults&,const LeImage&,WatcomRandom&);
std::array<ClubId,2> annual_regional_qualifiers(const CareerSnapshot&);
}
