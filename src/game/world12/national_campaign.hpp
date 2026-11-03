// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/identity.hpp"
#include "game/random.hpp"
#include <array>
#include "game/world12/sorted_map.hpp"
#include <optional>
#include <vector>
namespace bmh {class LeImage;}
namespace bmh::world12 {
struct World;struct OtherAppointment;
struct NationalStageRules {
    uint8_t entry=0,flags=0,previous=255,next=0,parallel=0,teams=0,rounds=0,groups=1,group_index=0,quota=0,seeds=0;
    bool knockout()const{return flags&4;}
    bool operator==(const NationalStageRules&)const=default;
};
struct NationalStage {
    NationalStageRules rules;uint8_t played=0;
    std::vector<ClubId> teams;
    std::array<std::vector<uint16_t>,2> scores;
    ClubId winner,runner_up;
    bool operator==(const NationalStage&)const=default;
};
struct NationalCampaign {
    uint32_t season=0;uint8_t competition=7,active_entry=0;uint64_t revision=0;
    std::vector<ClubId> entrants;std::vector<NationalStage> stages;
    bool complete=false;ClubId winner,runner_up;
    std::array<ClubId,4> places;
    bool operator==(const NationalCampaign&)const=default;
};
struct NationalCampaignHistory {
    Country primary_country=Country::germany;
    SortedMap<uint32_t,NationalCampaign> seasons;
    // The Source's twenty newest World-Cup podiums, newest first.
    std::vector<std::array<ClubId,4>> honours;
    bool operator==(const NationalCampaignHistory&)const=default;
};
struct NationalGroup {
    uint32_t season=0;uint8_t competition=7,source_entry=0,played=0,round=0,leg=0,flags=0;uint64_t revision=0;
    std::vector<Fixture> matches;std::vector<uint16_t> other_leg;
    bool operator==(const NationalGroup&)const=default;
};
struct NationalGroupScores {uint8_t source_entry=0,played=0;std::vector<uint16_t> scores;bool operator==(const NationalGroupScores&)const=default;};
struct NationalResults {uint32_t season=0;uint64_t revision=0;std::vector<NationalGroupScores> groups;bool operator==(const NationalResults&)const=default;};
struct NationalDrawOutcome {uint64_t game_draws=0,watcom_draws=0;};
uint16_t national_calendar_flag(uint32_t season);
bool national_appointment(const OtherAppointment&,uint32_t season);
std::vector<NationalStageRules> national_programme_rules(const LeImage&,uint32_t season);
// New-season only. Season0 reuses the already-owned initial draw when present.
// Subsequent odd years require their exact preceding field/winners.
NationalDrawOutcome prepare_national_campaign(World&,const LeImage&,uint32_t season,GameRandom&,WatcomRandom&,Country primary=Country::germany);
void validate_national_campaigns(const World&);
std::vector<NationalGroup> prepare_national_groups(const NationalCampaign&);
// AdvanceCup owns group ranking, best-third selection, links and all draw RNG.
// Scoring, coach budgets and calendar transactions have distinct owners.
void commit_national_results(World&,const NationalResults&,const LeImage&,WatcomRandom&);
}
