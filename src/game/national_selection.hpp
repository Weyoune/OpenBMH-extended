// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace bmh {
// Original quotas, ascending positional strength, and stable threshold ties.
// Tokens belong to the caller: native squad offsets or indices into a global
// candidate list. A global player identity is never packed into a squad word.
class NationalSquadSelection {
public:
    inline static constexpr std::array<uint8_t, 4> quotas{3, 5, 7, 5};
    void consider(unsigned position, int16_t strength, uint32_t token) {
        if (position >= quotas.size()) throw std::invalid_argument("national squad: invalid position");
        std::size_t insertion = 0;
        while (insertion < quotas[position] && strength > scores_[position][insertion]) ++insertion;
        if (!insertion) return;
        const auto last = insertion - 1;
        for (std::size_t i = 0; i < last; ++i) {
            scores_[position][i] = scores_[position][i + 1];
            tokens_[position][i] = tokens_[position][i + 1];
        }
        scores_[position][last] = strength;
        tokens_[position][last] = token;
        if (counts_[position] < quotas[position]) ++counts_[position];
    }
    uint32_t token(std::size_t position, std::size_t rank) const { return tokens_.at(position).at(rank); }
    bool complete() const { return counts_ == quotas; }
private:
    std::array<std::array<int16_t, 7>, 4> scores_{};
    std::array<std::array<uint32_t, 7>, 4> tokens_{};
    std::array<uint8_t, 4> counts_{};
};
}
