// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/identity.hpp"
#include <optional>
#include "game/world12/sorted_map.hpp"
#include <vector>
namespace bmh {struct GameState;class LeImage;}
namespace bmh::world12 {
struct CareerSnapshot;
struct PlannedFriendly {
    uint64_t id=0;uint16_t day=0;Fixture fixture;bool national=false;
    std::optional<uint8_t> payer;uint32_t cost=0;
    bool operator==(const PlannedFriendly&)const=default;
};
struct PlayedFriendly {
    PlannedFriendly booking;uint16_t score=0;uint8_t end_minute=0;std::optional<uint8_t> kickoff_disposition;
    bool operator==(const PlayedFriendly&)const=default;
};
struct FriendlyPlan {
    uint32_t season=0;uint64_t revision=0,next_id=1;
    SortedMap<ClubId,uint8_t> national_progress; // Actual Source national record byte210, retained across annual plan resets.
    std::vector<PlannedFriendly> pending;std::vector<PlayedFriendly> completed;
    bool operator==(const FriendlyPlan&)const=default;
};
struct FriendlyGroup {
    uint32_t season=0;uint16_t day=0;uint64_t revision=0;bool national=false;uint8_t flags=6;
    std::vector<uint64_t> bookings;std::vector<Fixture> matches;
    bool operator==(const FriendlyGroup&)const=default;
};
struct FriendlyResults {FriendlyGroup group;std::vector<uint16_t> scores;std::vector<uint8_t> end_minutes;std::vector<std::optional<uint8_t>> kickoff_dispositions;bool operator==(const FriendlyResults&)const=default;};
// Only fresh Source initialization or an actual annual calendar reset creates a
// known empty plan. Missing previous planning history never becomes known empty.
inline FriendlyPlan empty_friendly_plan(uint32_t season,const FriendlyPlan* previous=nullptr){FriendlyPlan p;p.season=season;if(previous)p.national_progress=previous->national_progress;return p;}
void initialize_friendly_plan(CareerSnapshot&,const GameState& actual_reference);
bool friendly_appointment(const struct OtherAppointment&);
void validate_friendly_plan(const CareerSnapshot&);
std::vector<FriendlyGroup> prepare_friendly_groups(const CareerSnapshot&);
void commit_friendly_results(CareerSnapshot&,const std::vector<FriendlyResults>&);
uint8_t friendly_source_flags(const LeImage&);
}
