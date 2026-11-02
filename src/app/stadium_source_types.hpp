// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/stadium_blocks.hpp"
#include <optional>
#include <string>
#include <utility>
namespace bmh::app {
struct StadiumSourceRuntime {
    struct BlockExpansion {
        StadiumBlockExpansionQuote quote;
        uint32_t total_target_places = 0;
        uint8_t desired_features = 0;
        int header_text_variant = -1;
        std::optional<StandBlockOffer> tip;
    };
    bool opened = false;
    bool data_open = false;
    uint8_t data_flags = 0x40;
    int manager = -1, club = -1, stable_club = -1, mode = -1;
    uint64_t scope = 0, sequence = 0, revision = 0;
    int price_percent = 100;
    std::optional<std::pair<int, int32_t>> periphery_quote;
    std::optional<StadiumInvestment> investment;
    std::optional<uint8_t> selection_status;
    int option_index = -1, text_variant = -1;
    std::optional<StadiumInvestmentOption> option;
    std::optional<StandBlockOffer> tip;
    std::optional<BlockExpansion> block_expansion;
    std::string feedback;
};

}
