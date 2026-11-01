// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "app/world12_session.hpp"
#include "ui/save_load.hpp"
namespace bmh::app::world12 {
// Validates a single regular FBM target confined to the configured save directory.
std::filesystem::path checked_game_path(const std::filesystem::path& directory,const ui::SaveLoadRequest&);
// Writes/verifies an exclusive temporary, then publishes it without truncating
// the previous target. Failure before publication preserves that target.
void publish_game_save(const std::filesystem::path&,const Savegame&,bool overwrite);
ui::SaveLoadInfoReader save_info_reader(LeImage rules);
}
