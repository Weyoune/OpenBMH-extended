// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/save.hpp"
namespace bmh::world12 {
uint8_t prepared_cup_flags(const KnockoutStage&);
void validate_prepared_event(const CareerSnapshot&);
}
