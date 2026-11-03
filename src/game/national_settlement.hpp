// SPDX-License-Identifier: GPL-3.0-only
#pragma once
namespace bmh {
struct GameState;
// Shared Source association allowance, podium premium and honours update.
// AdvanceCup owns its normal exactly-once invocation at competition4 completion.
void national_tournament_end(GameState&);
}
