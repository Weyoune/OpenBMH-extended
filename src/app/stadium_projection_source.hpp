// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "app/stadium_source_types.hpp"
#include "game/state.hpp"
#include "ui/stadium.hpp"
#include "ui/pending_offer.hpp"
#include "game/match.hpp"
#include "game/messages.hpp"
#include <bit>
namespace bmh::app {
template<class RuntimeByte>
ui::StadiumState stadium_projection_source(const GameState& state,const LeImage& rules,const TextDb& texts,
    uint64_t revision,const ui::DeskState& desk,const StadiumSourceRuntime& runtime,RuntimeByte runtime_byte) {
    auto stadium=ui::stadium_from_game(state,rules,revision);
    const int team = stadium.runtime_club;
    const int stable = int(state.club_ids.at(size_t(team)));
    const bool owned = runtime.manager == stadium.manager_slot &&
        runtime.club == team && runtime.stable_club == stable && runtime.revision == revision;
    stadium = ui::stadium_from_game(state, rules, revision, owned ? runtime.price_percent : 100);
    stadium.source = desk.source;
    stadium.club_name = desk.club_name;
    stadium.stable_club_identity = stable;
    stadium.plain_background = runtime_byte(::bmh::layout::PlainBackground) != 0;
    stadium.decorated_stadium_header = !stadium.plain_background;
    stadium.compact_dialog_frame = runtime_byte(::bmh::layout::CompactDialogFrame) != 0;
    stadium.solid_panel_background = runtime_byte(::bmh::layout::SolidPanelBackground) != 0;
    stadium.gradient_text = runtime_byte(::bmh::layout::TextGradientDisabled) == 0 &&
        runtime_byte(::bmh::layout::TextGradientMode) == 2;
    stadium.source_level = state.level;
    stadium.season_tickets = uint16_t(state.manager_records.at(size_t(stadium.manager_slot) * 0xb0c + 0x2ac)) |
        uint16_t(state.manager_records.at(size_t(stadium.manager_slot) * 0xb0c + 0x2ad)) << 8;
    const int source_team = int16_t(uint16_t(state.manager_records.at(size_t(stadium.manager_slot) * 0xb0c + 0x290)) |
        uint16_t(state.manager_records.at(size_t(stadium.manager_slot) * 0xb0c + 0x291)) << 8);
    const int source_league = source_team > (kNoTeam - 1) ? 0 : match_team_league(state,source_team);
    if (source_league < 0 || size_t(source_league) >= state.league_rounds.size())
        throw FormatError("stadium source league is outside the live league table");
    stadium.live_league_round = state.projected_match_clubs.empty()?state.league_rounds.at(size_t(source_league)):state.projected_match_clubs.at(size_t(source_team)).round_cursor;
    stadium.ticket_refund_rate = std::bit_cast<int32_t>(rules.u32(::bmh::layout::CareerSession_FormatError_RuntimeField5 + uint32_t(source_league) * 4));
    stadium.investments_enabled = state.memory.u8(::bmh::layout::GameMode) != 1;
    for (size_t i = 0; i < stadium.investments.size(); ++i) {
        stadium.investments[i].original_label = texts[734 + i];
        if (!stadium.investments_enabled) stadium.investments[i].available = false;
    }
    if (owned) {
        stadium.opened = runtime.opened;
        stadium.scope = runtime.scope; stadium.sequence = runtime.sequence;
        if (runtime.data_open)
            stadium.data = ui::stadium_data_from_game(state, rules, stadium.manager_slot, runtime.data_flags);
        stadium.selected_investment = runtime.investment;
        if (runtime.option_index >= 0) stadium.selected_option = runtime.option_index;
        stadium.investment_text_variant = runtime.text_variant;
        stadium.action_feedback = runtime.feedback;
        if (runtime.tip) {
            PendingOfferQuestion question;
            question.manager = runtime.manager; question.source_flags = 0x204;
            question.original_message = message_text(state, ::bmh::layout::CareerSession_FormatError_TextPointer8) +
                format_number(state, runtime.tip->percent, 1) + message_text(state, ::bmh::layout::CareerSession_FormatError_TextPointer9);
            stadium.construction_tip_dialog = ui::pending_offer_question_dialog(state, question);
            stadium.construction_tip_price = runtime.tip->offered_price;
        }
        if (runtime.periphery_quote) {
            stadium.quoted_object = runtime.periphery_quote->first;
            stadium.quoted_price = runtime.periphery_quote->second;
            PendingOfferQuestion question;
            question.manager = runtime.manager;
            question.source_flags = 1; // ConstructionPricePercent -> PresentManagerDialog: ordinary JA/NEIN question.
            question.original_message = message_text(state, ::bmh::layout::CareerSession_FormatError_TextPointer6) +
                format_number(state, stadium.quoted_price, 0x106) + message_text(state, ::bmh::layout::CareerSession_FormatError_TextPointer7);
            stadium.periphery_quote_dialog = ui::pending_offer_question_dialog(state, question);
            stadium.periphery_quote_dialog->compact_dialog_frame = stadium.compact_dialog_frame;
        }
        if (runtime.block_expansion) {
            const auto& draft = *runtime.block_expansion;
            ui::StadiumBlockExpansionEditor editor;
            editor.quote = draft.quote;
            editor.total_target_places = draft.total_target_places;
            editor.desired_features = draft.desired_features;
            editor.header_text_variant = draft.header_text_variant;
            editor.offer = draft.tip;
            const uint32_t current = uint32_t(draft.quote.current_capacity_thousands) * 1000;
            const uint32_t added = draft.total_target_places - current;
            editor.current_places_text = format_number(state, int32_t(current), 5);
            editor.new_places_text = format_number(state, int32_t(added), 5);
            editor.target_places_text = format_number(state, int32_t(draft.total_target_places), 5);
            const auto cost_text = [&](uint32_t amount) {
                return format_number(state, int32_t(amount), 0x181) + rules.cstring(::bmh::layout::CareerSession_FormatNumber_ExecutableField);
            };
            editor.per_thousand_cost_text = format_number(state, int32_t(draft.quote.price_per_1000), 0x181);
            editor.new_places_cost_text = cost_text(draft.quote.new_places_cost);
            editor.seats_cost_text = cost_text(draft.quote.seats_cost);
            editor.roof_cost_text = cost_text(draft.quote.roof_cost);
            editor.total_cost_text = cost_text(draft.quote.base_price);
            editor.discount_text = rules.cstring(::bmh::layout::CareerSession_FormatNumber_TextPointer) +
                format_number(state, 100 - runtime.price_percent, 1) + rules.cstring(::bmh::layout::CareerSession_FormatNumber_ExecutableField2);
            stadium.block_expansion = std::move(editor);
        }
        if (runtime.block_expansion && runtime.block_expansion->tip) {
            PendingOfferQuestion question;
            question.manager = runtime.manager;
            question.source_flags = 0x204;
            question.original_message = message_text(state, ::bmh::layout::CareerSession_FormatError_TextPointer8) +
                format_number(state, runtime.block_expansion->tip->percent, 1) +
                message_text(state, ::bmh::layout::CareerSession_FormatError_TextPointer9);
            stadium.construction_tip_dialog = ui::pending_offer_question_dialog(state, question);
            stadium.construction_tip_dialog->compact_dialog_frame = stadium.compact_dialog_frame;
            stadium.construction_tip_price = runtime.block_expansion->tip->offered_price;
        }
    }
    return stadium;
}
}
