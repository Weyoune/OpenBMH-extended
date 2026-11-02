// SPDX-License-Identifier: GPL-3.0-only
#include "app/world12_clock_presenters.hpp"
#include "app/world12_session.hpp"
#include "formats/palette.hpp"
#include "game/month.hpp"
#include "game/save.hpp"
#include "game/world12/monthly_rosters.hpp"
#include "ui/pending_offer.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace bmh;
namespace {
void check(bool value, const char* text) {
    if (!value)
        throw std::runtime_error(text);
}
template <class F> void rejects(F action, const char* text) {
    bool rejected = false;
    try {
        action();
    } catch (const std::exception&) {
        rejected = true;
    }
    check(rejected, text);
}
GameState fixture() {
    GameState g;
    g.level = 3;
    g.manager_teams = {0};
    g.manager_records.resize(0xb0c);
    g.tactics.resize(0x840);
    g.league_rounds.assign(9, 0);
    g.results.assign(9 * 0x17c, 0xcc);
    g.squad_slots.assign(size_t(runtime_dimensions.total_slots), 0xfff);
    g.squad_slots[0] = 0x8000;
    g.players.resize(1);
    g.players[0] = {"Synthetic", 60u | (70u << 7) | (75u << 14), 5u << 21, 0};
    g.club_ids.assign(419, 0);
    g.club_names = {'C', 'l', 'u', 'b', 0};
    g.club_heap_78.resize(419 * 2);
    g.player_names = {'P', 'l', 'a', 'y', 'e', 'r', 0};
    g.player_name_offsets = {0};
    g.player_cards = {0};
    g.player_bonus = {0};
    g.memory.add_block(layout::ActiveManager, 1);
    g.memory.add_block(layout::GameMode, 1);
    g.memory.add_block(layout::TextGradientDisabled, 1);
    g.memory.add_block(layout::PlainBackground, 1);
    g.memory.set8(layout::GameMode, 3);
    g.tactics[0xd] = 100;
    g.tactics[0x11] = 12;
    g.tactics[0x50] = 0xa0;
    g.tactics[0x51] = 0x0f;
    return g;
}
void actual(const std::filesystem::path& data,
            const std::filesystem::path& input,
            const std::filesystem::path& loan_input,
            const std::filesystem::path& output) {
    namespace w = bmh::world12;
    namespace a = bmh::app::world12;
    const auto resources = a::load_runtime_resources(data);
    auto source = w::read_save(load_savegame(input));
    const auto seat = *source.desktop->active_seat;
    auto& manager = *std::find_if(
        source.world.managers.begin(), source.world.managers.end(),
        [&](const auto& manager) { return manager.seat == seat; });
    int target = -1;
    for (int row = 0; row < 24; ++row) {
        const auto& slot =
            source.world.clubs.at(manager.club).roster[size_t(row)];
        if (!slot.empty() &&
            (manager.tactics->backing->scalar_rows[size_t(row)][0xb] & 0x60) ==
                0 &&
            (manager.tactics->backing->scalar_rows[size_t(row)][0x4f] & 0x80) ==
                0) {
            target = row;
            break;
        }
    }
    check(target >= 0, "actual contract fixture has no eligible player");
    manager.tactics->backing->scalar_rows[size_t(target)][0xc] = 1;
    const auto capture = [&](const std::string& name, const Image& frame,
                             const w::CareerSnapshot& snapshot) {
        if (output.empty())
            return;
        std::filesystem::create_directories(output);
        const auto palette = load_palette(ui::asset_path(data, "M3PC.COL"));
        std::ofstream file(output / (name + ".ppm"), std::ios::binary);
        file << "P6\n320 240\n255\n";
        for (auto pixel : frame.pixels) {
            const auto c = palette.argb[pixel];
            file.put(char(c >> 16));
            file.put(char(c >> 8));
            file.put(char(c));
        }
        write_savegame(output / (name + ".FBM"), w::make_save(snapshot));
    };
    auto cancelled = source;
    w::MonthlyContractHooks stopped;
    stopped.renew = [&](GameState& g, GameRandom&, WatcomRandom& random,
                        uint8_t actor, w::PlayerId, int row,
                        const w::AiMatchProjection& p) -> std::optional<bool> {
        size_t local = 0;
        while (p.global_manager(uint8_t(local)) != actor)
            ++local;
        a::MonthlyRenewalModal modal(g, random, int(local), row,
                                     *resources.texts);
        return modal.finish(false);
    };
    rejects(
        [&] {
            w::settle_monthly_roster(cancelled, seat, resources.reference,
                                     *resources.texts, stopped);
        },
        "actual interrupted renewal committed");
    check(cancelled == source, "actual interrupted renewal leaked billing/RNG");
    auto renewed = source;
    size_t calls = 0;
    w::MonthlyContractHooks interactive;
    interactive.renew =
        [&](GameState& g, GameRandom&, WatcomRandom& random, uint8_t actor,
            w::PlayerId person, int row,
            const w::AiMatchProjection& p) -> std::optional<bool> {
        ++calls;
        size_t local = 0;
        while (p.global_manager(uint8_t(local)) != actor)
            ++local;
        check(actor == seat && row == target &&
                  p.global_player(
                      g.squad_slots.at(size_t(g.manager_teams[local]) * 24 +
                                       size_t(row)) &
                      0xfff) == person,
              "actual renewal lost canonical owner");
        a::MonthlyRenewalModal modal(g, random, int(local), row,
                                     *resources.texts, 7);
        const auto view = modal.view();
        const auto& crest =
            source.world.crests.at(source.world.clubs.at(manager.club).crest);
        auto decorated = view;
        decorated.badge_atlas = crest.path;
        decorated.badge_sprite = crest.cell;
        capture("contract-expired",
                ui::ContractsScreen(data, decorated, resources.german).render(),
                source);
        ui::ContractRequest request;
        request.manager_slot = int(local);
        request.runtime_club = g.manager_teams[local];
        request.row = row;
        request.player_id = int(p.local_player(person));
        request.request_id = 1;
        request.revision = 7;
        ui::ContractActionResult result;
        std::string error;
        if (!modal.apply(request, result, error))
            throw std::runtime_error(error);
        const auto quote = modal.view();
        request.kind = ui::ContractActionKind::submit_offer;
        request.request_id = 2;
        request.revision = quote.revision;
        request.quote_id = quote.quote_id;
        request.demand = quote.demands->at(0);
        request.monthly_salary = std::min<uint32_t>(
            200000, std::max<uint32_t>(1, request.demand * 2));
        request.proposed_bonuses =
            std::find_if(quote.players.begin(), quote.players.end(),
                         [&](const auto& player) { return player.row == row; })
                ->team_premiums;
        if (!modal.apply(request, result, error))
            throw std::runtime_error(error);
        check(result.accepted, "actual Core contract offer refused");
        return modal.finish(true);
    };
    const auto outcome = w::settle_monthly_roster(
        renewed, seat, resources.reference, *resources.texts, interactive);
    check(calls == 1 && outcome.released == 0 &&
              std::find_if(
                  renewed.world.managers.begin(), renewed.world.managers.end(),
                  [&](const auto& m) {
                      return m.seat == seat;
                  })->tactics->backing->scalar_rows[size_t(target)][0xc] == 12,
          "actual renewed Source term did not publish");
    check(w::read_save(w::make_save(renewed)) == renewed,
          "actual monthly renewal failed FBM roundtrip");
    if (!output.empty())
        write_savegame(output / "contract-renewed.FBM", w::make_save(renewed));
    auto loans = w::read_save(load_savegame(loan_input));
    check(!loans.world.loans.empty(),
          "actual loan fixture has no Core-created ledger");
    const auto leased = loans.world.loans.front().player;
    loans.world.loans.front().months = 1;
    const auto before = loans;
    size_t questions = 0, editors = 0;
    w::LoanReturnHooks hooks;
    hooks.active_seat = *loans.desktop->active_seat;
    hooks.keep = [&](const GameState& g, uint8_t actor,
                     const std::string& question,
                     const w::AiMatchProjection& p) -> std::optional<bool> {
        ++questions;
        size_t local = 0;
        while (p.global_manager(uint8_t(local)) != actor)
            ++local;
        auto view = ui::loan_return_question_dialog(g, int(local), question);
        const auto& crest = loans.world.crests.at(
            loans.world.clubs
                .at(p.global_club(uint32_t(g.manager_teams[local])))
                .crest);
        view.badge_atlas = crest.path;
        view.badge_sprite = crest.cell;
        ui::GameEndTaxDialog dialog(data, view, {}, std::nullopt,
                                    resources.german);
        capture("loan-question", dialog.render(), before);
        check(dialog.hit_test(100, 171) == ui::GameEndTaxDialog::Button::yes,
              "Source yes/no hitbox changed");
        return true;
    };
    hooks.offer = [&](GameState& g, GameRandom& game, WatcomRandom& random,
                      uint8_t actor, int slot, int entry,
                      const HumanBuyerSaleHooks&,
                      const w::AiMatchProjection& p) -> int {
        ++editors;
        size_t local = 0;
        while (p.global_manager(uint8_t(local)) != actor)
            ++local;
        a::LoanReturnOfferModal modal(g, game, random, int(local), slot, entry,
                                      7);
        const auto editor = modal.view();
        auto decorated = editor;
        const auto& crest = loans.world.crests.at(
            loans.world.clubs.at(p.global_club(uint32_t(editor.runtime_club)))
                .crest);
        const auto& seller = loans.world.crests.at(
            loans.world.clubs.at(p.global_club(uint32_t(editor.seller))).crest);
        decorated.panel.badge_atlas = crest.path;
        decorated.panel.badge_sprite = crest.cell;
        decorated.panel.seller_badge_atlas = seller.path;
        decorated.panel.seller_badge_sprite = seller.cell;
        capture("loan-return-editor",
                ui::OutgoingTransferScreen(data, decorated, resources.german)
                    .render(),
                before);
        ui::OutgoingTransferRequest request;
        request.kind = ui::OutgoingTransferRequestKind::cancel;
        request.identity = {editor.manager_slot, editor.runtime_club,
                            editor.revision,     editor.list_entry,
                            editor.source_slot,  editor.player,
                            editor.seller,       editor.bid_slot};
        request.fee = editor.fee;
        request.monthly_salary = editor.monthly_salary;
        request.duration_years = editor.duration_years;
        request.bonuses = editor.bonuses;
        request.quote_loan = editor.quote_loan;
        request.quoted_fee = editor.panel.quoted_fee;
        request.quoted_monthly_salary = editor.panel.quoted_monthly_salary;
        request.quoted_years = editor.panel.quoted_years;
        std::string error;
        if (!modal.apply(request, {}, error))
            throw std::runtime_error(error);
        return modal.finish(true);
    };
    const auto returned = w::return_monthly_loans(loans, resources.reference,
                                                  *resources.texts, hooks);
    check(questions == 1 && editors == 1 && returned.returned == 1 &&
              loans.world.loans.empty() &&
              w::read_save(w::make_save(loans)) == loans,
          "actual return-editor cancel did not complete the Source return");
    if (!output.empty())
        write_savegame(output / "loan-returned.FBM", w::make_save(loans));
    auto retained = before;
    size_t real_notices = 0;
    hooks.offer = [&](GameState& g, GameRandom& game, WatcomRandom& random,
                      uint8_t actor, int slot, int entry,
                      const HumanBuyerSaleHooks&,
                      const w::AiMatchProjection& p) -> int {
        size_t local = 0;
        while (p.global_manager(uint8_t(local)) != actor)
            ++local;
        a::LoanReturnOfferModal modal(g, game, random, int(local), slot, entry,
                                      7);
        HumanBuyerSaleHooks sale;
        sale.allocator_gate =
            resources.german.u8(layout::MatchEventsDisabled) == 0
                ? HumanBuyerSaleAllocatorGate::zero
                : HumanBuyerSaleAllocatorGate::blocked;
        sale.direct_mode = HumanBuyerSaleDirectMode::ordinary_ai_purchase;
        sale.notice = [&](const GameState& current,
                          const HumanBuyerSaleNotice& notice) {
            ++real_notices;
            const PendingOfferNotice source_notice{
                notice.manager, notice.raw_offer_slot,
                notice.source_text_address, notice.original_message};
            const auto panel =
                ui::pending_offer_notice_dialog(current, source_notice);
            capture("loan-buyback-notice",
                    ui::GameEndTaxDialog(data, panel, {}, std::nullopt,
                                         resources.german)
                        .render(),
                    before);
        };
        const auto request = [&](ui::OutgoingTransferRequestKind kind,
                                 ui::OutgoingTransferField field,
                                 uint32_t value) {
            const auto e = modal.view();
            ui::OutgoingTransferRequest r;
            r.kind = kind;
            r.field = field;
            r.value = value;
            r.identity = {e.manager_slot, e.runtime_club, e.revision,
                          e.list_entry,   e.source_slot,  e.player,
                          e.seller,       e.bid_slot};
            r.fee = e.fee;
            r.monthly_salary = e.monthly_salary;
            r.duration_years = e.duration_years;
            r.bonuses = e.bonuses;
            r.quote_loan = e.quote_loan;
            r.quoted_fee = e.panel.quoted_fee;
            r.quoted_monthly_salary = e.panel.quoted_monthly_salary;
            r.quoted_years = e.panel.quoted_years;
            if (kind == ui::OutgoingTransferRequestKind::quote_change) {
                if (field == ui::OutgoingTransferField::fee)
                    r.fee = value;
                else if (field == ui::OutgoingTransferField::monthly_salary)
                    r.monthly_salary = value;
            }
            return r;
        };
        std::string error;
        const auto first = modal.view();
        if (!modal.apply(request(ui::OutgoingTransferRequestKind::quote_change,
                                 ui::OutgoingTransferField::fee,
                                 first.panel.asking_fee),
                         sale, error))
            throw std::runtime_error(error);
        if (!modal.apply(request(ui::OutgoingTransferRequestKind::quote_change,
                                 ui::OutgoingTransferField::monthly_salary,
                                 first.money_calculators[1].maximum),
                         sale, error))
            throw std::runtime_error(error);
        if (!modal.apply(request(ui::OutgoingTransferRequestKind::submit,
                                 ui::OutgoingTransferField::fee, 0),
                         sale, error))
            throw std::runtime_error(error);
        check(modal.closed(),
              "actual return buyback did not complete Source settlement");
        return modal.finish(true);
    };
    const auto kept = w::return_monthly_loans(retained, resources.reference,
                                              *resources.texts, hooks);
    check(real_notices > 0 && kept.returned == 0 &&
              retained.world.loans.empty(),
          "actual buyback lost its supported Source result");
    const auto borrower = before.world.loans.front().borrower;
    check(std::any_of(retained.world.clubs.at(borrower).roster.begin(),
                      retained.world.clubs.at(borrower).roster.end(),
                      [&](const auto& row) { return row.player == leased; }) &&
              w::read_save(w::make_save(retained)) == retained,
          "actual return buyback lost canonical player/save");
    if (!output.empty())
        write_savegame(output / "loan-bought-back.FBM", w::make_save(retained));
    std::cout << "Actual Source monthly renewal, cancellation rollback and "
                 "Core-created loan return question/editor passed\n";
}
} // namespace
int main(int argc, char** argv) try {
    if (argc == 5) {
        actual(argv[1], argv[2], argv[3], argv[4]);
        return 0;
    }
    TextDb texts;
    texts.strings.assign(TEXT_COUNT, "Synthetic result");
    auto g = fixture();
    g.texts = &texts;
    WatcomRandom random(91);
    const auto before = g;
    const auto initial = random;
    {
        app::world12::MonthlyRenewalModal modal(g, random, 0, 0, texts, 7);
        const auto view = modal.view();
        check(!view.player_details_available &&
                  view.players[0].remaining_months == 0,
              "monthly contract view lost expiry/card boundary");
        ui::ContractRequest request;
        request.manager_slot = 0;
        request.runtime_club = 0;
        request.row = 0;
        request.player_id = 0;
        request.request_id = 1;
        request.revision = 7;
        ui::ContractActionResult result;
        std::string error;
        if (!modal.apply(request, result, error))
            throw std::runtime_error(error);
        const auto quote = modal.view();
        auto oracle = before;
        auto expected = initial;
        const auto demands = contract_demands(oracle, 0, 0, false, expected);
        check(quote.demands == demands,
              "monthly demands used desk-only refusal policy");
        auto stale = request;
        stale.kind = ui::ContractActionKind::submit_offer;
        stale.revision = 8;
        stale.request_id = 2;
        stale.quote_id = quote.quote_id + 1;
        stale.demand = demands[0];
        stale.monthly_salary = 200000;
        check(!modal.apply(stale, result, error),
              "monthly modal accepted stale quote");
        request.kind = ui::ContractActionKind::submit_offer;
        request.revision = quote.revision;
        request.request_id = 2;
        request.quote_id = quote.quote_id;
        request.demand = demands[0];
        request.monthly_salary = 200000;
        const bool accepted = contract_offer(
            oracle, 0, 0, 0, request.monthly_salary, request.demand, false,
            true, expected, &request.proposed_bonuses);
        check(accepted, "synthetic monthly renewal oracle refused");
        check(modal.apply(request, result, error) && result.accepted,
              "monthly renewal did not use the actual Core offer");
        check(g.tactics == before.tactics && random.seed() == initial.seed(),
              "monthly modal wrote live state before completion");
        check(!modal.finish(false),
              "interrupted monthly modal returned an answer");
        check(g.tactics == before.tactics && random.seed() == initial.seed(),
              "interrupted monthly modal leaked contract/RNG");
    }
    {
        app::world12::MonthlyRenewalModal modal(g, random, 0, 0, texts, 7);
        ui::ContractActionResult result;
        std::string error;
        ui::ContractRequest request;
        request.manager_slot = 0;
        request.runtime_club = 0;
        request.row = 0;
        request.player_id = 0;
        request.request_id = 1;
        request.revision = 7;
        if (!modal.apply(request, result, error))
            throw std::runtime_error(error);
        const auto quote = modal.view();
        request.kind = ui::ContractActionKind::submit_offer;
        request.request_id = 2;
        request.revision = quote.revision;
        request.quote_id = quote.quote_id;
        request.demand = quote.demands->at(0);
        request.monthly_salary = 200000;
        if (!modal.apply(request, result, error))
            throw std::runtime_error(error);
        check(modal.finish(true) == true && g.tactics[0xc] == 12,
              "completed renewal did not update real Source term");
        rejects([&] { modal.finish(true); }, "retired monthly modal reused");
    }
    {
        auto refused = fixture();
        refused.texts = &texts;
        WatcomRandom wr(91);
        app::world12::MonthlyRenewalModal modal(refused, wr, 0, 0, texts, 7);
        ui::ContractActionResult result;
        std::string error;
        ui::ContractRequest request;
        request.manager_slot = 0;
        request.runtime_club = 0;
        request.row = 0;
        request.player_id = 0;
        request.request_id = 1;
        request.revision = 7;
        if (!modal.apply(request, result, error))
            throw std::runtime_error(error);
        const auto quote = modal.view();
        request.kind = ui::ContractActionKind::submit_offer;
        request.request_id = 2;
        request.revision = quote.revision;
        request.quote_id = quote.quote_id;
        request.demand = quote.demands->at(0);
        request.monthly_salary = 0;
        check(modal.apply(request, result, error) && !result.accepted,
              "expired low offer unexpectedly accepted");
        check(modal.finish(true) == false && (refused.tactics[0xb] & 0x40) &&
                  (refused.tactics[0x4f] & 0x80),
              "expired-row Source refusal flags were lost");
    }
    auto status = LeImage::from_bytes(
        layout::WeeklyManagerStatusQuestion,
        Bytes{'S', 'y', 'n', 't', 'h', 'e', 't', 'i', 'c', 0});
    const auto panel =
        ui::weekly_offer_question_dialog(g, {0, 15, 0, 0, {}}, status);
    check(panel.body == "Synthetic" &&
              panel.kind == ui::TaxDialogKind::question &&
              panel.source_flags == 1,
          "weekly status question lost Source CString/yes-no flags");
    rejects(
        [&] {
            ui::weekly_offer_question_dialog(g, {0, 15, 0, 0, {}}, LeImage{});
        },
        "missing weekly status CString fabricated a question");
    std::cout << "Staged monthly renewal: Source demands/expiry offer, stale "
                 "quote, real term update and cancellation/RNG rollback; "
                 "weekly status CString passed\n";
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
