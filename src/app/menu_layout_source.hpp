// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "formats/binary.hpp"
#include <array>
#include <string>
namespace bmh::app {
// Source CopyN payload only. The caller owns actor, path and rendered-geometry checks.
inline bool validate_menu_layout_source(const Bytes& bytes,std::string& error) {
    constexpr size_t kLayoutBytes=54*16;constexpr int kDeskItemCount=54;
        if (bytes.size() != kLayoutBytes) { error = "menu_layout_wrong_size"; return false; }
        // Bound the forward traversal before asking the desk projector to
        // validate geometry, kinds, and remaining source fields. Native layouts
        // may contain valid custom back-links; Source load does not normalize them.
        std::array<bool, kDeskItemCount> seen{};
        const int head = bytes[5];
        // CopyN permits an empty linked drawer (head == 0).
        if (head == 0) return true;
        if (head < 1 || head >= kDeskItemCount) { error = "menu_layout_invalid_head"; return false; }
        int item = head;
        for (int steps = 0; steps < kDeskItemCount; ++steps) {
            if (item <= 0 || item >= kDeskItemCount || seen[size_t(item)]) {
                error = "menu_layout_invalid_chain"; return false;
            }
            seen[size_t(item)] = true;
            const size_t at = size_t(item) * 16;
            item = bytes[at + 5];
            if (item == 0) return true;
            if (item == 0xff) { error = "menu_layout_invalid_chain_terminator"; return false; }
        }
        error = "menu_layout_chain_cycle"; return false;
}
}
