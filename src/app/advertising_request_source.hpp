// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "ui/advertising.hpp"
#include "game/state.hpp"
#include <limits>
#include <algorithm>
namespace bmh::app {
// Modal authorization and atomic publication belong to the caller.
inline bool apply_advertising_request_source(GameState& g,WatcomRandom& random,
    const ui::AdvertisingRequest& request,uint64_t& revision,uint64_t& last_request_id,
    std::string& error,AdvertisingSpendingResult* spending_result=nullptr) {
    error.clear();
    if (spending_result) *spending_result = {};
    const int manager = request.manager_slot;
    if (!g.memory.has(::bmh::layout::ActiveManager) || manager < 0 || manager > 3 ||
        size_t(manager) >= g.manager_teams.size() || int(g.memory.u8(::bmh::layout::ActiveManager)) != manager ||
        request.runtime_club != g.manager_teams[size_t(manager)] ||
        request.revision != revision || revision == std::numeric_limits<uint64_t>::max() ||
        request.request_id == 0 || request.request_id <= last_request_id ||
        request.kind == ui::AdvertisingActionKind::check_renewal ||
        request.renewal_display_slot != -1 || request.renewal_contract_slot != -1 ||
        request.renewal_expected_contract != std::array<uint8_t, 12>{}) {
        error = "Diese Werbeaktion ist nicht mehr aktuell.";
        return false;
    }
    if (request.kind == ui::AdvertisingActionKind::accept_offer) {
        if (request.offer.manager != manager || request.spending != AdvertisingSpendingRequest{}) {
            error = "Das Werbeangebot enthält fremde Ausgabedaten.";
            return false;
        }
        const auto result = accept_advertising_offer(g, request.offer);
        if (result.status != AdvertisingOfferStatus::accepted) {
            error = result.status == AdvertisingOfferStatus::unsupported_extension
                ? "Diese Vertragsverlängerung ist noch nicht verfügbar."
                : "Dieses Werbeangebot kann nicht übernommen werden.";
            return false;
        }
    } else if (request.kind == ui::AdvertisingActionKind::set_spending) {
        if (request.spending.manager != manager || request.offer != AdvertisingOfferRequest{}) {
            error = "Der Ausgabenauftrag enthält fremde Vertragsdaten.";
            return false;
        }
        const auto result = set_advertising_spending(g, request.spending, random);
        if (spending_result) *spending_result = result;
        if (result.status != AdvertisingSpendingStatus::accepted &&
            result.status != AdvertisingSpendingStatus::no_change &&
            result.status != AdvertisingSpendingStatus::insufficient_funds) {
            error = "Dieser Werbeausgabenauftrag ist nicht mehr gültig.";
            return false;
        }
        // Insufficient funds is an Original handled response: it clears the old
        // merchandise amount, but neither books cash nor draws a delivery delay.
    } else {
        error = "Unbekannte Werbeaktion.";
        return false;
    }
    last_request_id = request.request_id;
    ++revision;
    return true;
}
inline bool validate_advertising_renewal_request_source(const GameState& g,
    const ui::AdvertisingRequest& request,uint64_t revision,uint64_t last_request_id,std::string& error) {
    if (request.kind != ui::AdvertisingActionKind::check_renewal ||
        request.request_id == 0 || request.request_id <= last_request_id ||
        request.manager_slot < 0 || request.manager_slot > 3 ||
        !g.memory.has(::bmh::layout::ActiveManager) || int(g.memory.u8(::bmh::layout::ActiveManager)) != request.manager_slot ||
        size_t(request.manager_slot) >= g.manager_teams.size() ||
        request.runtime_club != g.manager_teams[size_t(request.manager_slot)] ||
        request.revision != revision || revision == std::numeric_limits<uint64_t>::max() ||
        request.renewal_display_slot < 0 || request.renewal_display_slot > 15 ||
        request.renewal_contract_slot < 0 || request.renewal_contract_slot > 15 ||
        request.offer != AdvertisingOfferRequest{} || request.spending != AdvertisingSpendingRequest{}) {
        error = "Diese Werbeaktion ist nicht mehr aktuell.";
        return false;
    }
    const size_t record = size_t(request.manager_slot) * 0xb0c + 0xa34 +
                          size_t(request.renewal_contract_slot) * 0x0c;
    if (record + 12 > g.manager_records.size() ||
        !std::equal(request.renewal_expected_contract.begin(), request.renewal_expected_contract.end(),
                    g.manager_records.begin() + std::ptrdiff_t(record)) ||
        (request.renewal_expected_contract[6] & 0x7f) == 0 || request.renewal_expected_contract[5] != 0) {
        error = "Diese Vertragsverlängerung ist nicht mehr aktuell.";
        return false;
    }

    return true;
}

}
