// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "ui/contracts.hpp"
#include "ui/manager.hpp"
#include "formats/textd.hpp"
#include "game/month.hpp"
#include <algorithm>
#include <limits>
namespace bmh::app {
// Callers own modal authorization and rollback. Native and typed sessions share
// the Original quote, insurance, bonus and offer validation/calculation path.
template<class QuoteRecord,class Unchanged>
bool apply_contract_request_source(GameState& g,const TextDb& texts,WatcomRandom& random,
    const ui::ContractRequest& request,uint64_t& revision,std::optional<QuoteRecord>& quote_record,
    uint64_t& next_quote_id,uint64_t& last_request_id,ui::ContractActionResult& result,
    std::string& error,Unchanged demands_unchanged) {
    const auto reject = [&](std::string message) { error = std::move(message); return false; };
    const auto original_text = [&](uint32_t address) {
        return ui::original_to_utf8(texts[(address - ::bmh::layout::TextPointerTable) / 4]);
    };
    const int active = g.memory.has(::bmh::layout::ActiveManager) ? int(g.memory.u8(::bmh::layout::ActiveManager)) : -1;
    if (request.request_id == 0 || request.request_id <= last_request_id ||
        request.revision != revision || revision == std::numeric_limits<uint64_t>::max() ||
        request.manager_slot != active || active < 0 || active > 3 ||
        size_t(active) >= g.manager_teams.size() || request.runtime_club != g.manager_teams[size_t(active)] ||
        request.runtime_club == kNoTeam)
        return reject("Der Vertragsauftrag gehört zu einem veralteten Manager oder Stand.");
    const bool team_bonuses = request.kind == ui::ContractActionKind::submit_bonuses && request.row == -1;
    const SquadRange range = squad_range(request.runtime_club);
    int player = -1;
    size_t board = 0;
    if (team_bonuses) {
        if (request.player_id != -1 || request.quote_id != 0)
            return reject("Der Teamprämienauftrag enthält eine fremde Spieleridentität.");
    } else {
        if (request.row < 0 || size_t(request.row) >= range.count || request.row >= 24 ||
            range.first + size_t(request.row) >= g.squad_slots.size())
            return reject("Der ausgewählte Kaderplatz ist nicht mehr gültig.");
        player = g.squad_slots[range.first + size_t(request.row)] & 0x0fff;
        if (player == 0x0fff || player != request.player_id || size_t(player) >= g.players.size())
            return reject("Der Spieler gehört nicht mehr zu diesem Kaderplatz.");
        board = size_t(active) * 0x840 + size_t(request.row) * 0x58;
        if (board + 0x58 > g.tactics.size()) return reject("Der Vertragsdatensatz ist unvollständig.");
    }
    if (request.kind == ui::ContractActionKind::request_demands) {
        if (request.quote_id != 0 || request.term_column != 0 || request.demand != 0 || request.monthly_salary != 0 ||
            request.insurance_months != 0 || std::any_of(request.proposed_bonuses.begin(),
                request.proposed_bonuses.end(), [](uint8_t b) { return b != 0; }))
            return reject("Der Forderungsauftrag enthält ungültige Angebotsdaten.");
        GameState staged_game = g;
        WatcomRandom staged_rand = random;
        std::array<uint32_t, 4> demands{};
        try { demands = contract_demands(staged_game, active, request.row, true, staged_rand); }
        catch (const std::exception&) { return reject("Die Gehaltsforderungen konnten nicht ermittelt werden."); }
        if (!demands_unchanged(staged_game,g))
            return reject("Die Forderungsabfrage hat unerwartet den Spielstand verändert.");
        random = staged_rand;
        ++revision;
        QuoteRecord quote;
        quote.quote_id = next_quote_id++;
        quote.request_id = request.request_id;
        quote.revision = revision;
        quote.manager_slot = active;
        quote.runtime_club = request.runtime_club;
        quote.row = request.row;
        quote.player_id = player;
        quote.demands = demands;
        quote_record = quote;
        last_request_id = request.request_id;
        result.success = true;
        result.quote_id = quote.quote_id;
        result.demands = demands;
        return true;
    }
    if (!team_bonuses && (!quote_record || request.quote_id != quote_record->quote_id ||
        request.request_id <= quote_record->request_id || request.revision != quote_record->revision ||
        request.manager_slot != quote_record->manager_slot || request.runtime_club != quote_record->runtime_club ||
        request.row != quote_record->row || player != quote_record->player_id))
        return reject("Dieser Auftrag ist nicht mehr an die aktuelle Verhandlung gebunden.");
    const bool full_mode = g.memory.u8(::bmh::layout::GameMode) == 3;
    if (request.kind == ui::ContractActionKind::insure_player) {
        if (!full_mode || request.term_column >= 4 || request.insurance_months < 1 || request.insurance_months > 60 ||
            request.demand != 0 || request.monthly_salary != 0 ||
            std::any_of(request.proposed_bonuses.begin(), request.proposed_bonuses.end(), [](uint8_t b) { return b != 0; }))
            return reject("Der Versicherungsauftrag enthält ungültige Daten.");
        // ContractBonuses checks these in this order and uses the corresponding Original dialog.
        if (g.tactics[board + 0x0b] & 0x20) return reject(original_text(::bmh::layout::CareerSession_Reject_TextPointer));
        if (g.tactics[board + 0x0b] & 0x40) return reject(original_text(::bmh::layout::CareerSession_Reject_TextPointer2));
        if (g.tactics[board + 0x43] != 0) return reject(original_text(::bmh::layout::CareerSession_Reject_TextPointer3));
        if (g.players[size_t(player)].w35c >> 27) return reject(original_text(::bmh::layout::CareerSession_Reject_TextPointer4));
        GameState staged_game = g;
        if (!contract_insurance(staged_game, active, request.row, request.term_column, request.insurance_months).accepted)
            return reject("Die Versicherung konnte nicht bestätigt werden.");
        g = std::move(staged_game);
        ++revision;
        last_request_id = request.request_id;
        quote_record->revision = revision;
        result.success = result.accepted = true;
        result.quote_id = quote_record->quote_id;
        result.demands = quote_record->demands;
        return true;
    }
    for (size_t i = 0; i < request.proposed_bonuses.size(); ++i)
        if (request.proposed_bonuses[i] > (i < 3 ? 20 : 200))
            return reject("Die Prämie liegt über dem Originalmaximum.");
    if (request.kind == ui::ContractActionKind::submit_bonuses) {
        if (!full_mode || request.term_column != 0 || request.demand != 0 || request.monthly_salary != 0 ||
            request.insurance_months != 0) return reject("Der Prämienauftrag enthält ungültige Daten.");
        GameState staged_game = g;
        const bool accepted = contract_bonuses(staged_game, active, request.row, request.proposed_bonuses);
        g = std::move(staged_game);
        ++revision;
        last_request_id = request.request_id;
        quote_record.reset();
        result.success = true;
        result.accepted = accepted;
        result.feedback = accepted ? original_text(team_bonuses ? ::bmh::layout::CareerSession_Reject_TextPointer8 : ::bmh::layout::CareerSession_Reject_TextPointer9) :
            team_bonuses ? original_text(::bmh::layout::CareerSession_Reject_TextPointer7) : std::string{};
        return true;
    }
    if (request.kind != ui::ContractActionKind::submit_offer || request.term_column >= 4 || request.insurance_months != 0 ||
        request.demand != quote_record->demands[request.term_column] || request.demand == 0 ||
        request.demand == std::numeric_limits<uint32_t>::max() || request.monthly_salary > 200000 ||
        (g.tactics[board + 0x4f] & 0x80) || (g.tactics[board + 0x0b] & 0x60))
        return reject("Dieses Angebot ist im Original nicht mehr verhandelbar.");
    if (!full_mode && !std::equal(request.proposed_bonuses.begin(), request.proposed_bonuses.end(),
                                  g.tactics.begin() + long(board + 0x12)))
        return reject("Prämienänderungen sind nur in Spielmodus III verfügbar.");
    GameState staged_game = g;
    WatcomRandom staged_rand = random;
    bool accepted = false;
    try {
        accepted = contract_offer(staged_game, active, request.row, request.term_column,
                                 request.monthly_salary, request.demand, false, false, staged_rand,
                                 &request.proposed_bonuses);
    } catch (const std::exception&) { return reject("Der Vertrag konnte nicht verhandelt werden."); }
    g = std::move(staged_game);
    random = staged_rand;
    ++revision;
    last_request_id = request.request_id;
    quote_record.reset();
    result.success = true;
    result.accepted = accepted;
    result.feedback = original_text(accepted ? ::bmh::layout::CareerSession_Reject_TextPointer5 : ::bmh::layout::CareerSession_Reject_TextPointer6);
    return true;
}
}
