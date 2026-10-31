// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/save.hpp"
#include "game/prediction.hpp"
namespace bmh::world12 {
struct LeaguePrediction {
    bmh::TablePredictionResult result;
    std::optional<uint8_t> source_size_word;
};
// Read-only forecast. Goal-notice flags/messages belong to the career owner;
// future human ratings consume the result through explicit match context.
LeaguePrediction predict_league(const CareerSnapshot&,ClubId,int mode,
    const std::map<ClubId,uint8_t>& point_deductions={});
}
