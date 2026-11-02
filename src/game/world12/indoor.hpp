// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/identity.hpp"
#include "game/random.hpp"
#include <array>
#include <optional>
#include <string>
#include <vector>
namespace bmh {class LeImage;struct GameState;}
namespace bmh::world12 {
struct World;struct CareerSnapshot;struct OtherAppointment;
struct IndoorStageRules {
    uint8_t entry=0,flags=0,previous=255,next=0,parallel=0,teams=0,rounds=0,groups=0,group_index=0,quota=0;
    uint8_t team_offset=0,result_offset=0;
    bool operator==(const IndoorStageRules&)const=default;
};
struct IndoorTournament {
    uint8_t ordinal=0,strength=0;
    ClubId host;
    std::string name;
    std::array<ClubId,16> teams;
    std::array<uint16_t,16> scores{};
    std::array<uint8_t,5> played{};
    std::array<ClubId,4> places;
    bool complete=false;
    bool operator==(const IndoorTournament&)const=default;
};
struct IndoorDatePass {
    uint16_t day=0;uint8_t completed=0,total=0;
    bool ordinary_completed=false;
    bool operator==(const IndoorDatePass&)const=default;
};
struct IndoorSeason {
    uint64_t revision=0;
    std::array<IndoorStageRules,5> rules;
    std::array<ClubId,3> previous_holders;
    // Original BSS scratch positions are resolved when the physical selection
    // order becomes owned. They are never treated as global club identities.
    std::array<uint16_t,4> initial_scratch_slots{};
    std::array<ClubId,4> masters_scratch;
    std::vector<ClubId> selection_slots;
    std::array<uint16_t,40> invitations{},points{};
    std::array<ClubId,5> hosts;
    std::vector<IndoorTournament> tournaments;
    std::optional<IndoorDatePass> date_pass;
    ClubId masters_winner;
    bool operator==(const IndoorSeason&)const=default;
};
struct IndoorGroup {
    uint64_t revision=0;
    uint8_t tournament=0,source_entry=0,played=0,flags=0;
    std::vector<Fixture> matches;
    bool operator==(const IndoorGroup&)const=default;
};
struct IndoorGroupScores {uint8_t source_entry=0,played=0;std::vector<uint16_t> scores;bool operator==(const IndoorGroupScores&)const=default;};
struct IndoorResults {uint64_t revision=0;uint8_t tournament=0;std::vector<IndoorGroupScores> groups;bool operator==(const IndoorResults&)const=default;};
bool german_indoor_appointment(const OtherAppointment&);
std::array<IndoorStageRules,5> indoor_programme_rules(const LeImage&);
IndoorSeason initial_indoor_season(const World&,const LeImage&,const GameState&);
void validate_indoor_season(const World&);
std::vector<IndoorGroup> prepare_indoor_groups(const IndoorSeason&);
// Original first draw at the first scheduled event after Bundesliga round17.
// changed stays false when no draw is due; no RNG or finance then changes.
struct IndoorTransitionOutcome {bool changed=false;uint64_t game_draws=0,watcom_draws=0;};
IndoorTransitionOutcome prepare_indoor_draw(CareerSnapshot&,const GameState&,const LeImage&);
// Source group qualification, points, invitations, next draws and Masters awards
// publish together. Scoring and the calendar's3/2 date passes have separate owners.
IndoorTransitionOutcome commit_indoor_results(CareerSnapshot&,const IndoorResults&,const GameState&,const LeImage&);
}
