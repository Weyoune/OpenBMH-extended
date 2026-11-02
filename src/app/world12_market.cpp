// SPDX-License-Identifier: GPL-3.0-only
#include "app/world12_market.hpp"
#include "app/world12_session.hpp"
#include "app/world12_visible_menu.hpp"
#include "game/weekly.hpp"
#include "game/world12/desktop_projection.hpp"
#include "game/world12/economy.hpp"
#include "game/world12/market_auctions.hpp"
#include <algorithm>
namespace bmh::app::world12 {
using namespace bmh::world12;
namespace {
uint8_t local_actor(const AiMatchProjection& p, uint8_t seat) {
    for (uint8_t n = 0; n < p.human_managers(); ++n)
        if (p.global_manager(n) == seat)
            return n;
    throw FormatError("world12: market manager is not projected");
}
void install_market(AiMatchProjection& p, const CareerSnapshot& state) {
    auto& g = p.state;
    std::vector<PlayerId> players;
    std::vector<ClubId> clubs;
    for (const auto& row : state.world.transfer_market->rows)
        if (row) {
            players.push_back(row->player);
            clubs.push_back(row->seller);
            for (const auto& bid : row->bids)
                clubs.push_back(bid.bidder);
        }
    p.include_players(state.world, players);
    p.include_club_references(state.world, clubs);
    g.memory.add_block(layout::EditorList, 44 * 0x58);
    for (size_t row = 0; row < 44; ++row) {
        const auto at = layout::EditorList + uint32_t(row * 0x58);
        for (uint32_t b = 0; b < 0x58; ++b)
            g.memory.set8(at + b, 0);
        for (uint32_t b = 0; b < 4; ++b)
            g.memory.set16(at + b * 2, 0xffff);
        g.memory.set16(at + 8, 0xffff);
        g.squad_slots.at(size_t(runtime_dimensions.pool_first) + row) = 0xfff;
        if (!state.world.transfer_market->rows[row])
            continue;
        const auto& value = *state.world.transfer_market->rows[row];
        g.squad_slots.at(size_t(runtime_dimensions.pool_first) + row) =
            uint16_t(p.local_player(value.player));
        g.memory.set16(at + 8,
                       value.seller.valid()
                           ? uint16_t(p.local_club_reference(value.seller) |
                                      (value.human_seller ? 0x8000 : 0))
                           : 0xffff);
        g.memory.set32(at + 0x2c, value.fee);
        g.memory.set32(at + 0x30, value.salary);
        g.memory.set8(at + 0x54, value.asking_percent);
        g.memory.set8(at + 0x55, value.demand_percent);
        g.memory.set8(at + 0x56, value.weeks);
        for (size_t slot = 0; slot < 4; ++slot) {
            const auto& bid = value.bids[slot];
            g.memory.set16(at + uint32_t(slot * 2),
                           bid.bidder.valid()
                               ? uint16_t(p.local_club_reference(bid.bidder))
                               : 0xffff);
            g.memory.set32(at + 0x1c + uint32_t(slot * 4), bid.fee);
            g.memory.set32(at + 0xc + uint32_t(slot * 4), bid.salary);
            g.memory.set8(at + 0x34 + uint32_t(slot), bid.months);
            for (size_t n = 0; n < 7; ++n)
                g.memory.set8(at + 0x38 + uint32_t(slot * 7 + n),
                              bid.bonuses[n]);
        }
    }
    g.list_toggle = state.world.transfer_market->list_toggle;
}
void capture_market(CareerSnapshot& state, const AiMatchProjection& p) {
    auto& market = *state.world.transfer_market;
    const auto& g = p.state;
    for (size_t row = 0; row < 40; ++row) {
        const auto at = layout::EditorList + uint32_t(row * 0x58);
        const auto player =
            g.squad_slots.at(size_t(runtime_dimensions.pool_first) + row) &
            0xfff;
        if (player == 0xfff) {
            market.rows[row].reset();
            continue;
        }
        const auto previous = market.rows[row];
        MarketListing value;
        value.player = p.global_player(player);
        const auto seller = g.memory.u16(at + 8);
        if (seller != 0xffff && seller != 0xfff)
            value.seller = p.global_club(seller & 0x7fff);
        value.human_seller = seller != 0xffff && (seller & 0x8000) != 0;
        value.loan = previous && previous->player == value.player
                         ? previous->loan
                         : false;
        value.fee = g.memory.u32(at + 0x2c);
        value.salary = g.memory.u32(at + 0x30);
        value.asking_percent = g.memory.u8(at + 0x54);
        value.demand_percent = g.memory.u8(at + 0x55);
        value.weeks = g.memory.u8(at + 0x56);
        for (size_t slot = 0; slot < 4; ++slot) {
            auto& bid = value.bids[slot];
            const auto buyer = g.memory.u16(at + uint32_t(slot * 2));
            if (buyer != 0xffff)
                bid.bidder = p.global_club(buyer);
            bid.fee = g.memory.u32(at + 0x1c + uint32_t(slot * 4));
            bid.salary = g.memory.u32(at + 0xc + uint32_t(slot * 4));
            bid.months = g.memory.u8(at + 0x34 + uint32_t(slot));
            for (size_t n = 0; n < 7; ++n)
                bid.bonuses[n] =
                    g.memory.u8(at + 0x38 + uint32_t(slot * 7 + n));
        }
        market.rows[row] = std::move(value);
    }
    market.list_toggle = uint8_t(g.list_toggle);
}
} // namespace
MarketDraft prepare_market_draft(const CareerSnapshot& state,
                                 const RuntimeResources& resources,
                                 ui::World12MarketIdentity identity,
                                 std::optional<uint8_t> listing) {
    state.validate();
    if (!state.world.transfer_market || !state.desktop ||
        state.desktop->active_seat != identity.manager)
        throw FormatError("world12: market has no active canonical manager");
    const auto add = [](std::vector<ClubId>& clubs, ClubId club) {
        if (club.valid() &&
            std::find(clubs.begin(), clubs.end(), club) == clubs.end())
            clubs.push_back(club);
    };
    std::vector<ClubId> extra;
    if (state.transfers)
        for (const auto& owned : state.transfers->loans) {
            const auto loan = std::find_if(
                state.world.loans.begin(), state.world.loans.end(),
                [&](const auto& loan) { return loan.player == owned.player; });
            if (loan == state.world.loans.end())
                throw FormatError(
                    "world12: market loan snapshot has no canonical borrower");
            add(extra, loan->borrower);
        }
    if (listing) {
        if (*listing >= 40 || !state.world.transfer_market->rows[*listing])
            throw FormatError("world12: market listing was recycled");
        const auto& row = *state.world.transfer_market->rows[*listing];
        if (!row.seller.valid())
            throw FormatError(
                "world12: free-agent purchase owner is unavailable");
        add(extra, row.seller);
        for (const auto& bid : row.bids)
            add(extra, bid.bidder);
    }
    MarketDraft draft;
    draft.identity = identity;
    draft.projection =
        project_manager_desktop(state, resources.reference, extra);
    draft.projection.state.texts = resources.texts.get();
    install_transfer_loans(draft.projection, state);
    install_market(draft.projection, state);
    return draft;
}
ui::World12MarketState project_market(const CareerSnapshot& state,
                                      const RuntimeResources& resources,
                                      const MarketDraft& draft) {
    ui::World12MarketState out;
    out.identity = draft.identity;
    out.market = ui::transfer_market_from_game(draft.projection.state,
                                               draft.identity.revision);
    out.market.source = "world12";
    // Typed snapshots do not own this legacy UI word. Use InitializeCareer
    // defaults for the process-only market window, never reference-memory
    // zeroes.
    out.market.source_view_flags = 0xdf0;
    out.market.auxiliary_actions_available = false;
    out.preview = draft.preview;
    out.notice = draft.notice;
    if (draft.offer) {
        out.offer = ui::outgoing_transfer_from_game(
            draft.offer_projection->state, *draft.offer,
            draft.identity.revision, out.market.gradient_text);
        out.offer->player_info_available = false;
        const auto& crest = state.world.crests.at(
            state.world.clubs.at(draft.identity.club).crest);
        out.offer->panel.badge_atlas = crest.path;
        out.offer->panel.badge_sprite = crest.cell;
        const auto seller = state.world.transfer_market
                                ->rows[size_t(draft.offer->request.list_entry)]
                                ->seller;
        const auto& seller_crest =
            state.world.crests.at(state.world.clubs.at(seller).crest);
        out.offer->panel.seller_badge_atlas = seller_crest.path;
        out.offer->panel.seller_badge_sprite = seller_crest.cell;
    }
    (void)resources;
    return out;
}
} // namespace bmh::app::world12

