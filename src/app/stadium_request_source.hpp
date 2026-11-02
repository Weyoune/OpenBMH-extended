// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "app/stadium_source_types.hpp"
#include "game/state.hpp"
#include "ui/stadium.hpp"
#include "game/messages.hpp"
#include <climits>
namespace bmh::app {
inline bool is_block_expansion_action(ui::StadiumRequestKind kind) {
    return kind==ui::StadiumRequestKind::expansion_select||kind==ui::StadiumRequestKind::expansion_set_total||
        kind==ui::StadiumRequestKind::expansion_toggle_feature||kind==ui::StadiumRequestKind::expansion_cancel||
        kind==ui::StadiumRequestKind::expansion_confirm||kind==ui::StadiumRequestKind::expansion_answer_tip;
}
// Callers own the modal and publication boundary. All construction prices,
// durations, offers, compensation and notices remain the shared Core operations.
inline bool apply_stadium_request_source(GameState& g,const LeImage& rules,GameRandom& game,WatcomRandom& random,
    StadiumSourceRuntime& stadium,uint64_t& next_scope,const ui::StadiumRequest& supplied_request,uint64_t& revision,
    std::string& error,const ConstructionNoticeHook& notice_presenter={},const SeasonTicketCompensationNoticeHook& compensation_presenter={}) {
    constexpr uint8_t kBlockSeat=1,kBlockRoof=2,kBlockFeatures=kBlockSeat|kBlockRoof;
    struct Checkpoint{GameState state;GameRandom game;WatcomRandom random;StadiumSourceRuntime stadium;uint64_t scope;};
    const auto checkpoint=[&]{return Checkpoint{g,game,random,stadium,next_scope};};
    const auto restore=[&](Checkpoint before){g=std::move(before.state);game=before.game;random=before.random;stadium=std::move(before.stadium);next_scope=before.scope;};
    const int active = int(g.memory.u8(::bmh::layout::ActiveManager));
    auto request = supplied_request;
    // Existing CLI periphery callers predate the UI scope envelope. Restrict
    // this compatibility adapter to their four original operations and an
    // entirely absent envelope; investments and close remain strictly owned.
    const bool legacy_periphery = !request.stable_club_identity && request.scope == 0 && request.sequence == 0 &&
        (request.kind == ui::StadiumRequestKind::open || request.kind == ui::StadiumRequestKind::quote_periphery ||
         request.kind == ui::StadiumRequestKind::decline_periphery || request.kind == ui::StadiumRequestKind::build_periphery);
    if (legacy_periphery && active >= 0 && active <= 3 && size_t(active) < g.manager_teams.size() &&
        request.runtime_club >= 0 && size_t(request.runtime_club) < g.club_ids.size()) {
        request.stable_club_identity = int(g.club_ids[size_t(request.runtime_club)]);
        if (request.kind != ui::StadiumRequestKind::open) {
            request.scope = stadium.scope; request.sequence = stadium.sequence;
        }
    }
    const bool block_action = is_block_expansion_action(request.kind);
    if (!block_action && (request.block_slot != -1 || request.block_total_places != 0 ||
                          request.block_features != 0 || request.expected_block_quote)) {
        error = "unexpected_block_expansion_fields"; return false;
    }
    if (request.manager_slot != active || active < 0 || active > 3 ||
        size_t(active) >= g.manager_teams.size() || request.runtime_club != g.manager_teams[size_t(active)] ||
        request.runtime_club < 0 || request.runtime_club >= kNoTeam ||
        size_t(request.runtime_club) >= g.club_ids.size() ||
        request.stable_club_identity != std::optional<int>(int(g.club_ids[size_t(request.runtime_club)])) ||
        request.revision != revision || revision == UINT64_MAX || !g.memory.has(::bmh::layout::GameMode)) {
        error = "stale_manager_or_revision"; return false;
    }
    const size_t record = size_t(active) * 0xb0c;
    if (record + 0xb0c > g.manager_records.size() ||
        (int(g.manager_records[record + 0x290]) | int(g.manager_records[record + 0x291]) << 8) != request.runtime_club) {
        error = "invalid_stadium_owner"; return false;
    }
    const int mode = g.memory.u8(::bmh::layout::GameMode);
    if (mode < 1 || mode > 3) { error = "invalid_stadium_mode"; return false; }
    if (request.kind == ui::StadiumRequestKind::open) {
        const bool legacy_reentry = legacy_periphery && !stadium.data_open && !stadium.investment && !stadium.tip &&
            !stadium.block_expansion;
        if ((stadium.opened && !legacy_reentry) || request.scope != 0 || request.sequence != 0 || request.object != -1 ||
            request.option != -1 || request.price != 0 || request.accept_tip || next_scope == UINT64_MAX) {
            error = "invalid_stadium_open"; return false;
        }
    } else if (!stadium.opened || stadium.manager != active || stadium.club != request.runtime_club ||
               stadium.stable_club != *request.stable_club_identity || stadium.mode != mode ||
               stadium.revision != revision || request.scope != stadium.scope ||
               request.sequence != stadium.sequence || stadium.sequence == UINT64_MAX) {
        error = "stale_stadium_scope"; return false;
    }
    auto before = checkpoint();
    const uint64_t before_revision = revision;
    try {
        const auto apply = [&]() -> bool {
            const auto invalid = [&error](const char* reason) { error = reason; return false; };
            const auto plain = [&request](int object = -1) {
                return request.object == object && request.option == -1 && request.price == 0 && !request.accept_tip;
            };
            const auto clear_investment = [&] {
                stadium.investment.reset(); stadium.selection_status.reset(); stadium.option.reset(); stadium.tip.reset();
                stadium.option_index = -1; stadium.text_variant = -1;
            };
            StadiumInvestmentConstants constants;
            for (unsigned i = 0; i < 8; ++i) {
                constants.price[i] = rules.u32(::bmh::layout::CareerSession_DispatchStadium_RuntimeField + i * 4);
                constants.weeks[i] = rules.u8(::bmh::layout::CareerSession_DispatchStadium_RuntimeField2 + (14 + i) * 5);
            }
            const auto options = [&](StadiumInvestment investment) {
                return stadium_investment_options_source(g, active, investment, stadium.price_percent, constants);
            };
            const auto selected_valid = [&]() {
                if (!stadium.investment || !stadium.option || stadium.option_index < 0) return false;
                const auto current = options(*stadium.investment);
                return size_t(stadium.option_index) < current.size() &&
                    current[size_t(stadium.option_index)] == *stadium.option;
            };
            if (request.kind == ui::StadiumRequestKind::open) {
                StadiumSourceRuntime opened;
                opened.opened = true; opened.manager = active; opened.club = request.runtime_club;
                opened.stable_club = *request.stable_club_identity; opened.mode = mode;
                opened.scope = next_scope++;
                opened.price_percent = construction_price_percent(g, active, game);
                if (opened.price_percent < 1 || opened.price_percent > 427) return invalid("invalid_stadium_discount");
                stadium = std::move(opened);
                return true;
            }
            if (request.kind == ui::StadiumRequestKind::close) {
                if (stadium.data_open || !plain() || stadium.investment || stadium.periphery_quote || stadium.tip ||
                    stadium.block_expansion)
                    return invalid("invalid_stadium_close");
                stadium.opened = false; stadium.periphery_quote.reset(); clear_investment();
                stadium.feedback.clear(); return true;
            }
            const bool data_action = request.kind == ui::StadiumRequestKind::open_data ||
                request.kind == ui::StadiumRequestKind::toggle_data_curve ||
                request.kind == ui::StadiumRequestKind::toggle_data_scale ||
                request.kind == ui::StadiumRequestKind::set_data_ticket_price ||
                request.kind == ui::StadiumRequestKind::close_data;
            if (data_action) {
                if (stadium.investment || stadium.periphery_quote || stadium.tip || stadium.block_expansion ||
                    request.option != -1 || request.price != 0 || request.accept_tip)
                    return invalid("invalid_stadium_data_context");
                if (request.kind == ui::StadiumRequestKind::open_data) {
                    if (stadium.data_open || request.object != -1) return invalid("invalid_stadium_data_open");
                    const uint32_t flags_at = ::bmh::layout::Flags + uint32_t(active) * 3 + 1;
                    if (!g.memory.has(flags_at)) return invalid("missing_stadium_data_flags");
                    stadium.data_flags = g.memory.u8(flags_at);
                    // Build the pure projection before accepting modal ownership.
                    (void)ui::stadium_data_from_game(g, rules, active, stadium.data_flags);
                    stadium.data_open = true;
                } else {
                    if (!stadium.data_open) return invalid("stadium_data_not_open");
                    if (request.kind == ui::StadiumRequestKind::toggle_data_curve) {
                        if (request.object < 0 || request.object > 5) return invalid("invalid_stadium_data_curve");
                        stadium.data_flags ^= uint8_t(1u << request.object);
                    } else if (request.kind == ui::StadiumRequestKind::toggle_data_scale) {
                        if (request.object < 0 || request.object > 1) return invalid("invalid_stadium_data_scale");
                        stadium.data_flags ^= 0xc0; // Either ABS or REL button toggles both Source bits.
                    } else if (request.kind == ui::StadiumRequestKind::set_data_ticket_price) {
                        if (request.object < 10 || request.object > 25) return invalid("invalid_stadium_data_ticket");
                        g.manager_records[record + 0x26e] = uint8_t(request.object);
                        (void)ui::stadium_data_from_game(g, rules, active, stadium.data_flags);
                    } else {
                        if (request.object != -1) return invalid("invalid_stadium_data_close");
                        g.memory.set8(::bmh::layout::Flags + uint32_t(active) * 3 + 1, stadium.data_flags);
                        stadium.data_open = false;
                    }
                }
                stadium.feedback.clear();
                return true;
            }
            if (stadium.data_open) return invalid("stadium_data_active");
            if (mode == 1 && !legacy_periphery &&
                (request.kind == ui::StadiumRequestKind::quote_periphery ||
                 request.kind == ui::StadiumRequestKind::build_periphery ||
                 request.kind == ui::StadiumRequestKind::decline_periphery))
                return invalid("stadium_periphery_disabled");
            if (request.kind == ui::StadiumRequestKind::quote_periphery) {
                if (stadium.investment || stadium.periphery_quote || stadium.block_expansion || request.object < 0 ||
                    request.object >= 14 || !plain(request.object)) return invalid("invalid_periphery_offer");
                const size_t object = size_t(request.object);
                const uint8_t level = g.manager_records[record + object];
                if (g.manager_records[record + 0x1e + object] != 0 || (object == 0 ? level >= 0xab : level != 0))
                    return invalid("periphery_unavailable");
                const int32_t price = periphery_offer(g, request.object, game);
                if (price <= 0) return invalid("invalid_periphery_price");
                stadium.periphery_quote = std::make_pair(request.object, price);
                stadium.feedback.clear(); return true;
            }
            if (request.kind == ui::StadiumRequestKind::decline_periphery ||
                request.kind == ui::StadiumRequestKind::build_periphery) {
                if (stadium.investment || stadium.block_expansion || !stadium.periphery_quote ||
                    request.option != -1 || request.accept_tip ||
                    request.object != stadium.periphery_quote->first || request.price != stadium.periphery_quote->second)
                    return invalid("stale_periphery_offer");
                if (request.kind == ui::StadiumRequestKind::build_periphery) {
                    const size_t object = size_t(request.object);
                    const uint8_t level = g.manager_records[record + object];
                    if (g.manager_records[record + 0x1e + object] != 0 || (object == 0 ? level >= 0xab : level != 0))
                        return invalid("periphery_unavailable");
                    // ConstructionPricePercent consumes this genuine JA even when PayConstruction's
                    // payment fails. The Source presenter runs before the quote
                    // is cleared; payment/sponsor effects are not rolled back.
                    const bool built = build_periphery(g, active, request.object,
                        uint32_t(request.price), random, notice_presenter);
                    stadium.feedback = built || notice_presenter ? std::string{} : message_text(g, ::bmh::layout::Advertising_FormatError_TextPointer18);
                }
                stadium.periphery_quote.reset();
                if (request.kind == ui::StadiumRequestKind::decline_periphery) stadium.feedback.clear();
                return true;
            }

            // A–H uses its own quote/commit domain and remains available in
            // Source mode 1, where facilities are disabled. All request fields
            // outside the typed block envelope must stay neutral.
            if (block_action) {
                if (request.object != -1 || request.option != -1 || request.price != 0 ||
                    (request.kind != ui::StadiumRequestKind::expansion_answer_tip && request.accept_tip))
                    return invalid("invalid_block_expansion_envelope");
                const auto block_constants=stadium_block_expansion_constants(rules);
                const auto quote_block = [&](StadiumBlockSlot slot, uint32_t total, uint8_t features) {
                    return quote_stadium_block_expansion(g, active, slot, total, features,
                                                         stadium.price_percent, block_constants);
                };
                if (request.kind == ui::StadiumRequestKind::expansion_select) {
                    if (stadium.investment || stadium.periphery_quote || stadium.block_expansion ||
                        request.block_slot < 0 || request.block_slot >= 8 || request.block_total_places != 0 ||
                        request.block_features != 0 || request.expected_block_quote)
                        return invalid("invalid_block_expansion_selection");
                    const auto slot = static_cast<StadiumBlockSlot>(request.block_slot);
                    const uint8_t raw = g.manager_records.at(record + 0x44 + size_t(request.block_slot));
                    const uint32_t current = uint32_t(g.manager_records.at(record + 0x4c + size_t(request.block_slot))) * 1000;
                    auto quote = quote_block(slot, current, uint8_t(raw & kBlockFeatures));
                    if (!quote) return invalid("block_expansion_unavailable");
                    StadiumSourceRuntime::BlockExpansion draft;
                    draft.quote = *quote;
                    draft.total_target_places = current;
                    draft.desired_features = uint8_t(raw & kBlockFeatures);
                    game.set_seed(int32_t(g.memory.u32(::bmh::layout::CareerSession_Invalid_RuntimeField2)));
                    draft.header_text_variant = game.range(3, 10); // Front, once on editor entry.
                    stadium.block_expansion = std::move(draft);
                    stadium.feedback.clear();
                    return true;
                }

                if (!stadium.block_expansion || stadium.investment || stadium.periphery_quote ||
                    request.block_slot < 0 || request.block_slot >= 8 ||
                    request.block_slot != int(stadium.block_expansion->quote.slot) ||
                    !request.expected_block_quote || *request.expected_block_quote != stadium.block_expansion->quote)
                    return invalid("stale_block_expansion_quote");
                auto& draft = *stadium.block_expansion;
                if (draft.tip) {
                    const auto& tip = *draft.tip;
                    const bool valid_tip = tip.decision == StandBlockOfferDecision::Pending &&
                        tip.base_price == draft.quote.base_price && tip.base_days == draft.quote.base_weeks &&
                        tip.price == tip.base_price && tip.percent >= 6 && tip.percent <= 14 &&
                        tip.offered_price == uint32_t(uint64_t(tip.base_price) * uint32_t(100 + tip.percent) / 100);
                    if (!valid_tip) return invalid("stale_block_expansion_tip");
                }
                auto fresh = quote_block(draft.quote.slot, draft.total_target_places, draft.desired_features);
                if (!fresh || *fresh != draft.quote)
                    return invalid("block_expansion_source_changed");

                const auto matches_draft = [&] {
                    return request.block_total_places == draft.total_target_places &&
                        request.block_features == draft.desired_features;
                };
                switch (request.kind) {
                case ui::StadiumRequestKind::expansion_set_total: {
                    if (draft.tip || request.block_features != draft.desired_features)
                        return invalid("stale_block_expansion_draft");
                    auto next_quote = quote_block(draft.quote.slot, request.block_total_places, draft.desired_features);
                    if (!next_quote) return invalid("invalid_block_expansion_total");
                    draft.total_target_places = request.block_total_places;
                    draft.quote = *next_quote;
                    stadium.feedback.clear();
                    return true;
                }
                case ui::StadiumRequestKind::expansion_toggle_feature: {
                    const uint8_t changed = uint8_t(request.block_features ^ draft.desired_features);
                    if (draft.tip || request.block_total_places != draft.total_target_places ||
                        (request.block_features & uint8_t(~kBlockFeatures)) != 0 ||
                        (changed != kBlockSeat && changed != kBlockRoof) ||
                        (request.block_features & uint8_t(draft.quote.current_raw_flags & kBlockFeatures)) !=
                            uint8_t(draft.quote.current_raw_flags & kBlockFeatures))
                        return invalid("invalid_block_expansion_feature");
                    auto next_quote = quote_block(draft.quote.slot, draft.total_target_places, request.block_features);
                    if (!next_quote) return invalid("invalid_block_expansion_feature");
                    draft.desired_features = request.block_features;
                    draft.quote = *next_quote;
                    stadium.feedback.clear();
                    return true;
                }
                case ui::StadiumRequestKind::expansion_cancel:
                    if (draft.tip || !matches_draft()) return invalid("stale_block_expansion_cancel");
                    stadium.block_expansion.reset();
                    stadium.feedback.clear();
                    return true;
                case ui::StadiumRequestKind::expansion_confirm: {
                    if (draft.tip || !matches_draft()) return invalid("stale_block_expansion_confirm");
                    if (draft.quote.base_price == 0) {
                        stadium.block_expansion.reset();
                        stadium.feedback.clear();
                        return true;
                    }
                    auto tip = prepare_stand_block_offer(draft.quote.base_price, draft.quote.base_weeks, random);
                    if (tip.offered_price > uint32_t(INT32_MAX))
                        return invalid("block_expansion_tip_price_overflow");
                    draft.tip = tip;
                    stadium.feedback.clear();
                    return true;
                }
                case ui::StadiumRequestKind::expansion_answer_tip: {
                    if (!draft.tip || !matches_draft() || request.accept_tip &&
                        uint64_t(draft.tip->base_price) * uint32_t(100 + draft.tip->percent) >
                            uint64_t(INT32_MAX))
                        return invalid("invalid_block_expansion_tip_answer");
                    StandBlockOffer resolved = *draft.tip;
                    resolve_stand_block_offer(resolved, request.accept_tip, random);
                    const bool built = build_stadium_block_expansion(g, active, draft.quote.slot,
                        block_constants, draft.quote, resolved, random, notice_presenter, compensation_presenter);
                    stadium.block_expansion.reset();
                    stadium.feedback = built || notice_presenter ? std::string{} : message_text(g, ::bmh::layout::Advertising_FormatError_TextPointer18);
                    return true;
                }
                default:
                    return invalid("unsupported_block_expansion_request");
                }
            }
            if (mode == 1) return invalid("stadium_investments_disabled");
            if (request.kind == ui::StadiumRequestKind::select_investment) {
                if (stadium.investment || stadium.periphery_quote || stadium.block_expansion || request.object < 0 ||
                    request.object >= 8 || !plain(request.object)) return invalid("invalid_investment_selection");
                const auto investment = static_cast<StadiumInvestment>(request.object);
                const auto current = options(investment);
                if (current.empty()) return invalid("investment_unavailable");
                game.set_seed(int32_t(g.memory.u32(::bmh::layout::CareerSession_Invalid_RuntimeField2)));
                stadium.text_variant = game.range(3, 10); // Front: exactly once at dialog entry.
                stadium.investment = investment; stadium.selection_status = current.front().current_status;
                stadium.option_index = -1; stadium.option.reset(); // Source iVar5=-1 until an explicit tier click.
                stadium.feedback.clear(); return true;
            }
            if (request.kind == ui::StadiumRequestKind::cancel_investment) {
                if (!stadium.investment || stadium.tip || !plain(int(*stadium.investment)))
                    return invalid("stale_investment_selection");
                clear_investment(); stadium.feedback.clear(); return true; // Initial Abbruch precedes W(6,14).
            }
            if (request.kind == ui::StadiumRequestKind::select_investment_option) {
                if (!stadium.investment || stadium.tip || request.object != int(*stadium.investment) ||
                    request.option < 0 || request.accept_tip || !stadium.selection_status ||
                    g.manager_records[record + 0x3c + size_t(*stadium.investment)] != *stadium.selection_status)
                    return invalid("stale_investment_selection");
                const auto current = options(*stadium.investment);
                if (size_t(request.option) >= current.size() || request.price != int32_t(current[size_t(request.option)].base_price))
                    return invalid("stale_investment_price");
                stadium.option_index = request.option; stadium.option = current[size_t(request.option)];
                return true; // Pure tier choice: neither Source generator advances.
            }
            if (request.kind == ui::StadiumRequestKind::confirm_investment) {
                if (stadium.tip || !selected_valid() || request.object != int(*stadium.investment) ||
                    request.option != stadium.option_index || request.accept_tip ||
                    request.price != int32_t(stadium.option->base_price)) return invalid("stale_investment_selection");
                stadium.tip = prepare_stand_block_offer(stadium.option->base_price, stadium.option->base_weeks, random);
                if (stadium.tip->offered_price > uint32_t(INT32_MAX)) return invalid("invalid_investment_tip_price");
                return true;
            }
            if (request.kind == ui::StadiumRequestKind::answer_construction_tip) {
                if (!stadium.tip || !selected_valid() || request.object != int(*stadium.investment) ||
                    request.option != stadium.option_index || request.price != int32_t(request.accept_tip ?
                        stadium.tip->offered_price : stadium.tip->base_price)) return invalid("stale_construction_tip");
                auto resolved = *stadium.tip;
                resolve_stand_block_offer(resolved, request.accept_tip, random);
                const bool built = build_stadium_investment_source(g, active, *stadium.investment,
                    stadium.price_percent, constants, *stadium.option, resolved, random, notice_presenter);
                // A genuine PayConstruction/BookMoney2 refusal consumes the answer and
                // retains the sponsor share clearing and Source W draws.
                // Interactive callers have already shown Source's timed funds
                // notice; headless legacy callers retain their text feedback.
                clear_investment();
                stadium.feedback = built || notice_presenter ? std::string{} : message_text(g, ::bmh::layout::Advertising_FormatError_TextPointer18);
                return true;
            }
            return invalid("unsupported_stadium_request");
        };
        if (!apply()) { restore(std::move(before)); return false; }
        ++revision; ++stadium.sequence; stadium.revision = revision;
        error.clear(); return true;
    } catch (...) {
        restore(std::move(before)); revision = before_revision; throw;
    }

}
}
