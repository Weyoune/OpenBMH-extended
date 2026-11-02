// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "app/contract_source_types.hpp"
#include "game/desk_actions.hpp"
#include "game/state.hpp"
#include "ui/contracts.hpp"
#include "ui/outgoing_transfer.hpp"
namespace bmh::app::world12 {
// A synchronous modal owns copies until the frontend explicitly completes.
// Destruction/cancellation leaves both the caller state and its random streams.
class MonthlyRenewalModal {
  public:
    MonthlyRenewalModal(GameState&, WatcomRandom&, int local_manager,
                        int expired_row, const TextDb&, uint64_t revision = 0);
    ui::ContractsState view() const;
    bool apply(const ui::ContractRequest&, ui::ContractActionResult&,
               std::string& error);
    std::optional<bool> finish(bool completed);
    bool renewed() const { return renewed_; }

  private:
    GameState& live_;
    WatcomRandom& live_random_;
    GameState state_;
    WatcomRandom random_;
    const TextDb& texts_;
    int manager_ = -1, expired_ = -1, player_ = -1;
    uint8_t active_ = 0;
    uint64_t revision_ = 0, next_quote_ = 1, last_request_ = 0;
    std::optional<app::ContractSourceQuote> quote_;
    bool renewed_ = false, finished_ = false;
};
class LoanReturnOfferModal {
  public:
    LoanReturnOfferModal(GameState&, GameRandom&, WatcomRandom&,
                         int local_manager, int source_slot, int entry,
                         uint64_t revision = 0);
    ui::OutgoingTransferEditorState view() const;
    bool apply(const ui::OutgoingTransferRequest&, const HumanBuyerSaleHooks&,
               std::string& error,
               const std::function<bool(GameState&, int)>& team_info = {});
    int finish(bool completed);
    bool closed() const { return owner_.closed; }
    GameState& staged() { return state_; }

  private:
    GameState& live_;
    GameRandom& live_game_;
    WatcomRandom& live_random_;
    GameState state_;
    GameRandom game_;
    WatcomRandom random_;
    TransferOfferEditorState owner_;
    uint64_t revision_ = 0;
    bool finished_ = false,submitted_=false;
};
} // namespace bmh::app::world12
