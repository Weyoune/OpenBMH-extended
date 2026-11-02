// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "app/world12_session.hpp"
#include "ui/flow.hpp"
namespace bmh::app::world12 {
enum class StartupProgress { idle,selecting,prepared };
struct StartupResult {StartupProgress progress=StartupProgress::idle;std::string error;};
using SettingsNoticePresenter=std::function<bool(const GameState&,uint32_t message_address)>;
struct StartupTurnHooks {ClockHooks clock;bmh::world12::HumanRoundHooks matches;};
// Same request boundary used by the development SDL entry and input tests.
// Prepared includes the owned initial empty event and explicit typed desktop.
// Initial draw/ticket presentations and later management/day actions remain separate.
StartupResult serve_startup(ui::StartupFlow&,const ui::FlowResult&,std::unique_ptr<Session>&,
    const std::filesystem::path& data,uint32_t seed,uint64_t scope,const PendingOfferHooks& hooks={},
    const StartupTurnHooks* turn=nullptr,const AdvertisingRenewalPresenter& advertising={},const ConstructionNoticeHook& construction={},
    const SeasonTicketCompensationNoticeHook& compensation={},std::optional<ui::SettingsSoundCapabilities> sound=std::nullopt,const SettingsNoticePresenter& settings_notice={},const ClubInformationPresenter& club_information={},const TableRangePresenter& table_range={},const MarketPresenters& market={});
}
