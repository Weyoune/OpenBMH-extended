// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/identity.hpp"
#include <vector>
#include "game/random.hpp"
#include <array>
#include <optional>
namespace bmh { class LeImage; }
namespace bmh::world12 {
struct World;
struct OtherAppointment;
enum class EuropeanCompetition:uint8_t { uefa_cup=1,champions_league=2,cup_winners_cup=3 };
struct EuropeanStageRules {
    uint8_t entry=0,flags=0,previous=255,next=0,parallel=0,teams=0,rounds=0,group_count=1,group_index=0,quota=0,seeds=0;
    bool knockout()const {return flags&4;}
    bool operator==(const EuropeanStageRules&)const=default;
};
struct EuropeanStage {
    EuropeanStageRules rules;
    uint8_t played=0;
    // Source list order, including all knockout rounds. Future slots stay unknown.
    std::vector<ClubId> teams,seeds;
    std::array<std::vector<uint16_t>,2> scores;
    ClubId winner,runner_up;
    bool operator==(const EuropeanStage&)const=default;
};
struct EuropeanCampaign {
    EuropeanCompetition competition=EuropeanCompetition::uefa_cup;
    uint64_t revision=0;
    std::vector<ClubId> entrants;
    std::vector<EuropeanStage> stages;
    uint8_t active_entry=0;
    bool complete=false;
    ClubId winner,runner_up;
    bool operator==(const EuropeanCampaign&)const=default;
};
using EuropeanCampaigns=std::array<EuropeanCampaign,3>;
struct EuropeanGroup {
    EuropeanCompetition competition=EuropeanCompetition::uefa_cup;
    uint8_t source_entry=0,played=0,round=0,leg=0,flags=0;
    uint64_t revision=0;
    std::vector<Fixture> matches;
    std::vector<uint16_t> other_leg;
    bool operator==(const EuropeanGroup&)const=default;
};
struct EuropeanGroupScores {uint8_t source_entry=0,played=0;std::vector<uint16_t> scores;bool operator==(const EuropeanGroupScores&)const=default;};
struct EuropeanResults {
    EuropeanCompetition competition=EuropeanCompetition::uefa_cup;
    uint64_t revision=0;
    std::vector<EuropeanGroupScores> groups;
    bool operator==(const EuropeanResults&)const=default;
};
struct EuropeanClubStatus {
    EuropeanCompetition competition=EuropeanCompetition::uefa_cup;
    bool entered=false,active=false,eliminated=false,holder=false,winner=false;
};
std::optional<std::vector<EuropeanCompetition>> european_cups_on(const OtherAppointment&);
std::vector<EuropeanStageRules> european_programme_rules(const LeImage&,EuropeanCompetition);
EuropeanCampaigns initial_european_campaigns(const World&,const LeImage&);
void validate_european_campaigns(const World&);
std::vector<EuropeanGroup> prepare_european_groups(const EuropeanCampaign&);
std::vector<ClubId> current_european_members(const EuropeanCampaign&);
std::optional<EuropeanClubStatus> european_club_status(const World&,ClubId);
// Core AdvanceCup owns all shuffles, group ranking/qualification and stage links.
// Match scores must already be produced by the match owner; no scores are drawn here.
void commit_european_results(World&,const EuropeanResults&,const LeImage&,WatcomRandom&,uint32_t season=0);
}
