// SPDX-License-Identifier: GPL-3.0-only
// world12_soak - plays whole careers of the shared German/English world without a window.
//
//   world12_soak <data directory> <managers 1..4> <scope: de | en | deen | ende> <seed> <seasons> [digest file]
//
// The managers take the first clubs of the first selectable league of each enabled country in
// turn (German, English, German, ...; with "ende" the English club comes first). Every question of the game is answered the quiet way
// (no to offers and tax options, no bid accepted, every notice acknowledged), every desk is left
// by the kick-off, and every match is played without presentation. After each turn the program
// writes one line with the date and a fingerprint of the saved career to the digest file, checks
// that the career survives a save and a load unchanged at every season change, and stops with
// the date and the reason at the first refusal of the engine. Exit code 0: all seasons played.
//
// This is the regression net of the typed world: two builds that behave the same write the same
// digest file, and a stop names the first place a real career cannot pass.
#include "formats/palette.hpp"
#include "ui/world12_readonly_screen.hpp"
#include "ui/world12_desktop.hpp"
#include "ui/desktop.hpp"
#include "game/world12/manager_change.hpp"
#include "ui/world12_extended_pages.hpp"
#include "app/world12_session.hpp"
#include "app/world12_startup.hpp"
#include "app/world12_pending.hpp"
#include "game/world12/codec.hpp"
#include "game/world12/save.hpp"
#include "game/pending_offer.hpp"
#include "ui/desktop.hpp"
#include "ui/manager.hpp"
#include "ui/save_load.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace bmh;
using namespace bmh::world12;
namespace session_app = bmh::app::world12;

namespace {

// Answers of a manager who declines everything: weekly offers, auctions, incidents.
session_app::ClockHooks quiet_clock_hooks() {
    session_app::ClockHooks hooks;
    hooks.services.incidents.answer = [](const GameState&, uint8_t, const WeeklyOfferQuestion&) -> std::optional<bool> { return false; };
    hooks.services.auctions.allocator = TransferAllocatorContext::zero;
    hooks.services.auctions.select = [](const GameState&, const TransferOfferListView&, const AiMatchProjection&) -> std::optional<int> { return -1; };
    hooks.services.auctions.notice = [](const GameState&, const TransferDecisionEvent&, const AiMatchProjection&) {};
    // An expiring contract is not renewed: the player leaves, as with a manager who never opens the contracts.
    hooks.services.contracts.renew = [](GameState&, GameRandom&, WatcomRandom&, uint8_t, PlayerId, int, const AiMatchProjection&) -> std::optional<bool> { return false; };
    // The advertising relations of every manager's club: champion, cup holders, European field.
    hooks.relations = [](const CareerSnapshot& current) {
        WeeklyIncidentContext context{current.date, current.season, {}};
        const auto& fields = *current.world.initial_competitions;
        for (const auto& manager : current.world.managers) {
            WeeklyClubRelations relation;
            relation.club = manager.club;
            const auto country = current.world.manager_country(manager.seat);
            for (const auto& [c, id] : fields.champions) if (c == country) relation.advertising.champion = id == manager.club;
            for (const auto& [c, id] : fields.domestic_holders) if (c == country) relation.advertising.holders[0] = id == manager.club;
            for (size_t cup = 0; cup < 3; ++cup) {
                relation.advertising.holders[cup + 1] = fields.holders[cup] == manager.club;
                if (std::find(fields.fields[cup].begin(), fields.fields[cup].end(), manager.club) != fields.fields[cup].end())
                    relation.advertising.european_cup = uint8_t(cup + 1);
            }
            context.clubs.push_back(relation);
        }
        return context;
    };
    return hooks;
}

uint64_t fingerprint(const CareerSnapshot& snapshot) {
    const auto save = make_save(snapshot);
    uint64_t h = 14695981039346656037ull;
    for (const auto b : save.stream) h = (h ^ b) * 1099511628211ull;
    return h;
}

}  // namespace