namespace bmh::app::world12 {
namespace {
void require(bool value, const char* message) {
    if (!value)
        throw FormatError(message);
}
PlayerId checked_market_player(const ui::TransferMarketRequest& request,
                               const ui::World12MarketState& page,
                               const MarketDraft& draft) {
    require(request.bid.manager == page.market.manager_slot,
            "world12: market bid has another manager");
    require(request.bid.list_entry >= 0 && request.bid.list_entry < 40,
            "world12: invalid market row");
    const auto found =
        std::find_if(page.market.items.begin(), page.market.items.end(),
                     [&](const auto& item) {
                         return item.list_entry == request.bid.list_entry;
                     });
    require(found != page.market.items.end() &&
                found->player == request.bid.player &&
                found->source_slot == request.bid.source_slot,
            "world12: stale market listing/player");
    return draft.projection.global_player(uint32_t(found->player));
}
void capture_action(CareerSnapshot& candidate, AiMatchProjection& p,
                    std::optional<MarketListing> listing,
                    ClubId expected_buyer) {
    capture_market(candidate, p);
    bool moved = false;
    if (listing && expected_buyer.valid() &&
        expected_buyer != listing->seller) {
        const auto team = p.local_club(expected_buyer),
                   player = p.local_player(listing->player);
        const auto range = squad_range(int(team));
        for (size_t row = 0; row < range.count; ++row)
            moved |=
                (p.state.squad_slots.at(range.first + row) & 0xfff) == player;
    }
    if (moved) {
        p.commit_transfer_state(candidate.world, listing->player,
                                listing->seller, expected_buyer, true);
        capture_career_economy(candidate.world, p);
        capture_transfer_history(candidate, p);
        assign_transfer_loan_slots(candidate);
        capture_created_loan(candidate, p, listing->player, listing->seller,
                             expected_buyer);
    } else
        p.commit_match_state(candidate.world);
}
TransferOfferEditorState change_offer(GameState& g,
                                      const TransferOfferEditorState& owner,
                                      const ui::OutgoingTransferRequest& input,
                                      GameRandom& game, WatcomRandom& random) {
    const auto field = unsigned(input.field);
    require(field <= unsigned(ui::OutgoingTransferField::quote_loan),
            "world12: unknown offer calculator field");
    auto fee = owner.fee, salary = owner.salary;
    auto years = owner.years;
    auto bonuses = owner.bonuses;
    bool loan = owner.quote_loan;
    if (input.accepted) {
        if (field == 0)
            fee = input.value;
        else if (field == 1)
            salary = input.value;
        else if (field == 2)
            years = uint8_t(input.value);
        else if (field < 10)
            bonuses[field - 3] =
                uint8_t(field < 6 ? input.value : input.value / 5);
        else
            loan = !loan;
    }
    require(input.fee == fee && input.monthly_salary == salary &&
                input.duration_years == years && input.bonuses == bonuses &&
                input.quote_loan == loan,
            "world12: offer payload differs from its single calculator event");
    return edit_transfer_offer_editor_source(
        g, owner,
        {TransferOfferEditorField(field), input.value, input.accepted}, game,
        random);
}
} // namespace
ui::World12MarketState Session::open_market(const ui::DeskRequest& request) {
    validate_file_owner(request.manager_slot, request.revision);
    require(request.kind == ui::DeskRequestKind::open_menu &&
                request.menu_kind == 17 && request.value == -1 &&
                market_generation_ < UINT32_MAX && owner_.revision < UINT64_MAX,
            "world12: invalid transfer-market desktop owner");
    auto workspace = project_manager_desktop(state_, resources_.reference);
    const auto desk =
        ui::desk_from_game(workspace.state, resources_.german, owner_.revision);
    require(visible_world12_menu(desk, request.item_id, 17),
            "world12: transfer-market icon is not visible/enabled");
    const auto seat = *state_.desktop->active_seat;
    const auto& actor = *std::find_if(
        state_.world.managers.begin(), state_.world.managers.end(),
        [&](const auto& m) { return m.seat == seat; });
    ui::World12MarketIdentity identity{seat, actor.club, owner_.revision + 1,
                                       owner_.scope,
                                       (market_generation_ + 1) << 32};
    auto draft = std::make_shared<MarketDraft>(
        prepare_market_draft(state_, resources_, identity));
    auto page = project_market(state_, resources_, *draft);
    ++owner_.revision;
    ++market_generation_;
    market_owner_ = std::move(draft);
    preparation_menu_ = 17;
    return page;
}
ui::World12MarketState Session::market_view() const {
    require(preparation_menu_ == 17 && market_owner_ &&
                market_owner_->identity.revision == owner_.revision &&
                market_owner_->identity.scope == owner_.scope &&
                (market_owner_->identity.sequence >> 32) ==
                    market_generation_ &&
                state_.desktop &&
                state_.desktop->active_seat == market_owner_->identity.manager,
            "world12: stale transfer-market window");
    return project_market(state_, resources_, *market_owner_);
}
bool Session::apply_market(const ui::World12MarketRequest& request,
                           const std::filesystem::path& data,
                           const MarketPresenters& presenters,
                           const ClubInformationPresenter& club) {
    require(!market_dispatch_active_,
            "world12: reentrant market action is unavailable");
    market_dispatch_active_ = true;
    struct Release {
        bool& active;
        ~Release() { active = false; }
    } release{market_dispatch_active_};
    const auto current = market_view();
    const auto retained = market_owner_;
    const auto identity = current.identity;
    require(request.identity == identity && identity.revision < UINT64_MAX &&
                identity.sequence < UINT64_MAX &&
                ((identity.sequence + 1) >> 32) == market_generation_,
            "world12: stale transfer-market action");
    const auto before = state_;
    auto candidate = state_;
    auto draft = std::make_shared<MarketDraft>(*retained);
    auto next = identity;
    ++next.revision;
    ++next.sequence;
    GameRandom game(state_.game_seed);
    WatcomRandom random(state_.watcom_seed);
    MinuteReport report;
    std::string notice;
    if (request.close) {
        require(std::holds_alternative<std::monostate>(request.input) &&
                    !draft->offer,
                "world12: market close cannot steal an offer editor");
    } else if (const auto* input =
                   std::get_if<ui::TransferMarketRequest>(&request.input)) {
        require(!draft->offer && input->revision == identity.revision &&
                    input->manager_slot == current.market.manager_slot &&
                    input->runtime_club == current.market.runtime_club,
                "world12: market request has another manager/editor");
        require(
            input->selected_players.size() <= 1,
            "world12: marked transfer batches require their own Source owner");
        draft->preview.reset();
        if (input->kind == ui::TransferMarketRequestKind::list_player) {
            const auto found =
                std::find_if(current.market.squad.begin(),
                             current.market.squad.end(), [&](const auto& row) {
                                 return row.row == input->squad_row &&
                                        row.player == input->expected_player;
                             });
            require(found != current.market.squad.end(),
                    "world12: stale own-player listing");
            if (!input->selected_players.empty()) {
                const auto& selection = input->selected_players.front();
                require(selection.squad_row == found->row &&
                            selection.player == found->player &&
                            selection.seller == current.market.runtime_club &&
                            selection.source_slot ==
                                current.market.roster.squad_start_offset +
                                    found->row,
                        "world12: mismatched marked own player");
            }
            const auto result = list_manager_player(draft->projection.state,
                                                    current.market.manager_slot,
                                                    found->row, random, report);
            require(result.status == TransferActionStatus::accepted ||
                        result.status == TransferActionStatus::pending ||
                        result.status == TransferActionStatus::blocked_by_board,
                    "world12: unsupported own-player listing result");
            require(!report.unsupported, "world12: Source listing failed");
            capture_action(candidate, draft->projection, {}, {});
            notice = result.status == TransferActionStatus::accepted
                         ? "Der Spieler steht jetzt auf der Transferliste."
                     : result.status == TransferActionStatus::pending
                         ? "Der Spieler steht bereits auf der Transferliste."
                         : "Transferliste voll oder Spieler nicht verfügbar.";
        } else if (input->kind ==
                   ui::TransferMarketRequestKind::preview_quote) {
            require(input->selected_players.empty(),
                    "world12: preview cannot carry a marked batch");
            auto preview = draft->projection;
            TransferMarketPreviewRequest source{
                current.market.manager_slot, current.market.runtime_club,
                input->preview_source_slot,  input->expected_player,
                input->preview_seller,       input->preview_is_purchase,
                input->preview_is_loan,      input->preview_snapshot_index};
            if (input->preview_is_purchase) {
                const auto player =
                    checked_market_player(*input, current, *draft);
                require(input->expected_player == input->bid.player &&
                            input->preview_source_slot ==
                                input->bid.source_slot &&
                            input->preview_snapshot_index == -1,
                        "world12: stale purchase preview identity");
                const auto& listing =
                    *candidate.world.transfer_market
                         ->rows[size_t(input->bid.list_entry)];
                require(listing.seller.valid(),
                        "world12: free-agent preview owner is unavailable");
                require(draft->projection.global_club(
                            uint32_t(input->preview_seller)) == listing.seller,
                        "world12: preview seller changed");
                preview = prepare_market_draft(candidate, resources_, identity,
                                               uint8_t(input->bid.list_entry))
                              .projection;
                source.expected_player = int(preview.local_player(player));
                source.seller = int(preview.local_club(listing.seller));
            } else {
                const auto found = std::find_if(
                    current.market.squad.begin(), current.market.squad.end(),
                    [&](const auto& row) {
                        return row.row == input->squad_row &&
                               row.player == input->expected_player;
                    });
                require(input->preview_snapshot_index == -1 &&
                            found != current.market.squad.end() &&
                            input->preview_seller ==
                                current.market.runtime_club &&
                            input->preview_source_slot ==
                                current.market.roster.squad_start_offset +
                                    found->row,
                        "world12: unsupported/stale roster preview");
            }
            const auto quote = quote_transfer_market_preview_source(
                preview.state, source, random);
            require(quote.supported,
                    "world12: Source market preview is unavailable");
            draft->preview =
                ui::TransferMarketPreview{input->manager_slot,
                                          input->runtime_club,
                                          input->preview_source_slot,
                                          input->expected_player,
                                          input->preview_seller,
                                          next.revision,
                                          quote.delta,
                                          quote.projected_balance,
                                          input->preview_is_purchase,
                                          input->preview_is_loan,
                                          input->preview_snapshot_index};
        } else if (input->kind == ui::TransferMarketRequestKind::open_offer ||
                   input->kind ==
                       ui::TransferMarketRequestKind::review_offers) {
            const auto player = checked_market_player(*input, current, *draft);
            const auto row = uint8_t(input->bid.list_entry);
            const auto listing = *candidate.world.transfer_market->rows[row];
            auto action =
                prepare_market_draft(candidate, resources_, identity, row);
            auto& p = action.projection;
            auto& g = p.state;
            const auto local = local_actor(p, identity.manager);
            const auto actor = int(p.local_club(identity.club));
            if (!input->selected_players.empty()) {
                const auto& selection = input->selected_players.front();
                require(selection.squad_row == -1 &&
                            selection.list_entry == row &&
                            selection.player == input->bid.player &&
                            selection.source_slot == input->bid.source_slot &&
                            selection.seller >= 0 &&
                            draft->projection.global_club(
                                uint32_t(selection.seller)) == listing.seller,
                        "world12: marked market selection changed");
            }
            if (input->kind == ui::TransferMarketRequestKind::open_offer) {
                require(listing.seller != identity.club,
                        "world12: own listing requires its offer selector");
                int bid = -1;
                const auto at = layout::EditorList + uint32_t(row) * 0x58;
                for (int slot = 0; slot < 4; ++slot) {
                    const auto buyer = g.memory.u16(at + uint32_t(slot * 2));
                    if (buyer == 0xffff || buyer == actor) {
                        bid = slot;
                        break;
                    }
                }
                auto editor = open_transfer_offer_editor_source(
                    g,
                    {int(local), actor, int(row),
                     runtime_dimensions.pool_first + row,
                     int(p.local_player(player)), g.memory.u16(at + 8), bid,
                     input->offer_is_loan},
                    game, random);
                require(editor.supported,
                        "world12: Source offer editor opening failed");
                capture_action(candidate, p, {}, {});
                draft->offer = std::move(editor);
                draft->offer_projection = std::move(p);
            } else {
                require(listing.seller == identity.club &&
                            bool(presenters.select),
                        "world12: own offers require their visible selector");
                const auto view = project_transfer_offer_list(g, row);
                require(view.valid &&
                            view.player == int(p.local_player(player)) &&
                            view.seller == actor,
                        "world12: Source offer list changed");
                const auto selected = presenters.select(g, view);
                require(selected.has_value() && *selected >= -1 &&
                            *selected < 4,
                        "world12: offer selector interrupted/invalid");
                ClubId buyer;
                if (*selected >= 0) {
                    require(view.offers[size_t(*selected)].present &&
                                bool(presenters.notice),
                            "world12: accepted offer lacks its real "
                            "slot/notice presenter");
                    buyer = p.global_club(
                        uint32_t(view.offers[size_t(*selected)].buyer));
                    bool outcome_presented = false;
                    g.transfer_decision_observer = [&](const auto& event) {
                        outcome_presented = true;
                        presenters.notice(g, event);
                    };
                    const auto result = accept_manager_transfer_offer(
                        g, local, row, int(p.local_player(player)), *selected,
                        random, report,
                        resources_.german.u8(layout::MatchEventsDisabled) == 0
                            ? TransferAllocatorContext::zero
                            : TransferAllocatorContext::blocked);
                    require(!report.unsupported &&
                                result.status !=
                                    TransferActionStatus::unsupported,
                            "world12: Source offer settlement failed");
                    // The existing manual early-accept action has no legacy
                    // Move presentation. Confirm only the actual successful
                    // Core result.
                    if (result.status == TransferActionStatus::accepted &&
                        !outcome_presented) {
                        TransferDecisionEvent event;
                        event.kind = TransferDecisionEventKind::confirmation;
                        event.offers = view;
                        event.selected_bid_index = *selected;
                        event.accepted = true;
                        event.viewer_manager = local;
                        event.result_text_address =
                            layout::PendingOffer_FormatError_ExecutableField2;
                        presenters.notice(g, event);
                    }
                    notice =
                        result.status == TransferActionStatus::accepted
                            ? "Das Angebot wurde angenommen."
                            : "Das Angebot konnte nicht angenommen werden.";
                    g.transfer_decision_observer = {};
                }
                capture_action(candidate, p, listing, buyer);
            }
        } else
            throw FormatError("world12: free-agent/cards/history/poaching "
                              "action has no typed market owner");
    } else if (const auto* input =
                   std::get_if<ui::OutgoingTransferRequest>(&request.input)) {
        require(draft->offer && draft->offer_projection,
                "world12: offer action has no editor owner");
        auto& p = *draft->offer_projection;
        auto& g = p.state;
        const auto& owner = *draft->offer;
        const auto& id = input->identity;
        const auto seller = int(owner.request.expected_seller_word & 0x7fff);
        require(id.revision == identity.revision &&
                    id.manager_slot == owner.request.manager &&
                    id.runtime_club == owner.request.runtime_club &&
                    id.list_entry == owner.request.list_entry &&
                    id.source_slot == owner.request.source_slot &&
                    id.player == owner.request.expected_player &&
                    id.seller == seller && id.bid_slot == owner.bid_slot &&
                    input->quoted_fee == owner.quoted_fee &&
                    input->quoted_monthly_salary == owner.quoted_salary &&
                    input->quoted_years == owner.years,
                "world12: stale offer identity/quote");
        const auto listing =
            *candidate.world.transfer_market->rows[size_t(id.list_entry)];
        auto edited = owner;
        if (input->kind == ui::OutgoingTransferRequestKind::quote_change)
            edited = change_offer(g, owner, *input, game, random);
        else {
            require(input->fee == owner.fee &&
                        input->monthly_salary == owner.salary &&
                        input->duration_years == owner.years &&
                        input->bonuses == owner.bonuses &&
                        input->quote_loan == owner.quote_loan,
                    "world12: unowned offer-field mutation");
            if (input->kind == ui::OutgoingTransferRequestKind::cancel)
                edited = cancel_transfer_offer_editor_source(g, owner);
            else if (input->kind == ui::OutgoingTransferRequestKind::submit) {
                auto hooks = presenters.sale;
                hooks.allocator_gate =
                    resources_.german.u8(layout::MatchEventsDisabled) == 0
                        ? HumanBuyerSaleAllocatorGate::zero
                        : HumanBuyerSaleAllocatorGate::blocked;
                hooks.direct_mode =
                    HumanBuyerSaleDirectMode::ordinary_ai_purchase;
                hooks.before_accounts = {};
                hooks.after_paid_accounts = {};
                const auto result = submit_transfer_offer_editor_source(
                    g, owner, random, hooks, report);
                require(!report.unsupported,
                        "world12: Source offer submission failed");
                edited = result.editor;
            } else if (input->kind ==
                       ui::OutgoingTransferRequestKind::team_info) {
                auto inspected = candidate;
                const auto parent =
                    project_market(candidate, resources_, *draft);
                inspect_club_information(
                    inspected, resources_.reference, resources_.german,
                    *resources_.texts, identity.club, club,
                    ui::OutgoingTransferScreen(data, *parent.offer,
                                               resources_.german)
                        .render());
                const auto team = p.local_club(identity.club);
                const auto& roster =
                    inspected.world.clubs.at(identity.club).roster;
                for (size_t row = 0; row < 24; ++row) {
                    const auto& slot = roster[row];
                    g.squad_slots.at(team * 24 + row) =
                        slot.empty()
                            ? 0xfff
                            : uint16_t(p.local_player(slot.player) |
                                       ((slot.flags & 1) ? 0x8000 : 0) |
                                       ((slot.flags & 2) ? 0x4000 : 0) |
                                       ((slot.flags & 4) ? 0x2000 : 0) |
                                       ((slot.flags & 8) ? 0x1000 : 0));
                }
                edited =
                    resume_transfer_offer_team_info_source(g, owner, random);
            } else
                throw FormatError("world12: player cards require their "
                                  "canonical typed owner");
        }
        require(edited.supported, "world12: Source offer event failed");
        capture_action(candidate, p, listing, identity.club);
        if (edited.closed) {
            draft->offer.reset();
            draft->offer_projection.reset();
        } else
            draft->offer = std::move(edited);
    } else
        throw FormatError("world12: market action is empty");
    require(state_ == before && market_owner_ == retained &&
                owner_.revision == identity.revision &&
                owner_.scope == identity.scope && preparation_menu_ == 17,
            "world12: market presenter changed its live owner");
    candidate.game_seed = game.seed();
    candidate.watcom_seed = random.seed();
    candidate.validate();
    auto refreshed = prepare_market_draft(candidate, resources_, next);
    refreshed.offer = draft->offer;
    refreshed.offer_projection = std::move(draft->offer_projection);
    refreshed.preview = draft->preview;
    draft = std::make_shared<MarketDraft>(std::move(refreshed));
    if (draft->offer) {
        auto fresh =
            prepare_market_draft(candidate, resources_, next,
                                 uint8_t(draft->offer->request.list_entry));
        const auto& previous = *draft->offer_projection;
        for (size_t player = 0; player < previous.state.players.size();
             ++player)
            require(previous.global_player(uint32_t(player)) ==
                        fresh.projection.global_player(uint32_t(player)),
                    "world12: editor player mapping changed during refresh");
        fresh.projection.state = previous.state;
        draft->offer_projection = std::move(fresh.projection);
    }
    draft->notice = notice;
    auto page = project_market(candidate, resources_, *draft);
    if (page.offer)
        ui::OutgoingTransferScreen(data, *page.offer, resources_.german)
            .render();
    else
        ui::TransferMarketScreen(data, page.market, resources_.german).render();
    state_ = std::move(candidate);
    ++owner_.revision;
    if (request.close) {
        market_owner_.reset();
        preparation_menu_.reset();
        return true;
    }
    market_owner_ = std::move(draft);
    return false;
}
} // namespace bmh::app::world12
