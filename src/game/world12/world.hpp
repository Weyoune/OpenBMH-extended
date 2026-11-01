// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/catalogue.hpp"
#include "formats/m3d.hpp"
#include "game/long_career.hpp"
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
    std::optional<uint16_t> source_slot={};
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
struct ManagerAttendanceState {
    uint8_t league_home_count=0;
    std::array<uint8_t,19> ticket_prices{};
    std::array<uint32_t,19> spectators{},catering{};
    std::array<uint8_t,3> match_counts{};
    std::array<uint32_t,3> spectator_totals{},record_spectators{},record_spectator_dates{},record_income{},record_income_dates{};
    // League income is owned by ManagerSeasonServices::ticket_income.
    std::array<uint32_t,2> cup_income{};
    std::array<ClubId,3> spectator_opponents,income_opponents;
    bool operator==(const ManagerAttendanceState&)const=default;
};
struct ManagerMatchState {
    std::optional<ManagerAttendanceState> attendance;
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
struct CareerSettings {
    uint8_t mode=3,difficulty=2,duration=5,planned_managers=1;
    bool foreign_rule=false;
    bool operator==(const CareerSettings&)const=default;
};
struct ManagerStadium {
    std::array<uint8_t,22> facility_values{};
    std::array<uint8_t,8> stand_condition{},facility_levels{},stand_capacity{};
    uint8_t capacity_sum=0;
    bool operator==(const ManagerStadium&)const=default;
};
struct ManagerYouthTeam {
    uint8_t strength=0,position=0,league=0;
    bool operator==(const ManagerYouthTeam&)const=default;
};
struct ManagerEnvironment {
    std::array<ManagerYouthTeam,3> youth;
    std::array<int32_t,6> monthly{};
    bool operator==(const ManagerEnvironment&)const=default;
};
struct ManagerSponsorContract {
    uint32_t amount=0;uint8_t months=0;
    bool operator==(const ManagerSponsorContract&)const=default;
};
struct ManagerSeasonServices {
    uint16_t tickets_sold=0;
    uint32_t ticket_income=0,total_ticket_income=0;
    std::array<std::array<uint8_t,12>,30> advertising_offers{};
    // Seventeen regular contracts exclude the designated-player extension.
    // Amount/month bytes of the three separately named sponsors are zero here.
    std::array<std::array<uint8_t,12>,17> advertising_contracts{};
    uint8_t strength_history_count=0;
    std::array<uint8_t,38> strength_history{},position_history{};
    bool operator==(const ManagerSeasonServices&)const=default;
};
struct ManagerCareerState {
    ManagerStadium stadium;
    ManagerEnvironment environment;
    std::array<ManagerSponsorContract,3> sponsors;
    uint8_t board_patience=0,training_plan=0;
    std::array<uint8_t,18> transfer_filters{};
    std::array<PlayerId,5> poach_attempts;
    std::optional<std::array<uint8_t,150>> season_records={};
    std::optional<ManagerSeasonServices> season_services={};
    bool operator==(const ManagerCareerState&)const=default;
};
struct Manager {
    uint8_t seat=0;std::string name;ClubId club;LeagueId start_league;uint8_t portrait=0;
    std::optional<ManagerFinance> finance={};
    std::optional<ManagerTactics> tactics={};
    std::optional<ManagerMatchState> match_state={};
    std::optional<ManagerCareerState> career={};
    bool operator==(const Manager&)const=default;
};
// A nomination references the club-owned players without transferring them.
// Its national tactical board is independent of the manager's club board.
struct NationalSquad {
    ClubId team;
    uint32_t nominated_on=0;
    std::optional<uint8_t> coach;
    std::array<RosterSlot,24> roster;
    ManagerTactics tactics;
    ManagerMatchState match_state;
    bool operator==(const NationalSquad&)const=default;
};
struct CrestResource {std::string path;uint8_t cell=0;bool operator==(const CrestResource&)const=default;};
struct EconomyMessage {
    uint32_t recipients=0,read_by=0;uint16_t date=0;
    std::array<uint8_t,150> text{};
    bool operator==(const EconomyMessage&)const=default;
};
struct ManagerMonthlyEconomy {
    uint8_t history_count=0;
    std::array<uint32_t,2> expenses{};
    // Salary/bonus totals and categorized income/expenses. The shared main
    // account, ticket, interest and overdraft counters have separate owners.
    std::array<uint32_t,15> counters{};
    bool operator==(const ManagerMonthlyEconomy&)const=default;
};
struct ManagerEconomy {
    uint32_t deposit=0,interest_income=0,overdraft_interest=0;
    std::optional<uint32_t> bank_robbery_month; // First day of the last blocked source month.
    std::optional<ManagerMonthlyEconomy> monthly={};
    // bank, then four global human lender seats; principal,total,payment,term,
    // elapsed months,payments made (bank_credit.hpp).
    std::array<std::array<uint32_t,6>,5> loans{};
    std::array<uint8_t,30> construction_weeks{};
    std::array<uint8_t,8> planned_stand_capacity{},stand_flags{};
    uint32_t submitted_balance_plan=0;
    // Source170,171,174..179; 172/173 belong to ManagerMatchState.
    std::array<uint8_t,8> offers{};
    uint8_t camp_weeks=0,camp_flags=0,weekly_count=0;
    uint32_t camp_bill=0;
    std::array<uint8_t,52> facility_history{};
    std::array<std::array<uint32_t,52>,4> financial_history{};
    bool operator==(const ManagerEconomy&)const=default;
};
struct CareerEconomy {
    uint8_t week=0;
    std::array<ManagerEconomy,4> managers{};
    std::vector<EconomyMessage> messages;
    uint32_t recipient_mask=0;
    bool operator==(const CareerEconomy&)const=default;
};
struct CareerMarkets {
    std::array<uint8_t,12*0x5c> stocks{};
    std::array<uint8_t,6*0x1c> funds{};
    uint8_t first_default_name=0;
    bool operator==(const CareerMarkets&)const=default;
};
struct CareerPlayerPools {
    std::array<PlayerId,80> domestic;
    std::array<PlayerId,20> foreign;
    bool operator==(const CareerPlayerPools&)const=default;
};
struct InitialCompetitionFields {
    // Source order: UEFA Cup, Champions League, Cup Winners' Cup.
    std::array<std::vector<ClubId>,3> qualified,fields;
    std::array<ClubId,3> holders;
    std::vector<ClubId> national_field;
    // Sorted country rows keep optional construction movable without allocating
    // a tree sentinel (notably with the MSVC standard library).
    std::vector<std::pair<Country,ClubId>> champions,domestic_holders;
    bool manager_services_complete=false;
    bool operator==(const InitialCompetitionFields&)const=default;
};
struct MarketBid {
    ClubId bidder;uint32_t fee=0,salary=0;uint8_t months=0;
    std::array<uint8_t,7> bonuses{};
    bool operator==(const MarketBid&)const=default;
};
struct MarketListing {
    PlayerId player;ClubId seller;bool human_seller=false,loan=false;
    uint32_t fee=0,salary=0;uint8_t asking_percent=0,demand_percent=0,weeks=0;
    std::array<MarketBid,4> bids;
    bool operator==(const MarketListing&)const=default;
};
struct TransferMarket {
    std::array<ClubId,4> last_entry40_bidders;
    bool entry40_history_known=true;
    std::array<std::optional<MarketListing>,44> rows;
    uint8_t list_toggle=0;int32_t checkpoint_game_seed=0;
    bool operator==(const TransferMarket&)const=default;
};
using GlobalPlayerStatistics = std::map<std::pair<ClubId,PlayerId>,PlayerClubStatistics>;
struct World {
    std::vector<NationalSquad> national_squads;
    GlobalPlayerStatistics player_statistics;
    std::optional<CareerEconomy> economy;
    bool players_initialized=false;
    // Present after the market/mood phase; old saves do not invent these values.
    std::optional<CareerMarkets> markets={};
    std::optional<CareerPlayerPools> free_players={};
    std::optional<CareerSettings> settings={};
    std::optional<InitialCompetitionFields> initial_competitions={};
    std::optional<TransferMarket> transfer_market={};
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
void validate_manager_attendance(const World&,const ManagerMatchState&);
// Filter an immutable imported master world before manager selection. Countries
// are removed as entities, never demoted into an AI/foreign fallback league.
World select_world_scope(const World& source,GameScope);
}
