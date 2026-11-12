// SPDX-License-Identifier: GPL-3.0-only
// CareerSession: club management: training, training camp, bank, stadium, toto.
// (moved out of career_session.cpp unchanged; the class is declared in app/career_session.hpp)
#include "app/career_session_internal.hpp"

namespace bmh::app {
namespace {

constexpr uint8_t kBlockSeat = 1;

constexpr uint8_t kBlockRoof = 2;

constexpr uint8_t kBlockFeatures = kBlockSeat | kBlockRoof;

// True for the stadium requests that belong to the block-expansion dialog (select, total,
// feature toggle, cancel, confirm, answer tip).
bool is_block_expansion_action(ui::StadiumRequestKind kind) {
    return kind == ui::StadiumRequestKind::expansion_select ||
        kind == ui::StadiumRequestKind::expansion_set_total ||
        kind == ui::StadiumRequestKind::expansion_toggle_feature ||
        kind == ui::StadiumRequestKind::expansion_cancel ||
        kind == ui::StadiumRequestKind::expansion_confirm ||
        kind == ui::StadiumRequestKind::expansion_answer_tip;
}

}  // namespace

// Training group: one request changes the types, areas, automation byte or one player's
// intensity or focus of the active manager. The request carries the desired result; it is
// validated against the current projection (stale manager, plan limits, expected values), applied
// on a staged copy and committed with revision plus one. Manual edits clear the matching
// assistant flag and never draw random numbers; initialize and enabling automation run the
// automatic training. Returns false with a short error code in 'error' otherwise.
bool CareerSession::dispatch_training(const ui::TrainingRequest& request, uint64_t& revision,
                                      std::string& error) {
    if (balance_draft_ || balance_dispatch_active_ || strengths_owner_ || strengths_dispatch_active_ ||
        top_scorers_owner_ || top_scorers_dispatch_active_ || stadium_.block_expansion || other_games_owner_ || other_games_dispatch_active_) { error = "strengths_active"; return false; }
    return apply_training_request_source(g_,exe_,request,revision,error);
}

// Training camp group: set_focus changes the four focus bytes of the active manager (an unchanged
// or invalid focus is refused); book books a resort for the week after checking the focus the UI
// saw, the weekly/visit limits and the funds (the booking draws random numbers). Refused while a
// modal owner is open or when the manager slot, club or revision is stale; revision plus one on
// success, short error codes in 'error' otherwise.
bool CareerSession::dispatch_training_camp(const ui::TrainingCampRequest& request, uint64_t& revision,
                                          std::string& error) {
    if (balance_draft_ || balance_dispatch_active_ || strengths_owner_ || strengths_dispatch_active_ ||
        top_scorers_owner_ || top_scorers_dispatch_active_ || stadium_.block_expansion || other_games_owner_ || other_games_dispatch_active_) { error = "strengths_active"; return false; }
    return apply_training_camp_request_source(g_,rand_,request,revision,error);
}

// Bank group: deposit moves, share and fund trades, stock tips and bank credit. Needs the active
// manager, club and current revision and no modal owner open. Every branch validates amounts and
// counts against the manager record and the stock/fund blocks, then applies the change to the
// live state and bumps the revision once. The notices of the last property purchase or credit
// refund are kept for the next projection. Returns false with a short error code otherwise.
bool CareerSession::dispatch_bank(const ui::BankRequest& request, uint64_t& revision, std::string& error) {
    if (balance_draft_ || balance_dispatch_active_ || strengths_owner_ || strengths_dispatch_active_ ||
        top_scorers_owner_ || top_scorers_dispatch_active_ || stadium_.block_expansion || other_games_owner_ || other_games_dispatch_active_) { error = "strengths_active"; return false; }
    BankSourcePresentation presentation{last_stock_tip_,{}, {}};
    if(!apply_bank_request_source(g_,exe_,game_,rand_,request,revision,presentation,error))return false;
    last_stock_tip_=presentation.last_tip;
    last_property_notice_=std::move(presentation.property_notice);
    last_credit_notice_=std::move(presentation.credit_notice);
    return true;
}

// True when a construction tip dialog is open and the process timer (when running) has no time
// left; read-only.
bool CareerSession::stadium_tip_deadline_expired() const {
    if (!stadium_.opened || (!stadium_.tip &&
        (!stadium_.block_expansion || !stadium_.block_expansion->tip))) return false;
    const auto byte = [this](uint32_t address) {
        if (g_.memory.has(address)) return g_.memory.u8(address);
        const auto found = club_modal_runtime_.find(address);
        return found == club_modal_runtime_.end() ? exe_.u8(address) : found->second;
    };
    const auto word = [&byte](uint32_t address) {
        uint32_t value = 0;
        for (unsigned i = 0; i < 4; ++i) value |= uint32_t(byte(address + i)) << (8 * i);
        return value;
    };
    return word(::bmh::layout::CareerSession_DispatchStrengths_ExecutableField2) != 0 && int32_t(word(::bmh::layout::CareerSession_FormatError_ExecutableField2)) < 1;
}

// Stadium group: an owner (stadium_) is opened with a scope id and every later request must echo
// scope and sequence, the manager, club, stable club identity, game mode and revision. All
// actions (data page, periphery quote/build, block expansion editor with its tip, investments)
// run inside the apply lambda below; on failure the stadium checkpoint is restored, on success
// revision, sequence and the owner's revision advance. Exceptions restore the checkpoint and
// the revision and are rethrown. Short error codes in 'error' otherwise.
// Older periphery callers send no scope envelope; a compatibility step fills it in for their four
// original operations only.
bool CareerSession::dispatch_stadium(const ui::StadiumRequest& request,uint64_t& revision,std::string& error,
    const ConstructionNoticeHook& notice,const SeasonTicketCompensationNoticeHook& compensation) {
    if (balance_draft_ || balance_dispatch_active_ || strengths_owner_ || strengths_dispatch_active_ ||
        top_scorers_owner_ || top_scorers_dispatch_active_ || other_games_owner_ || other_games_dispatch_active_) { error = "strengths_active"; return false; }
    return apply_stadium_request_source(g_,exe_,game_,rand_,stadium_,next_stadium_scope_,request,revision,error,notice,compensation);
}

// Submits a pools coupon for the active manager; only valid when the coupon has eleven matches.
// Revision plus one; false with a short error code otherwise.
bool CareerSession::dispatch_toto(const ui::TotoRequest& request, uint64_t& revision, std::string& error) {
    if (balance_draft_ || balance_dispatch_active_ || strengths_owner_ || strengths_dispatch_active_ ||
        top_scorers_owner_ || top_scorers_dispatch_active_ || stadium_.block_expansion || other_games_owner_ || other_games_dispatch_active_) { error = "strengths_active"; return false; }
    return apply_toto_request_source(g_,request,revision,error);
}

}  // namespace bmh::app
