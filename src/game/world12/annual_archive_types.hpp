// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/identity.hpp"
#include "game/long_career.hpp"
#include "game/world12/sorted_map.hpp"
#include <optional>
#include <vector>
namespace bmh::world12 {
struct World;
enum class AnnualTitleKind:uint8_t { league,domestic,european,indoor,national };
struct AnnualTitleRow {
    AnnualTitleKind kind=AnnualTitleKind::league;
    std::optional<Country> country;
    LeagueId league;
    // Domestic: canonical DomesticCup enum. European/national/indoor: Source
    // competition number. League rows use their canonical LeagueId instead.
    uint8_t competition=0;
    std::optional<uint8_t> source_category;
    ClubId winner,runner_up;
    std::array<ClubId,4> podium;
    bool operator==(const AnnualTitleRow&)const=default;
};
struct AnnualSeasonTitles {
    uint32_t season=0;
    std::vector<AnnualTitleRow> titles;
    bool operator==(const AnnualSeasonTitles&)const=default;
};
struct AnnualTitleArchive {
    bool earlier_history_unknown=false;
    uint32_t first_retained_season=0;
    std::vector<AnnualSeasonTitles> seasons;
    // Exact Original-encoding labels remain separate from resolved identities.
    // A matching name is never accepted as historical identity evidence.
    SortedMap<uint32_t,std::array<std::string,9>> unresolved_source_labels;
    bool operator==(const AnnualTitleArchive&)const=default;
};
void validate_annual_title_archive(const World&);
}
