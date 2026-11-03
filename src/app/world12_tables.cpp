// SPDX-License-Identifier: GPL-3.0-only
#include "app/world12_tables.hpp"
#include "app/world12_session.hpp"
#include "game/world12/rescheduling.hpp"
#include "app/world12_visible_menu.hpp"
#include "game/messages.hpp"
#include "game/world12/desktop_projection.hpp"
#include "game/world12/fixtures.hpp"
#include "ui/league_table.hpp"
#include <algorithm>
namespace bmh::app::world12 {
using namespace bmh::world12;
namespace {
Manager& table_actor(CareerSnapshot& state, uint8_t seat) {
    auto m =
        std::find_if(state.world.managers.begin(), state.world.managers.end(),
                     [&](const auto& m) { return m.seat == seat; });
    if (m == state.world.managers.end() || !m->career)
        throw FormatError("world12: missing table manager career");
    return *m;
}
uint8_t points_mode(const RuntimeResources& resources, const League& league) {
    const auto& rules = league.country == Country::england ? resources.english
                                                           : resources.german;
    const uint32_t row = league.playable                    ? league.local_index
                         : league.country == Country::spain ? 5
                         : league.country == Country::england ? 6
                         : league.country == Country::france  ? 7
                                                              : 8;
    return rules.u8(
               layout::CareerSession_BeginFriendlyOpponentTable_RuntimeField +
               row) &
           1;
}
void change_table_league(ui::World12TableSelection& selection, LeagueId league,
                         const CareerSnapshot& state,
                         const RuntimeResources& resources) {
    if (selection.league == league)
        return;
    const int retained_round = selection.pairings.round;
    selection.league = league;
    selection.age.reset();
    selection.age_basis = ui::StrengthsAgeBasis::entry_or_league_change_bit10;
    selection.pairings = LeaguePairingsOwner{};
    selection.pairings.round =
        retained_round >= 0 &&
                retained_round <
                    state.world.catalogue.league(league).rounds() &&
                retained_round <= state.league_rounds.at(league.value)
            ? retained_round
            : std::min<int>(state.league_rounds.at(league.value),
                            state.world.catalogue.league(league).rounds() - 1);
    selection.pairings.title_round = selection.pairings.round;
    selection.pairings_preferences =
        uint8_t((selection.pairings_preferences & 0xe0) | 1);
    selection.first_round = 0;
    selection.end_round = visible_league_round(state,league);
    ui::normalize_world12_table_range(selection, state);
    selection.normal_mode =
        points_mode(resources, state.world.catalogue.league(league));
    selection.word &= 0xf7f;
}
void retain_draw(ui::World12TableSelection& selection,
                 const ui::World12TableState& page) {
    if (page.strengths) {
        selection.strengths_word =
            page.strengths->projection.words.post_draw_settings_word;
        selection.age = page.strengths->projection.age_scratch;
    }
}
bool visible_club(const ui::World12TableState& page, ClubId club) {
    if (page.pairings) {
        for (const auto& row : page.pairings->source.rows)
            for (const auto* team : {&row.home, &row.away})
                if (page.local_clubs.at(size_t(team->runtime_team)) == club)
                    return true;
        return false;
    }
    return std::find(page.clubs.begin(), page.clubs.end(), club) !=
           page.clubs.end();
}

AiMatchProjection family_workspace(const CareerSnapshot& state,
                                   const RuntimeResources& resources,
                                   uint8_t seat,
                                   const std::vector<ClubId>& extra) {
    std::vector<ClubId> clubs;
    for (const auto& manager : state.world.managers)
        clubs.push_back(manager.club);
    for (auto club : extra)
        if (std::find(clubs.begin(), clubs.end(), club) == clubs.end())
            clubs.push_back(club);
    if (!state.world.settings)
        throw FormatError("world12: family has no career settings");
    const auto settings = *state.world.settings;
    auto projection = AiMatchProjection::create_for_desktop(
        state, clubs, resources.reference,
        {settings.mode, settings.difficulty, settings.foreign_rule, true});
    size_t local = 0;
    while (local < projection.human_managers() &&
           projection.global_manager(uint8_t(local)) != seat)
        ++local;
    if (local == projection.human_managers())
        throw FormatError("world12: family has no canonical manager");
    projection.state.memory.set8(layout::ActiveManager, uint8_t(local));
    return projection;
}
} // namespace
ui::World12TableState
project_league_family(const CareerSnapshot& state,
                      const RuntimeResources& resources,
                      const ui::World12TableIdentity& identity,
                      const ui::World12TableSelection& selection) {
    if (!resources.texts)
        throw FormatError("world12: table texts unavailable");
    auto out = ui::world12_table_state(state, identity, selection,
                                       resources.german, *resources.texts);
    const auto projected =
        family_workspace(state, resources, identity.manager, {});
    const auto& local = projected.state;
    out.table.decorated_background =
        local.memory.u8(layout::PlainBackground) == 0;
    out.table.original_table_panel =
        local.memory.u8(layout::SolidPanelBackground) == 0;
    out.table.source_show_previous_rank =
        local.memory.has(layout::LeagueTable_OriginalToUtf8_RuntimeField) &&
        local.memory.u8(layout::LeagueTable_OriginalToUtf8_RuntimeField) != 0 &&
        out.table.rounds_played > 1;
    const auto label = [&](uint32_t at) {
        return ui::original_to_utf8(
            (*resources.texts)[(at - layout::TextPointerTable) / 4]);
    };
    if (out.table.rounds_played >=
        state.world.catalogue.league(selection.league).rounds())
        out.subtitle = label(layout::CareerSession_FormatError_TextPointer3);
    else {
        auto suffix = label(layout::CareerSession_FormatError_TextPointer4);
        while (!suffix.empty() && suffix.front() == ' ')
            suffix.erase(suffix.begin());
        auto prefix = label(layout::CareerSession_FormatError_TextPointer2);
        while (!prefix.empty() && prefix.back() == ' ')
            prefix.pop_back();
        out.subtitle = prefix + " " +
                       std::to_string(out.table.rounds_played + 1) + ". " +
                       suffix;
    }

    out.child = selection.child;
    if (selection.child) {
        const auto& league = state.world.catalogue.league(selection.league);
        const auto& members =
            state.world.membership[selection.league.value].clubs;
        auto workspace =
            family_workspace(state, resources, identity.manager, members);
        auto& game = workspace.state;
        uint8_t local_manager = 0;
        while (workspace.global_manager(local_manager) != identity.manager)
            ++local_manager;
        const int actor = int(workspace.local_club(identity.club));
        for (const auto& manager : state.world.managers)
            out.local_clubs.push_back(manager.club);
        for (auto club : members)
            if (std::find(out.local_clubs.begin(), out.local_clubs.end(),
                          club) == out.local_clubs.end())
                out.local_clubs.push_back(club);
        for (size_t i = 0; i < out.local_clubs.size(); ++i)
            if (workspace.local_club(out.local_clubs[i]) != i)
                throw FormatError("world12: family local club mapping differs");
        if (selection.child == 2) {
            game.memory.add_block(layout::ManagerSettings + local_manager * 2,
                                  2);
            game.memory.set16(layout::ManagerSettings + local_manager * 2,
                              selection.strengths_word);
            ui::StrengthsProjectionContext context;
            context.league_name = league.name;
            for (auto club : members)
                context.teams.push_back(int(workspace.local_club(club)));
            context.recompute_age_as = selection.age_basis;
            context.cached_age = selection.age;
            context.retain_validated_owner_age = selection.age.has_value();
            ui::StrengthsState page;
            page.projection = ui::strengths_from_game(
                game, local_manager, int(league.id.value), context);
            page.identity = {int(identity.manager),
                             actor,
                             int(game.club_ids.at(size_t(actor))),
                             int(league.id.value),
                             state.date,
                             identity.revision,
                             identity.scope,
                             identity.sequence,
                             page.projection.words.post_draw_settings_word,
                             selection.word};
            // The renderer uses canonical seat identity; projection arithmetic
            // used its bounded local manager.
            page.projection.manager_slot = identity.manager;
            page.league_group = 0;
            page.title =
                message_text(game,
                             layout::CareerSession_FormatError_TextPointer5) +
                ui::utf8_to_original(league.name);
            page.subtitle = ui::utf8_to_original(out.subtitle);
            page.decorated = out.table.decorated_background;
            page.original_panel = out.table.original_table_panel;
            page.gradient = game.memory.u8(layout::TextGradientDisabled) == 0 &&
                            resources.german.u8(layout::TextGradientMode) == 2;
            out.strengths = std::move(page);
        } else if (selection.child == 1) {
            LeaguePairingsView source;
            source.owner = selection.pairings;
            auto& owner = source.owner;
            owner.manager = identity.manager;
            owner.runtime_club = actor;
            owner.club_identity = int(game.club_ids.at(size_t(actor)));
            owner.league = int(league.id.value);
            owner.league_group = 0;
            owner.supported = true;
            source.played_rounds = visible_league_round(state,league.id);
            source.total_rounds = league.rounds();
            source.preferences = selection.pairings_preferences;
            const auto date = [&](int round) -> uint32_t {
                const auto& schedule=selection.display_calendar?selection.display_calendar:state.calendar;
                if (schedule)
                    for (const auto& item : schedule->schedule.leagues)
                        if (item.league == league.id && item.round == round + 1)
                            return state.date - schedule->next_day + item.day;
                return 0xffff;
            };
            source.match_date = date(owner.round);
            TableQuery rank_query;
            rank_query.three_points = points_mode(resources, league) != 0;
            for (const auto& manager : state.world.managers)
                if (manager.match_state &&
                    state.world.clubs.at(manager.club).league == league.id &&
                    manager.match_state->point_deduction >> 5 == 3)
                    rank_query.point_deductions[manager.club] =
                        manager.match_state->point_deduction & 31;
            const auto standings =
                compute_standings(state.world, state.results, league.id,
                                  source.played_rounds, rank_query);
            const auto team = [&](ClubId club) {
                LeaguePairingsTeam result;
                result.runtime_team = int(workspace.local_club(club));
                result.club_identity =
                    int(game.club_ids.at(size_t(result.runtime_team)));
                if (source.played_rounds) {
                    const auto rank = std::find_if(
                        standings.begin(), standings.end(),
                        [&](const auto& row) { return row.club == club; });
                    result.previous_rank = rank->rank;
                }
                constexpr uint16_t flags[] = {0x1f1, 0x1f2, 0x1f4, 0x1f8,
                                              0x1ff};
                for (size_t i = 0; i < 5; ++i)
                    result.strength[i] =
                        team_average(game, result.runtime_team, flags[i]).value;
                return result;
            };
            std::vector<LeaguePairingsRow> rows;
            const bool filtered = owner.filter_active && owner.filter_team >= 0;
            if (!owner.postponed_active)
                for (int round = filtered ? 0 : owner.round;
                     round < (filtered ? league.rounds() : owner.round + 1);
                     ++round) {
                    const auto fixtures = bmh::world12::league_fixtures(
                        state.world, league.id, round + 1);
                    for (size_t i = 0; i < fixtures.size(); ++i) {
                        const auto& fixture = fixtures[i];
                        if (filtered &&
                            int(workspace.local_club(fixture.home)) !=
                                owner.filter_team &&
                            int(workspace.local_club(fixture.away)) !=
                                owner.filter_team)
                            continue;
                        LeaguePairingsRow row;
                        row.home = team(fixture.home);
                        row.away = team(fixture.away);
                        row.source_round = round;
                        row.fixture_index = int(i);
                        const auto score =
                            state.results.result(state.world.catalogue,
                                                 league.id, round + 1, int(i));
                        if (score.played)
                            row.raw_result =
                                uint8_t(score.home << 4 | score.away);
                        const auto moved=std::find_if(state.reschedules.begin(),state.reschedules.end(),[&](const auto& m){return m.league==league.id&&m.round==round+1&&m.match==i;});
                        if(moved!=state.reschedules.end()&&!moved->played){row.raw_result=0xce;row.postponed_date=state.date-state.calendar->next_day+moved->target_day;row.postponed_day=moved->target_day;row.postponed_queue_slot=moved->slot;}
                        if (filtered) {
                            const auto day = moved!=state.reschedules.end()?(state.date-state.calendar->next_day+moved->target_day):date(round);
                            if (day != 0xffff)
                                row.scheduled_date = day;
                        } else if (round >= league.rounds() / 2) {
                            const auto first = bmh::world12::league_fixtures(
                                state.world, league.id,
                                round + 1 - league.rounds() / 2);
                            for (size_t slot = 0; slot < first.size(); ++slot)
                                if (first[slot].home == fixture.away &&
                                    first[slot].away == fixture.home) {
                                    const auto leg = state.results.result(
                                        state.world.catalogue, league.id,
                                        round + 1 - league.rounds() / 2,
                                        int(slot));
                                    if (leg.played)
                                        row.first_leg_result =
                                            uint8_t(leg.home << 4 | leg.away);
                                }
                        }
                        rows.push_back(std::move(row));
                    }
                }
            if(owner.postponed_active)for(const auto& move:state.reschedules)if(move.league==league.id&&!move.played){const auto f=bmh::world12::league_fixtures(state.world,move.league,move.round).at(move.match);LeaguePairingsRow row;row.home=team(f.home);row.away=team(f.away);row.source_round=int(move.round)-1;row.fixture_index=move.match;row.raw_result=0xce;row.postponed_date=state.date-state.calendar->next_day+move.target_day;row.postponed_day=move.target_day;row.postponed_queue_slot=move.slot;rows.push_back(std::move(row));}
            source.total_rows = int(rows.size());
            source.page_offset = owner.postponed_active?owner.postponed_offset:filtered ? owner.filter_offset : 0;
            if (source.page_offset < 0 || source.page_offset > int(rows.size()))
                throw FormatError("world12: invalid pairings page offset");
            const size_t last =
                std::min(rows.size(), size_t(source.page_offset) + 10);
            source.rows.assign(rows.begin() + source.page_offset,
                               rows.begin() + last);
            auto page = ui::league_pairings_from_view(
                game, std::move(source), resources.german,
                ui::utf8_to_original(league.name));
            // Typed results distinguish played scores from Native marker bytes.
            // Preserve the complete legal nibble range, including a played
            // 12:12.
            for (size_t row = 0; row < page.source.rows.size(); ++row) {
                const auto& fixture = page.source.rows[row];
                const auto score = state.results.result(
                    state.world.catalogue, league.id, fixture.source_round + 1,
                    fixture.fixture_index);
                page.rows[row].result =
                    score.played
                        ? std::to_string(score.home) +
                              resources.german.cstring(
                                  layout::
                                      CupDraw_DrawGroupOverview_TextPointer9) +
                              std::to_string(score.away)
                        : fixture.postponed_date?page.rows[row].result:std::string{};
            }
            page.identity = {int(identity.manager),
                             actor,
                             int(game.club_ids.at(size_t(actor))),
                             int(league.id.value),
                             owner.round,
                             identity.revision,
                             identity.scope,
                             identity.sequence};
            if (!owner.postponed_active && !filtered)
                page.subtitle =
                    owner.title_round >= league.rounds()
                        ? message_text(
                              game,
                              layout::CareerSession_FormatError_TextPointer3)
                        : message_text(
                              game,
                              layout::LeaguePairings_FormatError_TextPointer4) +
                              std::to_string(owner.title_round + 1) + ". " +
                              message_text(
                                  game,
                                  layout::
                                      CareerSession_FormatError_TextPointer4);
            out.pairings = std::move(page);
        } else
            throw FormatError("world12: invalid league family child");
    }

    return out;
}
ui::World12TableState
Session::table_page(const CareerSnapshot& state,
                    const ui::World12TableIdentity& identity,
                    const ui::World12TableSelection& selection) const {
    return project_league_family(state, resources_, identity, selection);
}
ui::World12TableState Session::open_tables(const ui::DeskRequest& request) {
    validate_file_owner(request.manager_slot, request.revision);
    if (request.kind != ui::DeskRequestKind::open_menu ||
        (request.menu_kind < 0 || request.menu_kind > 2) ||
        request.value != -1 || table_generation_ >= UINT32_MAX ||
        owner_.revision == UINT64_MAX)
        throw FormatError("world12: invalid table desktop owner");
    const auto p = project_manager_desktop(state_, resources_.reference);
    const auto desk =
        ui::desk_from_game(p.state, resources_.german, owner_.revision);
    if (!visible_world12_menu(desk, request.item_id, request.menu_kind))
        throw FormatError("world12: table icon is not visible/enabled");
    return open_table_owned(*state_.desktop->active_seat,uint8_t(request.menu_kind));
}
ui::World12TableState Session::open_table_owned(uint8_t seat,uint8_t child) {
    if(child>2||table_generation_>=UINT32_MAX||owner_.revision==UINT64_MAX)
        throw FormatError("world12: invalid family window generation");
    auto candidate = state_;
    auto& manager = table_actor(candidate, seat);
    // OpenBMH deviates from the original: pre-schema33 development saves did
    // not represent this display word. First entry initializes that UI-only
    // unknown to InitializeCareer's 0x11; no gameplay history is reconstructed.
    const uint16_t word =
        manager.career->table_display_word.value_or(0x11) & 0xff7f;
    manager.career->table_display_word = word;
    auto selection = ui::initial_world12_table_selection(
        candidate, seat, word,
        points_mode(resources_,
                    candidate.world.catalogue.league(
                        candidate.world.clubs.at(manager.club).league)));
    // OpenBMH deviates from the original: pre-schema34 snapshots have no
    // typed pairings/strengths UI preferences. Their first entry uses the
    // Original new-career defaults; no gameplay state is reconstructed.
    selection.child = child;
    selection.strengths_word =
        manager.career->strengths_display_word.value_or(0x1ff);
    selection.pairings_preferences =
        manager.career->pairings_display_byte.value_or(1) & 0xef;
    selection.pairings.round = std::min<int>(
        candidate.league_rounds.at(selection.league.value),
        candidate.world.catalogue.league(selection.league).rounds() - 1);
    selection.pairings.title_round =
        candidate.league_rounds.at(selection.league.value);
    if (selection.child) {
        selection.word = ui::strengths_family_word_source(
                             selection.word, selection.normal_mode) &
                         0xff7f;
        selection.pairings_preferences =
            uint8_t((selection.pairings_preferences & 0xe0) | 1);
    }
    const ui::World12TableIdentity identity{seat, manager.club,
                                            owner_.revision + 1, owner_.scope,
                                            (table_generation_ + 1) << 32};
    auto view = table_page(candidate, identity, selection);
    retain_draw(selection, view);
    if (selection.child == 2)
        manager.career->strengths_display_word = selection.strengths_word;
    if (selection.child == 1)
        manager.career->pairings_display_byte = selection.pairings_preferences;
    manager.career->table_display_word = selection.word;
    candidate.validate();
    state_ = std::move(candidate);
    ++owner_.revision;
    ++table_generation_;
    table_owner_ = identity;
    table_selection_ = selection;
    preparation_menu_ = 0;
    return view;
}
ui::World12TableState Session::tables_view() const {
    if (preparation_menu_ != 0 || !table_owner_ ||
        table_owner_->revision != owner_.revision ||
        table_owner_->scope != owner_.scope ||
        (table_owner_->sequence >> 32) != table_generation_ ||
        !state_.desktop || state_.desktop->active_seat != table_owner_->manager)
        throw FormatError("world12: stale table window");
    return table_page(state_, *table_owner_, table_selection_);
}
bool Session::apply_tables(const ui::World12TableRequest& request,
                           const std::filesystem::path& data,
                           const ClubInformationPresenter& club_presenter,
                           const TableRangePresenter& range_presenter) {
    const auto current = tables_view();
    const auto identity = *table_owner_;
    if (request.identity != identity || owner_.revision == UINT64_MAX ||
        identity.sequence == UINT64_MAX ||
        ((identity.sequence + 1) >> 32) != table_generation_ ||
        (request.action != ui::World12TableAction::select_league &&
         request.league.valid()) ||
        (request.action != ui::World12TableAction::team_info &&
         request.action != ui::World12TableAction::pairings_control &&
         request.team.valid()) ||
        (request.action != ui::World12TableAction::control &&
         request.action != ui::World12TableAction::navigate &&
         request.action != ui::World12TableAction::pairings_control &&
         request.action != ui::World12TableAction::strengths_control &&
         request.value != -1))
        throw FormatError("world12: stale or malformed table action");
    const auto before = state_;
    const auto generation = table_generation_;
    const auto original_selection = table_selection_;
    auto candidate = state_;
    auto selection = table_selection_;
    switch (request.action) {
    case ui::World12TableAction::close:
        selection.pairings_preferences =
            uint8_t((selection.pairings_preferences & 0xe0) | 1);
        selection.word = uint16_t((selection.word & 0xf7f) |
                                  (selection.normal_mode ? 0x80 : 0));
        break;
    case ui::World12TableAction::select_league:
        if (std::find(current.leagues.begin(), current.leagues.end(),
                      request.league) == current.leagues.end())
            throw FormatError("world12: selected table league is hidden");
        change_table_league(selection, request.league, candidate, resources_);
        break;
    case ui::World12TableAction::next_group: {
        auto group = selection.group;
        do {
            group = TableGroup((uint8_t(group) + 1) % 3);
        } while (!current.groups[size_t(group)]);
        const auto ids = candidate.world.catalogue.table_leagues(group);
        const auto own = candidate.world.clubs.at(identity.club).league;
        selection.group = group;
        change_table_league(selection,
                            std::find(ids.begin(), ids.end(), own) != ids.end()
                                ? own
                                : ids.front(),
                            candidate, resources_);
        break;
    }
    case ui::World12TableAction::team_info:
        if (!request.team.valid() || !visible_club(current, request.team))
            throw FormatError("world12: table club is not visible");
        inspect_club_information(
            candidate, resources_.reference, resources_.german,
            *resources_.texts, request.team, club_presenter,
            ui::LeagueTableScreen(data, current, resources_.german).render());
        break;
    case ui::World12TableAction::control:
        if (selection.child != 0)
            throw FormatError("world12: table control outside table child");
        if (request.value != 9) {
            ui::apply_world12_table_control(selection, request.value,
                                            candidate.world.catalogue);
            break;
        }
        if (!range_presenter)
            throw FormatError(
                "world12: table range requires its visible presenter");
        {
            ui::TableRangeDialogProfile profile;
            profile.minimum = 1;
            profile.maximum =
                candidate.world.catalogue.league(selection.league).rounds();
            profile.default_value = 1;
            const auto p =
                project_manager_desktop(candidate, resources_.reference);
            profile.text_mode =
                p.state.memory.u8(layout::TextGradientDisabled) == 0 &&
                        resources_.german.u8(
                            layout::Advertising_Button_RuntimeField + 1) == 3
                    ? ui::TableRangeTextMode::source_gradient
                    : ui::TableRangeTextMode::flat;
            const auto backdrop =
                ui::LeagueTableScreen(data, current, resources_.german)
                    .render();
            const auto first = range_presenter(profile, backdrop);
            if (!first)
                throw FormatError(
                    "world12: table start-range presenter stopped");
            const auto validate =
                [&](const ui::TableRangeDialogOutcome& result,
                    const ui::TableRangeDialogProfile& limits) {
                    if (uint8_t(result.result) > 1 || result.x < 0 ||
                        result.x > 207 || result.y < 0 || result.y > 104 ||
                        (result.result == ui::TableRangeDialogResult::accept &&
                         (result.value < limits.minimum ||
                          result.value > limits.maximum)))
                        throw FormatError(
                            "world12: invalid table range outcome");
                };
            validate(*first, profile);
            if (first->result == ui::TableRangeDialogResult::accept) {
                profile.title_textd_address = layout::
                    CareerSession_CurrentLeagueTableDispatch_ExecutableField2;
                profile.minimum = first->value;
                profile.default_value = std::max<uint32_t>(
                    1, std::min<uint32_t>(
                           visible_league_round(candidate,selection.league),
                           profile.maximum));
                profile.x = first->x;
                profile.y = first->y;
                const auto last = range_presenter(profile, backdrop);
                if (!last)
                    throw FormatError(
                        "world12: table end-range presenter stopped");
                validate(*last, profile);
                if (last->result == ui::TableRangeDialogResult::accept) {
                    selection.first_round = int(first->value) - 1;
                    selection.end_round = int(last->value);
                }
                selection.word = uint16_t((selection.word & 0xcff) | 0x200);
                ui::normalize_world12_table_range(selection, candidate);
            }
            selection.word &= 0xf7f;
        }
        break;
    case ui::World12TableAction::navigate:
        if (request.value < 0 || request.value > 2 ||
            request.value == selection.child)
            throw FormatError("world12: invalid family navigation");
        selection.child = uint8_t(request.value);
        selection.normal_mode = points_mode(
            resources_, candidate.world.catalogue.league(selection.league));
        selection.first_round = 0;
        selection.end_round =
            visible_league_round(candidate,selection.league);
        ui::normalize_world12_table_range(selection, candidate);
        selection.age.reset();
        selection.age_basis =
            ui::StrengthsAgeBasis::entry_or_league_change_bit10;
        selection.word = ui::strengths_family_word_source(
                             selection.word, selection.normal_mode) &
                         0xff7f;
        selection.pairings = LeaguePairingsOwner{};
        selection.pairings.round = std::min<int>(
            candidate.league_rounds.at(selection.league.value),
            candidate.world.catalogue.league(selection.league).rounds() - 1);
        selection.pairings.title_round =
            candidate.league_rounds.at(selection.league.value);
        selection.pairings_preferences =
            uint8_t((selection.pairings_preferences & 0xe0) | 1);
        break;
    case ui::World12TableAction::strengths_control: {
        if (selection.child != 2 || request.value < 0 || request.value > 12)
            throw FormatError("world12: invalid strengths control");
        const auto changed = ui::strengths_control_word_source(
            selection.strengths_word, ui::StrengthsControl(request.value));
        if (!changed)
            throw FormatError("world12: unavailable strengths control");
        selection.strengths_word = *changed;
        if (request.value == 12) {
            selection.age.reset();
            selection.age_basis = ui::StrengthsAgeBasis::header_toggle_bit15;
        }
        break;
    }
    case ui::World12TableAction::pairings_control: {
        if (selection.child != 1 || request.value < 0 ||
            request.value > int(LeaguePairingsAction::presentation_redraw))
            throw FormatError("world12: invalid pairings control");
        auto& owner = selection.pairings;
        const auto action = LeaguePairingsAction(request.value);
        const int rounds =
            candidate.world.catalogue.league(selection.league).rounds();
        const auto clear = [&](int mode) {
            const int round = owner.round, title = owner.title_round;
            owner = LeaguePairingsOwner{};
            owner.round = round;
            owner.title_round = title;
            selection.pairings_preferences =
                uint8_t((selection.pairings_preferences & 0xe0) | mode);
        };
        const auto clicked = [&]() {
            if (!request.team.valid() || !visible_club(current, request.team))
                throw FormatError("world12: pairings club is not visible");
            const auto i = std::find(current.local_clubs.begin(),
                                     current.local_clubs.end(), request.team);
            return int(i - current.local_clubs.begin());
        };
        if (action != LeaguePairingsAction::search_team &&
            action != LeaguePairingsAction::filter_club && request.team.valid())
            throw FormatError("world12: pairings control has extraneous club");
        switch (action) {
        case LeaguePairingsAction::reset:
            owner.round = std::min<int>(
                candidate.league_rounds.at(selection.league.value), rounds - 1);
            owner.title_round =
                candidate.league_rounds.at(selection.league.value);
            clear(1);
            break;
        case LeaguePairingsAction::previous:
        case LeaguePairingsAction::next:
            if (owner.postponed_active){if(action==LeaguePairingsAction::previous)owner.postponed_offset=std::max(0,owner.postponed_offset-10);else if(owner.postponed_offset+10<current.pairings->source.total_rows)owner.postponed_offset+=10;break;}
            if (owner.filter_active) {
                if (owner.filter_team >= 0) {
                    if (action == LeaguePairingsAction::previous)
                        owner.filter_offset =
                            std::max(0, owner.filter_offset - 10);
                    else if (owner.filter_offset + 10 <
                             current.pairings->source.total_rows)
                        owner.filter_offset += 10;
                }
                break;
            }
            if (selection.pairings_preferences & 3)
                owner.title_round = owner.round =
                    (owner.round + rounds +
                     (action == LeaguePairingsAction::next ? 1 : -1)) %
                    rounds;
            break;
        case LeaguePairingsAction::search:
            clear(2);
            owner.search_first = owner.search_second = -2;
            break;
        case LeaguePairingsAction::filter_by_club:
            clear(4);
            owner.filter_active = true;
            owner.filter_team = -2;
            break;
        case LeaguePairingsAction::postponed_list:
            clear(8);
            owner.postponed_active = true;
            break;
        case LeaguePairingsAction::filter_club:
            if (!owner.filter_active)
                throw FormatError("world12: club filter is not armed");
            owner.filter_team = clicked();
            owner.filter_identity = owner.filter_team;
            owner.filter_offset = 0;
            break;
        case LeaguePairingsAction::search_team: {
            if (owner.search_first == -1 || owner.filter_active ||
                owner.postponed_active)
                throw FormatError("world12: search is not armed");
            const int local = clicked();
            if (!owner.search_second_click) {
                owner.search_first = local;
                owner.search_first_identity = local;
                owner.search_second = -1;
                owner.search_second_click = true;
            } else {
                owner.search_second = local;
                owner.search_second_identity = local;
                for (int round = 1; round <= rounds; ++round) {
                    const auto fixtures = bmh::world12::league_fixtures(
                        candidate.world, selection.league, round);
                    if (std::any_of(
                            fixtures.begin(), fixtures.end(),
                            [&](const auto& f) {
                                return f.home == current.local_clubs.at(size_t(
                                                     owner.search_first)) &&
                                       f.away == request.team;
                            })) {
                        owner.round = owner.title_round = round - 1;
                        owner.search_second_click = false;
                        break;
                    }
                }
            }
            break;
        }
        default:
            throw FormatError(
                "world12: pairings action requires an unavailable owner");
        }
        selection.word = ui::strengths_family_word_source(
                             selection.word, selection.normal_mode) &
                         0xff7f;
        break;
    }
    default:
        throw FormatError("world12: unknown table action");
    }
    if (state_ != before || owner_.revision != identity.revision ||
        table_owner_ != identity || table_generation_ != generation ||
        table_selection_ != original_selection || preparation_menu_ != 0)
        throw FormatError("world12: table presenter changed its live owner");
    auto next = identity;
    ++next.revision;
    ++next.sequence;
    const auto page = table_page(candidate, next, selection);
    retain_draw(selection, page);
    auto& career = *table_actor(candidate, identity.manager).career;
    career.table_display_word = selection.word;
    if (selection.child == 2 || table_selection_.child == 2)
        career.strengths_display_word = selection.strengths_word;
    if (selection.child == 1 || table_selection_.child == 1)
        career.pairings_display_byte = selection.pairings_preferences;
    candidate.validate();
    state_ = std::move(candidate);
    ++owner_.revision;
    if (request.action == ui::World12TableAction::close) {
        table_owner_.reset();
        preparation_menu_.reset();
        return true;
    }
    table_owner_ = next;
    table_selection_ = selection;
    return false;
}
} // namespace bmh::app::world12
