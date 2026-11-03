// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "ui/other_games_state.hpp"
#include "game/world12/save.hpp"

namespace bmh::ui {
enum class World12OtherPage:uint8_t {
    german_cup,uefa_cup,champions_league,cup_winners_cup,national_qualifiers,supercups,fa_cup,league_cup
};
struct World12OtherSelection {
    World12OtherPage page=World12OtherPage::german_cup;
    int stage=-1,round=-1,participant_offset=0;
    OtherGamesDisplayMode mode=OtherGamesDisplayMode::detail;
    bool operator==(const World12OtherSelection&)const=default;
};
struct World12CompetitionOverlay {
    uint64_t revision=0;
    uint8_t source_entry=0,played=0;
    std::vector<bmh::world12::Fixture> matches;
    std::vector<uint16_t> scores;
};
// Pure saved European programme projection, optionally overlaid with this exact
// active group's actual current scores. It never commits a result or draws.
OtherGamesState world12_european_games_state(const bmh::world12::CareerSnapshot&,
    const World12OtherGamesIdentity&,World12OtherSelection,
    const std::vector<World12CompetitionOverlay>& overlay={});
OtherGamesState world12_national_games_state(const bmh::world12::CareerSnapshot&,
    const World12OtherGamesIdentity&,const TextDb&,uint8_t source_entry=255,int round=-1,
    const std::vector<World12CompetitionOverlay>& overlay={});
OtherGamesState world12_regional_games_state(const bmh::world12::CareerSnapshot&,
    const World12OtherGamesIdentity&,const TextDb&,int round=-1,
    const std::optional<World12CompetitionOverlay>& overlay=std::nullopt);
OtherGamesState world12_indoor_games_state(const bmh::world12::CareerSnapshot&,
    const World12OtherGamesIdentity&,uint8_t tournament,uint8_t source_entry,int round=-1,
    const std::optional<World12CompetitionOverlay>& overlay=std::nullopt);
World12OtherSelection default_world12_other_selection(const bmh::world12::World&);
OtherGamesState world12_other_games_state(const bmh::world12::CareerSnapshot&,
    const World12OtherGamesIdentity&,World12OtherSelection,const std::vector<World12CompetitionOverlay>& overlay={});
// Read-only navigation. Team-info/close are returned to the typed window owner;
// this helper never writes the world or interprets local renderer tokens as IDs.
World12OtherSelection navigate_world12_other_games(const bmh::world12::CareerSnapshot&,
    const World12OtherGamesIdentity&,World12OtherSelection,const OtherGamesRequest&);
}
