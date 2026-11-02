// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "ui/bank.hpp"
#include "game/desk_actions.hpp"
#include "game/messages.hpp"
#include <limits>
#include <utility>
namespace bmh::app {
struct BankSourcePresentation {
    std::optional<int> last_tip;
    std::optional<ui::BankState::PropertyNotice> property_notice;
    std::optional<ui::BankState::CreditNotice> credit_notice;
};
// Shared Original command boundary. The caller owns modal authorization,
// staging/rollback and presentation; all bank calculations use the Core kernels.
inline bool apply_bank_request_source(GameState& g,const LeImage& exe,GameRandom& game,WatcomRandom& random,
    const ui::BankRequest& request,uint64_t& revision,BankSourcePresentation& presentation,std::string& error) {
    error.clear();
    if (revision == std::numeric_limits<uint64_t>::max()) {
        error = "revision_exhausted"; return false;
    }
    const int active = int(g.memory.u8(::bmh::layout::ActiveManager));
    if (request.manager_slot != active ||
        request.manager_slot < 0 || size_t(request.manager_slot) >= g.manager_teams.size() ||
        request.runtime_club != g.manager_teams[size_t(request.manager_slot)] || request.revision != revision) {
        error = "stale_manager_or_revision"; return false;
    }
    const size_t rec = size_t(request.manager_slot) * 0xb0c;
    std::optional<ui::BankState::PropertyNotice> property_notice;
    std::optional<ui::BankState::CreditNotice> credit_notice;
    const auto read_signed = [&](size_t offset) {
        const uint32_t v = uint32_t(g.manager_records.at(rec + offset)) |
            uint32_t(g.manager_records.at(rec + offset + 1)) << 8 |
            uint32_t(g.manager_records.at(rec + offset + 2)) << 16 |
            uint32_t(g.manager_records.at(rec + offset + 3)) << 24;
        return int32_t(v);
    };
    if (request.kind == ui::BankRequestKind::move_to_deposit) {
        if (!request.amount || request.asset_index != -1 || request.count != 0 ||
            int64_t(request.amount > 0 ? request.amount : -int64_t(request.amount)) > 99999999) {
            error = "invalid_amount"; return false;
        }
        const int64_t available = request.amount > 0 ? read_signed(0x314) : read_signed(0x318);
        if (available < 0 || int64_t(request.amount > 0 ? request.amount : -int64_t(request.amount)) > available) {
            error = "amount_unavailable"; return false;
        }
        move_to_deposit(g, request.manager_slot, request.amount);
    } else if (request.kind == ui::BankRequestKind::buy_shares || request.kind == ui::BankRequestKind::sell_shares ||
               request.kind == ui::BankRequestKind::stock_tip) {
        if (request.asset_index < 0 || request.asset_index >= kStocks ||
            g.stocks.size() < size_t(kStocks) * 0x5c) { error = "invalid_stock"; return false; }
        const size_t at = size_t(request.asset_index) * 0x5c;
        const int price = int(uint16_t(g.stocks.at(at + 0x4e) | uint16_t(g.stocks.at(at + 0x4f)) << 8));
        if (request.kind == ui::BankRequestKind::buy_shares) {
            if (request.count <= 0 || request.count > 9999 || price <= 0 ||
                request.count > max_shares_to_buy(g, request.manager_slot, request.asset_index)) {
                error = "invalid_share_count"; return false;
            }
            const int64_t raw = int64_t(price) * request.count;
            const int64_t total = raw + raw * 15 / 1000;
            if (total > int64_t(read_signed(0x314)) + read_signed(0x31c)) {
                error = "insufficient_funds"; return false;
            }
            if (!buy_shares(g, request.manager_slot, request.asset_index, request.count)) {
                error = "investment_limit"; return false;
            }
        } else if (request.kind == ui::BankRequestKind::sell_shares) {
            const size_t held_at = at + 0x50 + size_t(request.manager_slot) * 2;
            const int held = int(uint16_t(g.stocks.at(held_at) | uint16_t(g.stocks.at(held_at + 1)) << 8));
            if (request.count <= 0 || request.count > held) { error = "invalid_share_count"; return false; }
            sell_shares(g, request.manager_slot, request.asset_index, request.count);
        } else {
            if (request.count != 0 || request.amount != 0) { error = "invalid_tip_request"; return false; }
            presentation.last_tip = stock_tip(g, request.manager_slot, request.asset_index, random);
        }
    } else if (request.kind == ui::BankRequestKind::buy_fund || request.kind == ui::BankRequestKind::sell_fund) {
        if (request.amount != 0 || request.asset_index < 0 || request.asset_index >= kFunds || request.count <= 0 || request.count > 10 ||
            g.memory.u32(::bmh::layout::FundBlock + uint32_t(request.asset_index) * 0x1c) == 0) {
            error = "invalid_fund_request"; return false;
        }
        const uint32_t fund_at = ::bmh::layout::FundBlock + uint32_t(request.asset_index) * 0x1c;
        const uint16_t held = g.memory.u16(fund_at + 0x14 + uint32_t(request.manager_slot) * 2);
        if (request.kind == ui::BankRequestKind::buy_fund) {
            if (request.count > max_fund_units(g, request.manager_slot, request.asset_index)) {
                error = "invalid_fund_count"; return false;
            }
            const int32_t additional_cost = buy_fund_units(g, request.manager_slot, request.asset_index, request.count, game);
            if (request.asset_index != 0) {
                const std::string text = message_text(g, ::bmh::layout::CareerSession_Int32T_TextPointer2) + format_number(g, additional_cost, 0x101) +
                                         message_text(g, ::bmh::layout::CareerSession_Int32T_TextPointer);
                property_notice = ui::BankState::PropertyNotice{request.asset_index, additional_cost, ui::original_to_utf8(text)};
            }
        } else {
            if (request.count > held) { error = "invalid_fund_count"; return false; }
            sell_fund_units(g, request.manager_slot, request.asset_index, request.count);
        }
    } else if (request.kind == ui::BankRequestKind::bank_credit) {
        if (request.amount != 0 || request.asset_index != -1 || request.count != 0 || request.credit_edits.empty()) {
            error = "invalid_credit_request"; return false;
        }
        if (request.credit_counterparty != 0) {
            // Source case 5 scans configured human managers, excluding the borrower.
            // A runtime national-manager entry is not an additional human lender.
            const int configured = int(g.memory.u8(::bmh::layout::ManagerCount));
            if (request.credit_counterparty < 1 || request.credit_counterparty > 4) {
                error = "invalid_credit_request"; return false;
            }
            const int lender = request.credit_counterparty - 1;
            if (configured < 1 || configured > 4 || request.manager_slot >= configured ||
                lender < 0 || lender >= configured || lender == request.manager_slot) {
                error = "invalid_credit_request"; return false;
            }
        }
        const auto result = commit_bank_credit(g, exe, request.manager_slot,
            request.expected_credit, request.credit_edits, request.accept_credit_fees, request.credit_counterparty);
        if (result.status != BankCreditStatus::accepted) {
            error = result.status == BankCreditStatus::no_change ? "credit_no_change" :
                    result.status == BankCreditStatus::stale_request ? "stale_credit_request" :
                    result.status == BankCreditStatus::confirmation_required ? "credit_confirmation_required" :
                    "invalid_credit_request";
            return false;
        }
        if (result.refund != 0) {
            // "Die Bank" (CareerSession_Int32T_TextPointer3) or, in case 5, the lender's name (record +0xa8)
            const int lender = request.credit_counterparty - 1;
            std::string who = message_text(g, ::bmh::layout::CareerSession_Int32T_TextPointer3);
            if (lender >= 0) {
                who.clear();
                for (size_t i = 0; i < 26; ++i) {
                    const uint8_t ch = g.manager_records.at(size_t(lender) * 0xb0c + 0xa8 + i);
                    if (!ch) break;
                    who += char(ch);
                }
            }
            const std::string text = who + message_text(g, ::bmh::layout::CareerSession_Int32T_TextPointer4) +
                format_number(g, result.refund, 0x101) + message_text(g, ::bmh::layout::CareerSession_Int32T_TextPointer5);
            credit_notice = ui::BankState::CreditNotice{result.refund, ui::original_to_utf8(text)};
        }
    } else {
        error = "unsupported_bank_request"; return false;
    }
    presentation.property_notice=std::move(property_notice);
    presentation.credit_notice=std::move(credit_notice);
    ++revision;
    return true;
}
}
