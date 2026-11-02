// SPDX-License-Identifier: GPL-3.0-only
#include "app/world12_clock_presenters.hpp"
#include "app/contract_request_source.hpp"
namespace bmh::app::world12 {
MonthlyRenewalModal::MonthlyRenewalModal(GameState& live, WatcomRandom& random,
                                         int manager, int row,
                                         const TextDb& texts, uint64_t revision)
    : live_(live), live_random_(random), state_(live), random_(random),
      texts_(texts), manager_(manager), expired_(row), revision_(revision) {
    if (manager < 0 || size_t(manager) >= state_.manager_teams.size() ||
        row < 0 || row >= 24)
        throw FormatError("world12: expired contract has no local owner");
    const auto range = squad_range(state_.manager_teams[size_t(manager)]);
    if (size_t(row) >= range.count)
        throw FormatError("world12: expired contract row outside roster");
    player_ = state_.squad_slots.at(range.first + size_t(row)) & 0xfff;
    if (player_ == 0xfff)
        throw FormatError("world12: expired contract points to empty row");
    active_ = state_.memory.u8(layout::ActiveManager);
    state_.memory.set8(layout::ActiveManager, uint8_t(manager));
    state_.texts = &texts_;
}
ui::ContractsState MonthlyRenewalModal::view() const {
    auto out = ui::contracts_from_game(state_, manager_, revision_);
    out.selected_row = expired_;
    out.player_details_available = false;
    if (quote_) {
        out.selected_row = out.quote_row = quote_->row;
        out.quote_id = quote_->quote_id;
        out.demands = quote_->demands;
    }
    return out;
}
bool MonthlyRenewalModal::apply(const ui::ContractRequest& request,
                                ui::ContractActionResult& result,
                                std::string& error) {
    if (finished_) {
        error = "world12: expired contract modal retired";
        return false;
    }
    const auto unchanged = [](const GameState& a, const GameState& b) {
        return a.manager_records == b.manager_records &&
               a.tactics == b.tactics && a.squad_slots == b.squad_slots &&
               a.players.size() == b.players.size() &&
               std::equal(a.players.begin(), a.players.end(), b.players.begin(),
                          [](const auto& x, const auto& y) {
                              return x.name == y.name && x.w35c == y.w35c &&
                                     x.w364 == y.w364 && x.w368 == y.w368;
                          });
    };
    if (!app::apply_contract_request_source(
            state_, texts_, random_, request, revision_, quote_, next_quote_,
            last_request_, result, error, unchanged, expired_))
        return false;
    if (request.kind == ui::ContractActionKind::submit_offer &&
        request.row == expired_ && result.accepted)
        renewed_ = true;
    return true;
}
std::optional<bool> MonthlyRenewalModal::finish(bool completed) {
    if (finished_)
        throw FormatError("world12: expired contract modal already retired");
    finished_ = true;
    if (!completed)
        return {};
    const auto range = squad_range(state_.manager_teams.at(size_t(manager_)));
    if ((state_.squad_slots.at(range.first + size_t(expired_)) & 0xfff) !=
        player_)
        throw FormatError("world12: expired contract player changed");
    if (renewed_ && !state_.tactics.at(size_t(manager_) * 0x840 +
                                       size_t(expired_) * 0x58 + 0xc))
        throw FormatError(
            "world12: expired contract acknowledgement has no renewal");
    state_.memory.set8(layout::ActiveManager, active_);
    live_ = std::move(state_);
    live_random_ = random_;
    return renewed_;
}
LoanReturnOfferModal::LoanReturnOfferModal(GameState& live, GameRandom& game,
                                           WatcomRandom& random, int manager,
                                           int slot, int entry,
                                           uint64_t revision)
    : live_(live), live_game_(game), live_random_(random), state_(live),
      game_(game), random_(random), revision_(revision) {
    if (manager < 0 || size_t(manager) >= state_.manager_teams.size() ||
        entry < 40 || entry >= 44 ||
        slot != runtime_dimensions.pool_first + entry)
        throw FormatError(
            "world12: return offer has no authenticated special row");
    const auto at = layout::EditorList + uint32_t(entry) * 0x58;
    int bid = -1;
    const auto team = state_.manager_teams[size_t(manager)];
    for (int n = 0; n < 4; ++n) {
        const auto word = state_.memory.u16(at + uint32_t(n * 2));
        if (word == 0xffff || word == team) {
            bid = n;
            break;
        }
    }
    TransferOfferEditorRequest request{
        manager,
        team,
        entry,
        slot,
        int(state_.squad_slots.at(size_t(slot)) & 0xfff),
        state_.memory.u16(at + 8),
        bid,
        false};
    request.loan_return_entry = true;
    owner_ = open_transfer_offer_editor_source(state_, request, game_, random_);
    if (!owner_.supported || owner_.closed)
        throw FormatError(owner_.reason.empty()
                              ? "world12: return editor refused Source entry"
                              : owner_.reason);
}
ui::OutgoingTransferEditorState LoanReturnOfferModal::view() const {
    auto out = ui::outgoing_transfer_from_game(
        state_, owner_, revision_,
        state_.memory.u8(layout::TextGradientDisabled) == 0);
    out.team_info_available = true;
    out.player_info_available = false;
    return out;
}
bool LoanReturnOfferModal::apply(
    const ui::OutgoingTransferRequest& request,
    const HumanBuyerSaleHooks& hooks, std::string& error,
    const std::function<bool(GameState&, int)>& team_info) {
    try {
        if (finished_)
            throw FormatError("world12: return editor retired");
        const auto& r = owner_.request;
        const auto& id = request.identity;
        const int seller = r.expected_seller_word == 0xffff
                               ? 0xfff
                               : r.expected_seller_word & 0x7fff;
        if (id.manager_slot != r.manager || id.runtime_club != r.runtime_club ||
            id.revision != revision_ || id.list_entry != r.list_entry ||
            id.source_slot != r.source_slot || id.player != r.expected_player ||
            id.seller != seller || id.bid_slot != owner_.bid_slot ||
            request.quoted_fee != owner_.quoted_fee ||
            request.quoted_monthly_salary != owner_.quoted_salary ||
            request.quoted_years != owner_.years)
            throw FormatError("world12: stale return editor identity/quote");
        auto fee = owner_.fee, salary = owner_.salary;
        auto years = owner_.years;
        auto bonuses = owner_.bonuses;
        bool loan = owner_.quote_loan;
        if (request.kind == ui::OutgoingTransferRequestKind::quote_change) {
            const auto field = unsigned(request.field);
            if (field > 10)
                throw FormatError("world12: invalid return editor field");
            if (request.accepted) {
                if (field == 0)
                    fee = request.value;
                else if (field == 1)
                    salary = request.value;
                else if (field == 2)
                    years = uint8_t(request.value);
                else if (field < 10)
                    bonuses[field - 3] =
                        uint8_t(field < 6 ? request.value : request.value / 5);
                else
                    loan = !loan;
            }
        }
        if (request.fee != fee || request.monthly_salary != salary ||
            request.duration_years != years || request.bonuses != bonuses ||
            request.quote_loan != loan)
            throw FormatError("world12: unowned return editor payload");
        auto next = state_;
        auto gr = game_;
        auto wr = random_;
        auto owner = owner_;
        if (request.kind == ui::OutgoingTransferRequestKind::quote_change)
            owner = edit_transfer_offer_editor_source(
                next, owner,
                {TransferOfferEditorField(unsigned(request.field)),
                 request.value, request.accepted},
                gr, wr);
        else if (request.kind == ui::OutgoingTransferRequestKind::cancel)
            owner = cancel_transfer_offer_editor_source(next, owner);
        else if (request.kind == ui::OutgoingTransferRequestKind::team_info) {
            const auto validated =
                cancel_transfer_offer_editor_source(next, owner);
            if (!validated.supported || !team_info || !team_info(next, seller))
                throw FormatError(
                    "world12: return editor team information interrupted");
            owner = resume_transfer_offer_team_info_source(next, owner, wr);
        } else if (request.kind == ui::OutgoingTransferRequestKind::submit) {
            MinuteReport report;
            auto submitted = submit_transfer_offer_editor_source(
                next, owner, wr, hooks, report);
            if (report.unsupported)
                throw FormatError(report.reason);
            owner = std::move(submitted.editor);
        } else
            throw FormatError("world12: return editor nested inspection "
                              "requires its own staged owner");
        if (!owner.supported)
            throw FormatError(owner.reason);
        state_ = std::move(next);
        game_ = gr;
        random_ = wr;
        owner_ = std::move(owner);
        if (request.kind == ui::OutgoingTransferRequestKind::submit &&
            owner_.closed)
            submitted_ = true;
        ++revision_;
        return true;
    } catch (const std::exception& failure) {
        error = failure.what();
        return false;
    }
}
int LoanReturnOfferModal::finish(bool completed) {
    if (finished_)
        throw FormatError("world12: return editor already retired");
    finished_ = true;
    if (!completed)
        return -1;
    if (!owner_.closed)
        throw FormatError("world12: return editor not explicitly closed");
    live_ = std::move(state_);
    live_game_ = game_;
    live_random_ = random_;
    return submitted_ ? 1 : 0;
}
} // namespace bmh::app::world12
