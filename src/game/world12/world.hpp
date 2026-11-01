// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/catalogue.hpp"
#include "formats/m3d.hpp"
#include <map>
#include <optional>
namespace bmh::world12 {
struct PlayerRecord {
    PlayerId id;Player attributes;uint8_t bonus=0;uint16_t cards=0;
    bool operator==(const PlayerRecord& p)const{return id==p.id&&attributes.name==p.attributes.name&&attributes.w35c==p.attributes.w35c&&attributes.w368==p.attributes.w368&&attributes.w364==p.attributes.w364&&bonus==p.bonus&&cards==p.cards;}
};
enum class ClubKind:uint8_t { club,national_team };
struct ClubSeasonStats {
    std::array<uint8_t,42> counters{};
    std::array<ClubId,8> opponents;
    std::array<uint16_t,8> dates{};
    bool operator==(const ClubSeasonStats&)const=default;
};
struct ClubRecord {
    ClubId id;std::string name;Country country=Country::germany;ClubKind kind=ClubKind::club;
    LeagueId league;CrestId crest;uint8_t kit=0,rating=0,short_span=0;
    uint16_t match_flags=0,fairplay_points=0;uint8_t morale=25;bool lineup_indoor=false;
    ClubSeasonStats season_stats;
    std::array<RosterSlot,24> roster;
    bool operator==(const ClubRecord&)const=default;
};
struct LeagueMembers {LeagueId league;std::vector<ClubId> clubs;bool operator==(const LeagueMembers&)const=default;};
struct Loan {
    PlayerId player;ClubId owner,borrower;uint8_t months=0;
    bool operator==(const Loan&)const=default;
};
struct ManagerFinanceBaseline {
    uint32_t starting_balance=0,account_flags=0;
    bool operator==(const ManagerFinanceBaseline&)const=default;
};
struct ManagerFinance {
    int32_t balance=0;
    uint32_t credit=0,income=0,expenses=0,bonus_paid=0;
    std::optional<ManagerFinanceBaseline> baseline={};
    bool operator==(const ManagerFinance&)const=default;
};
struct ManagerTacticalRow {
    std::array<uint8_t,4> orders{};
    uint8_t partner=255,depth=0,lane=0;
    bool operator==(const ManagerTacticalRow&)const=default;
};
// Tactical records contain numeric state and two explicit player references.
// Reference words and the separately named row fields are zero in scalar_rows.
struct ManagerTacticalBacking {
    std::array<std::array<uint8_t,0x58>,24> scalar_rows{};
    std::array<PlayerId,24> marks,marked_by;
    std::array<uint8_t,0x160> formations{};
    std::array<uint8_t,13> formation_tail{}; // Original bench-coordinate reads beyond the manager table.
    std::array<uint8_t,2> enabled_formations{};
    std::array<uint8_t,4> automation{};
    bool operator==(const ManagerTacticalBacking&)const=default;
};
struct ManagerTactics {
    std::array<ManagerTacticalRow,24> rows;
    uint8_t automatic_weights=0,automatic_formation=0;
    std::optional<ManagerTacticalBacking> backing={};
    bool operator==(const ManagerTactics&)const=default;
};
void validate_manager_tactics(const ManagerTactics&);
struct ManagerMatchState {
    std::array<uint8_t,21> preferences{}; // Source c5..d9, including attitude/fitness.
    uint8_t tax_policy=0,sponsor_win_bonus=0,crowd_pressure=0,goal_notices=0,point_deduction=0;
    std::array<uint8_t,21> best_streaks{};
    std::array<uint8_t,72> timing_stats{};
    std::array<uint16_t,6> staff_ratings{};
    std::array<uint8_t,60> rating_history{};
    std::array<uint8_t,6> recent_scores{};
    std::array<ClubId,12> recent_fixtures;
    std::map<ClubId,std::array<uint8_t,10>> head_to_head;
    bool designated_takers=false;
    std::array<PlayerId,3> takers;
    bool operator==(const ManagerMatchState&)const=default;
};
struct Manager {
    uint8_t seat=0;std::string name;ClubId club;LeagueId start_league;uint8_t portrait=0;
    std::optional<ManagerFinance> finance={};
    std::optional<ManagerTactics> tactics={};
    std::optional<ManagerMatchState> match_state={};
    bool operator==(const Manager&)const=default;
};
struct CrestResource {std::string path;uint8_t cell=0;bool operator==(const CrestResource&)const=default;};
struct CareerMarkets {
    std::array<uint8_t,12*0x5c> stocks{};
    std::array<uint8_t,6*0x1c> funds{};
    uint8_t first_default_name=0;
    bool operator==(const CareerMarkets&)const=default;
};
struct World {
    bool players_initialized=false;
    // Present after the market/mood phase; old saves do not invent these values.
    std::optional<CareerMarkets> markets={};
    int32_t development_balance=0;
    uint8_t trend_rotation=0;
    Catalogue catalogue=Catalogue::dual_classic();
    std::map<PlayerId,PlayerRecord> players;
    std::map<ClubId,ClubRecord> clubs;
    std::array<LeagueMembers,kLeagueSlots> membership;
    std::vector<Loan> loans;
    std::vector<Manager> managers;
    // A crest is a distinct atlas identity, never combined with team flags.
    std::map<CrestId,CrestResource> crests;
    void validate(bool complete=true)const;
    std::vector<ClubId> selectable_clubs(Country country,LeagueId league)const;
    Country manager_country(uint8_t seat)const;
    Country national_coach_country(uint8_t seat)const;
    bool operator==(const World&)const=default;
};
// Filter an immutable imported master world before manager selection. Countries
// are removed as entities, never demoted into an AI/foreign fallback league.
World select_world_scope(const World& source,GameScope);
}
