// SPDX-License-Identifier: GPL-3.0-only
#include "game/desk_actions.hpp"
#include "game/state.hpp"
#include "ui/outgoing_transfer.hpp"
namespace bmh::ui {
OutgoingTransferEditorState
outgoing_transfer_from_game(const GameState& game,
                            const TransferOfferEditorState& owner,
                            uint64_t revision, bool gradient) {
    ui::OutgoingTransferEditorState out;
    const auto& request = owner.request;
    out.manager_slot = request.manager;
    out.runtime_club = request.runtime_club;
    out.revision = revision;
    out.list_entry = request.list_entry;
    out.source_slot = request.source_slot;
    out.player = request.expected_player;
    out.seller = request.expected_seller_word == 0xffff
                     ? 0x0fff
                     : request.expected_seller_word & 0x7fff;
    out.bid_slot = owner.bid_slot;
    out.source_mode = owner.mode;
    out.editable = owner.editable;
    out.closed = owner.closed;
    out.quote_loan = owner.quote_loan;
    out.initial_action_loan = request.loan;
    out.gradient_text = gradient;
    out.fee = owner.fee;
    out.monthly_salary = owner.salary;
    out.duration_years = owner.years;
    out.bonuses = owner.bonuses;
    const auto copy_profile = [](const TransferOfferInputProfile& p) {
        return ui::OutgoingTransferCalculatorProfile{
            p.supported,           p.currency, p.minimum, p.maximum, p.initial,
            p.source_text_address, p.presets};
    };
    for (size_t n = 0; n < 2; ++n)
        out.money_calculators[n] = copy_profile(owner.money_profiles[n]);
    for (size_t n = 0; n < 8; ++n)
        out.mode3_calculators[n] = copy_profile(owner.mode3_profiles[n]);
    if (owner.closed)
        return out; // No modal is constructed after Source cleanup.
    IncomingTransferDecisionView view;
    view.source_outgoing_editor_context = true;
    view.valid = true;
    view.list_entry = request.list_entry;
    view.player = out.player;
    view.seller = out.seller;
    view.seller_manager =
        out.seller == 0x0fff ? -1 : game.manager_of(out.seller);
    view.viewer_manager = request.manager;
    view.asking_fee = owner.asking_fee;
    view.asking_monthly_salary = owner.asking_salary;
    view.quoted_fee = owner.quoted_fee;
    view.quoted_monthly_salary = owner.quoted_salary;
    view.quoted_years = owner.years;
    const uint32_t list =
        ::bmh::layout::EditorList + uint32_t(request.list_entry) * 0x58;
    for (uint32_t n = 0; n < 4; ++n) {
        auto& row = view.offers[n];
        const auto buyer = game.memory.u16(list + n * 2);
        row.bid_index = int(n);
        row.present = buyer != 0xffff;
        row.buyer = row.present ? int(buyer) : -1;
        row.buyer_manager = row.present ? game.manager_of(row.buyer) : -1;
        row.fee = game.memory.u32(list + 0x1c + n * 4);
        row.monthly_salary = game.memory.u32(list + 0x0c + n * 4);
        const auto raw_term = game.memory.u8(list + 0x34 + n);
        row.loan = (raw_term & 0x80) != 0;
        row.contract_months = raw_term & 0x7f;
        row.eligible = row.present;
        for (uint32_t k = 0; k < 7; ++k)
            row.bonuses[k] = game.memory.u8(list + 0x38 + n * 7 + k);
    }
    out.panel = incoming_transfer_decision_from_game(game, view);
    return out;
}
} // namespace bmh::ui
