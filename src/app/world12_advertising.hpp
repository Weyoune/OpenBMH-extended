// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/desk_actions.hpp"
#include <functional>
namespace bmh::app::world12 {
enum class AdvertisingRenewalAnswer {renew,decline,acknowledged,abort};
using AdvertisingRenewalPresenter=std::function<AdvertisingRenewalAnswer(const GameState&,const AdvertisingRenewal&)>;
}
