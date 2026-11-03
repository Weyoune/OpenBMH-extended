// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/save.hpp"
#include "game/highscore.hpp"
namespace bmh::world12 {
struct OwnedHighscore {
    uint8_t manager=0;ClubId club;
    HighscoreScore score;
    HighscoreRow candidate;
};
struct OwnedHighscoreProjection {
    uint16_t signature=0;
    bool ranked=false;
    HighscoreTable rows;
    std::vector<OwnedHighscore> managers;
};
// Explicit verified signature ownership is required; zero means known unranked,
// never unknown. Assets come from each actual scored canonical manager. The
// Original ActiveManager/manager_count contexts remain unchanged for Legacy.
OwnedHighscoreProjection project_owned_highscores(const CareerSnapshot&,const GameState& reference,
    const HighscoreTable& existing={});
}
