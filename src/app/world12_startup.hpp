// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "app/world12_session.hpp"
#include "ui/flow.hpp"
namespace bmh::app::world12 {
enum class StartupProgress { idle,selecting,prepared };
struct StartupResult {StartupProgress progress=StartupProgress::idle;std::string error;};
struct StartupTurnHooks {ClockHooks clock;bmh::world12::HumanRoundHooks matches;};
// Same request boundary used by the development SDL entry and input tests.
// Prepared includes the owned initial empty event and explicit typed desktop.
// Initial draw/ticket presentations and later management/day actions remain separate.
StartupResult serve_startup(ui::StartupFlow&,const ui::FlowResult&,std::unique_ptr<Session>&,
    const std::filesystem::path& data,uint32_t seed,uint64_t scope,const PendingOfferHooks& hooks={},
    const StartupTurnHooks* turn=nullptr);
}
