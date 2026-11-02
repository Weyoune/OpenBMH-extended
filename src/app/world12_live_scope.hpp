// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/match_projection.hpp"
#include "game/match_engine.hpp"
#include "ui/pitch.hpp"
#include "ui/tactics.hpp"
namespace bmh::app::world12 {
struct LiveEditOutcome {bool applied=false;uint32_t notice=0;std::string error;};
// Numeric, synchronous capability for one actual watched human match. No world
// IDs are packed by this scope; the retained projection owns all local mappings.
class LiveScope {
public:
    LiveScope(bmh::world12::AiMatchProjection&,MatchEngine&,const LeImage&,int minute,int team,
        int source_mode,uint64_t& nonce,const std::optional<LiveMatchEvent>& event={});
    ~LiveScope();
    LiveScope(const LiveScope&)=delete;
    LiveScope& operator=(const LiveScope&)=delete;
    ui::LineupState lineup()const;
    ui::PitchExtras pitch()const;
    ui::TacticsState tactics()const;
    LiveEditOutcome apply(const ui::PitchRequest&);
    LiveEditOutcome apply(const ui::LineupRequest&);
    LiveEditOutcome apply(const ui::TacticsRequest&);
    void finish();
    void abort();
    uint64_t revision()const{return revision_;}
    int manager()const{return manager_;}
    int team()const{return team_;}
    int opponent()const{return opponent_;}
    int minute()const{return minute_;}
    bool event_origin()const{return event_.has_value();}
    const GameState& state()const{return projection_.state;}
private:
    bmh::world12::AiMatchProjection& projection_;MatchEngine& engine_;const LeImage& rules_;
    int minute_,team_,manager_,opponent_=-1,mode_;
    uint64_t& nonce_;uint64_t revision_=0;
    bool closed_=false;
    std::optional<LiveMatchEvent> event_;
    GameState before_;GameRandom game_before_;WatcomRandom random_before_;
    std::vector<MatchGroup> groups_before_;
    const MatchGroup* group_storage_=nullptr;
    void validate()const;
    void validate_actor(int,int,uint64_t)const;
};
}
