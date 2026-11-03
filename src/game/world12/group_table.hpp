// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/identity.hpp"
#include "game/table.hpp"
#include <vector>
namespace bmh::world12 {
struct SourceGroupTable {
    std::vector<ClubId> clubs;
    bmh::LeagueTable table;
};
// Immutable Source AddResult group table. Scores may include an actual current
// live round after the saved played cursor; no cursor, draw or seed is advanced.
SourceGroupTable project_source_group_table(const std::vector<ClubId>& clubs,uint8_t flags,
    uint8_t rounds,uint8_t played,const std::vector<uint16_t>& scores);
}
