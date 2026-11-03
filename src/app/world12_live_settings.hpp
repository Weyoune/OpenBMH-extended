// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/match_projection.hpp"
#include "game/match_engine.hpp"
#include "ui/settings.hpp"
namespace bmh::app::world12 {
// A synchronous S window belongs to the retained actual minute workspace.
// Only presentation cells return through the match owner's atomic publication.
class LiveSettingsScope {
public:
    LiveSettingsScope(bmh::world12::AiMatchProjection&,MatchEngine&,const LeImage&,
        int minute,uint64_t& nonce,std::optional<ui::SettingsSoundCapabilities> sound={});
    ~LiveSettingsScope();
    LiveSettingsScope(const LiveSettingsScope&)=delete;
    LiveSettingsScope& operator=(const LiveSettingsScope&)=delete;
    ui::SettingsState view()const;
    void apply(const ui::SettingsWriteRequest&);
    Bytes file(const ui::SaveLoadRequest&,const std::optional<Bytes>& incoming={});
    void finish();
    void abort();
    const GameState& state()const{return projection_.state;}
private:
    bmh::world12::AiMatchProjection& projection_;MatchEngine& engine_;const LeImage& rules_;
    std::optional<ui::SettingsSoundCapabilities> sound_;
    uint64_t& nonce_;uint64_t revision_=0;int manager_=-1;
    GameState before_;std::vector<MatchGroup> groups_;std::vector<uint8_t> buttons_before_,buttons_;
    bool desk_before_=false,closed_=false;
    int32_t game_seed_;uint32_t random_seed_;uint64_t game_calls_,random_calls_;
    const MatchGroup* storage_;
    void validate()const;
    void refresh(GameState&)const;
    void publish(GameState);
};
}
