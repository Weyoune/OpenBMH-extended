// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "app/world12_session.hpp"
#include "game/match_engine.hpp"
namespace bmh::app::world12 {
bmh::world12::LeagueId live_group_league(const bmh::world12::World&,
    const bmh::world12::AiMatchProjection&,const MatchGroup&);
// Unpublished display overlay: current scores are never a calendar receipt and
// cannot be committed through this capability. Original family actions run on
// an isolated Session; only explicit UI preferences return to the match owner.
class LiveFamilyScope {
public:
    LiveFamilyScope(const Session& resources_owner,const bmh::world12::CareerSnapshot& staged,
        bmh::world12::AiMatchProjection&,MatchEngine&,uint8_t canonical_seat,uint64_t& nonce,
        uint8_t child=0,std::optional<bmh::world12::LeagueId> league={});
    ~LiveFamilyScope();
    LiveFamilyScope(const LiveFamilyScope&)=delete;
    LiveFamilyScope& operator=(const LiveFamilyScope&)=delete;
    ui::World12TableState view()const;
    // True retires the scope through the Source close action.
    bool apply(const ui::World12TableRequest&,const std::filesystem::path&,
        const ClubInformationPresenter& club = {},const TableRangePresenter& range = {});
    void abort();
private:
    const Session& source_;
    bmh::world12::AiMatchProjection& projection_;
    MatchEngine& engine_;
    std::unique_ptr<Session> viewer_;
    uint8_t seat_;
    uint64_t& nonce_;
    uint64_t source_revision_,revision_=0;
    int32_t game_seed_;
    uint32_t watcom_seed_;
    uint64_t game_calls_,watcom_calls_;
    const MatchGroup* groups_;
    size_t group_count_;
    std::vector<bmh::world12::LeagueId> live_leagues_;
    bool closed_=false,dispatching_=false;
    void validate()const;
    void select_current_round();
};
// Testable, read-only overlay builder. Calendar ownership stays in the original
// staged snapshot; the returned view deliberately has no progression owner.
bmh::world12::CareerSnapshot live_family_snapshot(const bmh::world12::CareerSnapshot&,
    const bmh::world12::AiMatchProjection&,const MatchEngine&);
}