// Writes a 320x240 picture of the game as a 24-bit BMP (pictures for a person to look at).
static void save_picture(const std::filesystem::path& file, const Image& image, const Palette& palette) {
    const int w = image.width, h = image.height, stride = (w * 3 + 3) & ~3;
    std::vector<uint8_t> out(54 + size_t(stride) * h, 0);
    const auto put = [&](size_t at, uint32_t value, int bytes) { for (int i = 0; i < bytes; ++i) out[at + i] = uint8_t(value >> (8 * i)); };
    out[0] = 'B'; out[1] = 'M'; put(2, uint32_t(out.size()), 4); put(10, 54, 4); put(14, 40, 4); put(18, uint32_t(w), 4); put(22, uint32_t(h), 4);
    put(26, 1, 2); put(28, 24, 2); put(34, uint32_t(stride * h), 4);
    for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) {
        const uint32_t colour = palette.argb[image.pixels[size_t(y) * w + x]];
        const size_t at = 54 + size_t(h - 1 - y) * stride + size_t(x) * 3;
        out[at] = uint8_t(colour); out[at + 1] = uint8_t(colour >> 8); out[at + 2] = uint8_t(colour >> 16);
    }
    std::filesystem::create_directories(file.parent_path());
    if (FILE* f = std::fopen(file.string().c_str(), "wb")) { std::fwrite(out.data(), 1, out.size(), f); std::fclose(f); }
}

