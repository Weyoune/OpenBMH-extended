// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/world.hpp"
#include "game/state.hpp"
namespace bmh::world12 {
struct CareerSnapshot;
struct HumanProjectionPolicy {int mode=3,difficulty=0;bool foreign_rule=false,three_points=true;};
// The original match calculations operate on bounded local references. Global
// 32-bit IDs never enter their packed fields; mutations are mapped back explicitly.
// The AI factory rejects humans. The separate human factory requires complete
// account/tactical/match state plus an authoritative forecast snapshot.
class AiMatchProjection {
public:
    static AiMatchProjection create(const World&,const std::vector<ClubId>&,const GameState& rules_template);
    static AiMatchProjection create_for_humans(const CareerSnapshot&,const std::vector<ClubId>&,
        const GameState& rules_template,HumanProjectionPolicy);
    static AiMatchProjection create_for_desktop(const CareerSnapshot&,const std::vector<ClubId>&,
        const GameState& rules_template,HumanProjectionPolicy);
    // Rating-only foreign clubs retain the Original non-squad indices and Core
    // random ratings; no players are synthesized for them.
    static AiMatchProjection create_for_european(const CareerSnapshot&,const std::vector<ClubId>&,
        const GameState& rules_template,HumanProjectionPolicy);
    // Explicit non-roster references for market/free-agent/loan workspaces.
    void include_players(const World&,const std::vector<PlayerId>&);
    void include_club_references(const World&,const std::vector<ClubId>&);
    uint32_t local_club_reference(ClubId)const;
    GameState state;
    ClubId global_club(uint32_t local)const;
    PlayerId global_player(uint32_t local)const;
    uint32_t local_club(ClubId)const;
    uint32_t local_player(PlayerId)const;
    uint8_t global_manager(uint8_t local)const{return manager_seats_.at(local);}
    size_t human_managers()const{return manager_seats_.size();}
    void commit_player_attributes(World&)const;
    void commit_match_state(World&,bool capture_season_services=false)const;
    void commit_transfer_state(World&,PlayerId,ClubId seller,ClubId buyer,bool moved)const;
    void commit_released_players(World&,const std::vector<std::pair<PlayerId,ClubId>>&)const;
private:
    static AiMatchProjection create_core(const World&,const std::vector<ClubId>&,const GameState&,bool allow_empty);
    std::vector<ClubRecord> abstract_clubs_;
    struct TransferMove {PlayerId player;ClubId seller,buyer;};
    void commit_state(World&,bool,const std::vector<TransferMove>&)const;
    void install_player_statistics(const World&,bool reset);
    void capture_player_statistics(World&,const std::vector<TransferMove>&)const;
    GlobalPlayerStatistics original_player_statistics_;
    std::map<uint32_t,std::pair<ClubId,PlayerId>> statistics_bindings_;
    std::vector<ClubId> clubs_;
    std::vector<PlayerId> players_;
    std::vector<PlayerRecord> originals_;
    std::vector<ClubRecord> original_clubs_;
    std::vector<Manager> original_managers_;
    std::optional<CareerSettings> original_settings_;
    std::vector<uint8_t> manager_seats_;
    std::vector<ClubId> manager_reference_clubs_;
    std::map<uint16_t,ClubId> record_opponents_;
    int32_t original_development_balance_=0;
    uint8_t original_trend_rotation_=0;
};
}
