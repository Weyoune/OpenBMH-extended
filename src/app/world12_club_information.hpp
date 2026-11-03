// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <functional>
#include <memory>
#include "game/world12/save.hpp"
#include "ui/club_info.hpp"
namespace bmh::app {struct ClubModalServices;}
namespace bmh::app::world12 {
struct ClubInformationProvider:std::function<ui::ClubInfoProjection(ui::ClubInfoView)>{using std::function<ui::ClubInfoProjection(ui::ClubInfoView)>::function;std::shared_ptr<bmh::app::ClubModalServices> modals;int runtime_team=-1;};
using ClubInformationPresenter=std::function<bool(const ClubInformationProvider&,const Image&)>;
// Source roster entry has lineup/injury flag writes even in this inspection view.
// Publish only those mapped flags after the synchronous presenter has completed.
void inspect_club_information(bmh::world12::CareerSnapshot&,const GameState&,const LeImage&,
    const TextDb&,bmh::world12::ClubId,const ClubInformationPresenter&,const Image&,std::optional<uint8_t> viewer=std::nullopt,const LeImage* english_rules=nullptr);
}