int main(int argc, char** argv) {
    if (argc < 6) {
        std::fprintf(stderr, "usage: world12_soak <data directory> <managers 1..4> <de|en|deen> <seed> <seasons> [digest file]\n");
        return 2;
    }
    const std::filesystem::path data = argv[1];
    const int managers = std::atoi(argv[2]);
    const std::string scope_name = argv[3];
    const bool extended_screens = scope_name.find('j') != std::string::npos;   // "j": almanac and job offers (Beta 5)
    int club_changes = 0, almanac_views = 0;
    // WORLD12_SOAK_SHOTS=<directory>: pictures of the desk and of the additional screens at each first desk.
    const std::filesystem::path shots = std::getenv("WORLD12_SOAK_SHOTS") ? std::getenv("WORLD12_SOAK_SHOTS") : "";
    const uint32_t seed = uint32_t(std::strtoul(argv[4], nullptr, 0));
    const int seasons = std::atoi(argv[5]);
    std::FILE* digest = argc > 6 ? std::fopen(argv[6], "wb") : nullptr;
    int turns = 0;
    uint32_t date = 0, season = 0;
    int next_day = -1;   // the calendar day the stopped turn was heading for
    std::string phase = "start";
    try {
        if (managers < 1 || managers > 4 || seasons < 1) throw std::invalid_argument("managers 1..4 and at least one season");
        const bool germany = scope_name.find("de") != std::string::npos, england = scope_name.find("en") != std::string::npos;
        if (!germany && !england) throw std::invalid_argument("scope must name de, en or deen");
        const auto resources = session_app::load_runtime_resources(data);
        ui::SetupOptions options;
        options.country_selection = true;
        options.germany = germany;
        options.england = england;
        options.world = scope_name.find('w') != std::string::npos;   // "w": international competitions and the national coach role
        options.league = true;
        options.managers = managers;
        session_app::Session session(resources, options, seed, 71);

        phase = "manager selection";
        std::vector<Country> countries;
        if (germany) countries.push_back(Country::germany);
        if (england) countries.push_back(Country::england);
        if (scope_name.rfind("en", 0) == 0 && countries.size() == 2) std::swap(countries[0], countries[1]);  // "ende": the first manager is English
        std::vector<int> taken(countries.size(), 0);
        for (int m = 0; m < managers; ++m) {
            const size_t which = size_t(m) % countries.size();
            const auto country = countries[which];
            const auto context = session.selection_context();
            const auto league = session.snapshot().world.catalogue.starting_league(country, 0);
            ui::ManagerChoice choice;
            choice.slot = int(session.snapshot().world.managers.size());
            choice.master_club = -1;
            choice.world_club = session.snapshot().world.membership[league.value].clubs.at(size_t(taken[which]++));
            choice.country = country;
            choice.name = "Soak " + std::to_string(choice.slot);
            choice.portrait = choice.slot;
            choice.roster_revision = context.revision;
            session.confirm_manager(choice, context.revision);
        }
        phase = "start";
        session.prepare_start(session.revision());
        session.begin_first_event(session.revision());

        const auto clock = quiet_clock_hooks();
        HumanRoundHooks matches;
        matches.decline_unpresented_events = true;
        session_app::AnnualTransitionHooks annual;
        annual.tax.present = [](const AnnualTaxEvent&) { return true; };
        annual.tax.choose = [](const AnnualTaxEvent&) -> std::optional<bool> { return false; };
        annual.market = clock.services.auctions;
        annual.qualification = [](const UnpublishedAnnualQualification&) { return true; };
        annual.promotion = [](const session_app::AnnualPromotionNotice&) { return true; };
        annual.cup_draw = [](const CupDrawNotice&) { return true; };
        annual.national_draw = [](const NationalCampaign&) { return true; };
        annual.highscore = [](const OwnedHighscoreProjection&, bool) { return true; };
        annual.cup_draw_owned = [](const CareerSnapshot&, const CupDrawNotice&) { return true; };
        annual.national_draw_owned = [](const CareerSnapshot&, const NationalCampaign&) { return true; };
        PendingOfferHooks pending;
        pending.answer = [](const GameState&, const PendingOfferQuestion&) { return PendingOfferAnswer::no; };
        pending.select_transfer = [](const GameState&, const TransferOfferListView&) { return PendingOfferTransferChoice{PendingOfferTransferChoiceKind::decline, -1}; };
        pending.notice = [](const GameState&, const PendingOfferNotice&) {};
        pending.human_buyer_allocator_gate = PendingOfferAllocatorGate::zero;
        pending.program_error = [](const GameState&, uint16_t) {};

        const std::filesystem::path score_file = std::filesystem::temp_directory_path() / "world12_soak_highscore.dat";
        const uint32_t first_season = session.snapshot().season;
        bool complete = false;
        while (!complete) {
            const auto& snapshot = session.snapshot();
            date = uint32_t(snapshot.date); season = uint32_t(snapshot.season);
            if (int(season - first_season) >= seasons) break;
            if (!snapshot.desktop || !snapshot.desktop->active_seat || !snapshot.desktop->initial_event)
                throw std::runtime_error("no owned desktop to leave");
            if (snapshot.desktop->autosave_pending) {
                // The automatic save of the desk (every few months): the save is prepared as the game does
                // it (it draws from the Watcom generator) and must read back; no file is written.
                phase = "automatic save";
                ui::SaveLoadRequest save;
                save.action = ui::SaveLoadAction::save;
                save.file_kind = ui::SaveLoadFileKind::game;
                save.manager_slot = int(*snapshot.desktop->active_seat);
                save.revision = session.revision();
                save.path = std::filesystem::absolute(std::filesystem::temp_directory_path() / "AUTOSAVE.FBM");
                save.overwrite = true;
                (void)read_save(session.prepare_manual_save(save));
                continue;
            }
            ui::DeskRequest leave;
            leave.kind = ui::DeskRequestKind::advance_turn;
            leave.manager_slot = int(*snapshot.desktop->active_seat);
            leave.revision = session.revision();
            phase = "desk of seat " + std::to_string(leave.manager_slot);
            if (snapshot.desktop->initial_event->offer) {
                phase += ", pending offers";
                session.resolve_pending(data, leave, pending);
                leave.revision = session.revision();
            }
            if (extended_screens && session.snapshot().calendar && session.snapshot().calendar->next_day == 0 && !session.snapshot().prepared_event) {
                // First desk of a season. The almanac is built and closed; a manager with an offer he may
                // accept takes the first one.
                const auto open = [&](int item, int menu) {
                    ui::DeskRequest request;
                    request.kind = ui::DeskRequestKind::open_menu; request.manager_slot = leave.manager_slot;
                    request.item_id = item; request.menu_kind = menu; request.revision = session.revision();
                    return session.open_readonly(request, data, Image(320, 240));
                };
                const std::string tag = "s" + std::to_string(session.snapshot().season) + "_seat" + std::to_string(leave.manager_slot) + "_";
                const auto picture = [&](const std::string& name, const Image& image) {
                    if (!shots.empty()) save_picture(shots / (tag + name + ".bmp"), image, load_palette(ui::asset_path(data, "M3PC.COL")));
                };
                const auto desk_picture = [&](const std::string& name) {
                    if (shots.empty()) return;
                    ui::DesktopScreen desk(data, ui::world12_initial_desktop(data, session.snapshot(), session.reference(), session.rules(), session.options(), session.revision()), session.rules());
                    picture(name, desk.render());
                };
                desk_picture("desk");
                phase += ", almanac";
                { const auto page = open(54, ui::kMenuAlmanac);
                  if (!page.almanac || page.almanac->tabs.size() != 6) throw std::runtime_error("almanac has not six tabs");
                  if (!shots.empty()) { ui::World12ReadonlyScreen screen(data, page, session.rules());
                      for (int tab = 0; tab < 6; ++tab) { picture("almanac" + std::to_string(tab), screen.render()); screen.key("TAB"); } }
                  ui::World12ReadonlyRequest close; close.identity = page.identity;
                  if (!session.apply_readonly(close)) throw std::runtime_error("almanac did not close");
                  ++almanac_views; }
                const auto& now = session.snapshot(); const auto seat = *now.desktop->active_seat;
                const bool club_desk = now.world.clubs.at(std::find_if(now.world.managers.begin(), now.world.managers.end(), [&](const auto& m) { return m.seat == seat; })->club).kind == ClubKind::club;
                if (club_desk && now.world.extended && now.world.extended->job_offer_season == now.season &&
                    now.world.extended->job_offers.contains(seat) && manager_change_refusal(now, seat).empty()) {
                    phase += ", job offer";
                    const auto page = open(55, ui::kMenuJobOffers);
                    if (!page.job_offers || page.job_offers->offers.empty()) throw std::runtime_error("offer screen shows no offer");
                    if (!shots.empty()) { ui::World12ReadonlyScreen screen(data, page, session.rules()); picture("offers", screen.render());
                        screen.pointer_down(100, 72, false); picture("offers_confirm", screen.render()); }
                    ui::World12ReadonlyRequest accept; accept.identity = page.identity;
                    accept.action = ui::World12ReadonlyAction::accept_job_offer; accept.value = 0;
                    if (!session.apply_readonly(accept)) throw std::runtime_error("accepted offer did not close its screen");
                    ++club_changes;
                    desk_picture("desk_after_change");
                }
                leave.revision = session.revision();
            }
            const auto& event = *session.snapshot().desktop->initial_event;
            if (size_t(event.manager_position) + 1 < event.manager_order.size()) {
                session.advance_manager(leave);
                continue;
            }
            phase += ", turn";
            next_day = session.snapshot().calendar ? int(session.snapshot().calendar->next_day) : -1;
            const uint32_t season_before = uint32_t(session.snapshot().season);
            complete = session.advance_to_next_desktop(leave, data, clock, matches, &annual, score_file);
            ++turns;
            const auto& after = session.snapshot();
            if (digest) {
                std::fprintf(digest, "turn %d season %u day %u digest %016llx\n", turns, unsigned(after.season), unsigned(after.date),
                             static_cast<unsigned long long>(fingerprint(after)));
                std::fflush(digest);
            }
            if (uint32_t(after.season) != season_before) {
                phase = "save round trip at the season change";
                if (!(read_save(make_save(after)) == after)) throw std::runtime_error("the career differs after save and load");
                std::printf("season %u complete after %d turns\n", season_before, turns);
                std::fflush(stdout);
            }
        }
        if (extended_screens) std::printf("almanac opened %d times, %d changes of club\n", almanac_views, club_changes);
        std::printf("%s: %d seasons, %d turns, %d managers, scope %s, seed %u\n", complete ? "career complete" : "played",
                    int(session.snapshot().season - first_season), turns, managers, scope_name.c_str(), unsigned(seed));
        if (digest) std::fclose(digest);
        return 0;
    } catch (const std::exception& e) {
        std::printf("STOP season %u day %u (calendar day %d) after %d turns (%s): %s\n", unsigned(season), unsigned(date), next_day, turns, phase.c_str(), e.what());
        if (digest) std::fclose(digest);
        return 1;
    }
}
