// SPDX-License-Identifier: GPL-3.0-only
#include "app/world12_session.hpp"
#include "app/world12_desktop.hpp"
#include "ui/desktop.hpp"
#include "app/world12_pending.hpp"
#include "game/world12/economy.hpp"
#include "game/world12/codec.hpp"
#include "game/career.hpp"
#include "game/calendar.hpp"
#include "game/schedule.hpp"
#include "formats/data_profile.hpp"
#include "ui/setup.hpp"
#include <stdexcept>
namespace bmh::app::world12 {
using namespace bmh::world12;
RuntimeResources load_runtime_resources(const std::filesystem::path& data) {
    RuntimeResources r;r.master=decode_world(read_file(ui::asset_path(data,"WORLD12.JSON")));
    r.german=load_reference_le(ui::asset_path(data,"BMH.exe"),DataProfile{});
    r.english=load_reference_le(ui::asset_path(data,"RULES-EN.DAT"),DataProfile{});
    if(!r.english.u8(layout::LeagueStrengthRanges+1))throw FormatError("world12: English strength rules are incomplete; re-export RULES-EN.DAT");
    const auto layout_profile=DataProfile::international_11();
    if(r.german.u32(layout::CareerSession_DataDirectory_RuntimeField)!=layout_profile.counts.clubs||r.german.u32(layout::CareerSession_DataDirectory_RuntimeField3)!=layout_profile.counts.players||
        r.german.u16(layout::CareerSession_DispatchLineup_RuntimeField)!=layout_profile.counts.league_teams||!r.english.runtime_policy())throw FormatError("world12: unsupported numeric workspace or missing English runtime policy");
    install_runtime_profile(layout_profile);install_fixture_tables(r.german);uint8_t sizes[kLeagueCount];for(size_t i=0;i<kLeagueCount;++i)sizes[i]=layout_profile.leagues[i];set_league_sizes(sizes);
    r.reference=startup_reference_state(r.german,layout_profile.counts,read_file(ui::asset_path(data,"M3.MEN")),read_file(ui::asset_path(data,"M3.DAT")));
    std::vector<uint16_t> de(366),en(366);for(uint32_t day=0;day<366;++day){de[day]=r.german.u16(layout::CompetitionCalendar+day*2);en[day]=r.english.u16(layout::CompetitionCalendar+day*2);}
    const auto epoch=calendar_epoch_from_executable(r.german);const auto aligned=align_source_calendar(en,int(r.english.runtime_policy()->weekday),epoch.weekday,366);
    r.calendar=SeasonCalendar::from_sources(r.master.catalogue,de,aligned.entries);return r;
}
Session::Session(RuntimeResources resources,ui::SetupOptions options,uint32_t seed,uint64_t scope):resources_(std::move(resources)),options_(options),owner_{scope,1} {
    if(!scope||!options.country_selection||options.managers<1||options.managers>4||options.mode<1||options.mode>3||options.difficulty<0||options.difficulty>9||options.duration<0||options.duration>5)throw FormatError("world12: invalid session setup");
    const auto game_scope=ui::setup_game_scope(options);if(!game_scope.club_play())throw FormatError("world12: national-only session ownership remains unavailable");
    state_.world=select_world_scope(resources_.master,game_scope);
    const auto ranges=read_initial_strength_ranges(state_.world.catalogue,resources_.german,resources_.english);const auto plan=make_player_initialization_plan(resources_.master,game_scope,ranges);
    GameRandom game(0);WatcomRandom random(seed);
    initialize_club_player_state(state_.world,owner_,owner_,plan,resources_.reference,resources_.german.u8(layout::ForeignPlayerLimitEnabled)!=0,game,random);
    state_.game_seed=game.seed();state_.watcom_seed=random.seed();state_.results=SeasonResults::empty(state_.world.catalogue);
}
ui::SelectionContext Session::selection_context()const {return ui::world12_manager_selection_context(state_.world,owner_.revision);}
void Session::confirm_manager(const ui::ManagerChoice& choice,uint64_t revision) {
    if(choice.slot!=int(state_.world.managers.size())||choice.roster_revision!=revision||choice.master_club!=-1||!choice.world_club.valid()||choice.start_league<0||choice.start_league>4||choice.strength<0||choice.strength>2||choice.portrait<0||choice.portrait>11||prepared())throw FormatError("world12: stale or invalid manager chooser result");
    const auto found=state_.world.clubs.find(choice.world_club);if(found==state_.world.clubs.end()||found->second.country!=choice.country)throw FormatError("world12: manager chooser country/club mismatch");
    const auto before=state_;const auto before_owner=owner_;auto candidate=state_;auto staged_owner=owner_;GameRandom game(state_.game_seed);WatcomRandom random(state_.watcom_seed);
    const auto ranges=read_initial_strength_ranges(state_.world.catalogue,resources_.german,resources_.english);
    add_initial_manager(candidate.world,staged_owner,{owner_.scope,revision},{uint8_t(choice.slot),choice.world_club,candidate.world.catalogue.starting_league(choice.country,uint8_t(choice.start_league)),choice.name,uint8_t(choice.portrait),uint8_t(choice.strength)},
        {uint8_t(options_.mode),uint8_t(options_.difficulty),uint8_t(options_.duration),uint8_t(options_.managers),resources_.german.u8(layout::ForeignPlayerLimitEnabled)!=0},ranges,resources_.reference,choice.country==Country::germany?resources_.german:resources_.english,game,random);
    candidate.game_seed=game.seed();candidate.watcom_seed=random.seed();candidate.validate();if(state_!=before||owner_.scope!=before_owner.scope||owner_.revision!=before_owner.revision)throw FormatError("world12: session changed during manager confirmation");state_=std::move(candidate);owner_=staged_owner;
}
void Session::prepare_start(uint64_t revision) {
    if(revision!=owner_.revision||prepared()||state_.world.managers.size()!=size_t(options_.managers))throw FormatError("world12: incomplete/stale start request");
    const auto before=state_;const auto before_owner=owner_;auto candidate=state_;auto staged_owner=owner_;auto calendar=resources_.calendar;
    // Rebuild from the same source dates for the accepted scope; no disabled
    // country appointments are retained from the immutable full catalogue.
    std::vector<uint16_t> de(366),en(366);for(uint32_t d=0;d<366;++d){de[d]=resources_.german.u16(layout::CompetitionCalendar+d*2);en[d]=resources_.english.u16(layout::CompetitionCalendar+d*2);}
    const auto epoch=calendar_epoch_from_executable(resources_.german);const auto aligned=align_source_calendar(en,int(resources_.english.runtime_policy()->weekday),epoch.weekday,366);
    calendar=SeasonCalendar::from_sources(candidate.world.catalogue,de,aligned.entries);
    auto draws=prepare_initial_competitions(candidate,staged_owner,staged_owner,calendar,{resources_.german,resources_.english},resources_.reference);
    auto services=initialize_first_season_manager_services(candidate,staged_owner,staged_owner,resources_.reference);
    initialize_first_season_market(candidate,staged_owner,staged_owner);
    initialize_career_economy(candidate,resources_.reference);
    initialize_desktops(candidate,resources_.reference);
    if(state_!=before||owner_.scope!=before_owner.scope||owner_.revision!=before_owner.revision)throw FormatError("world12: session changed during preparation");
    state_=std::move(candidate);owner_=staged_owner;draws_=std::move(draws);services_=std::move(services);
}
DesktopEntryOutcome Session::begin_first_event(uint64_t revision) {
    return prepare_first_desk_event(state_,owner_,{owner_.scope,revision},resources_.reference);
}

DesktopEntryOutcome Session::advance_manager(const ui::DeskRequest& request) {
    if(request.kind!=ui::DeskRequestKind::advance_turn)throw FormatError("world12: not a manager-advance request");
    validate_file_owner(request.manager_slot,request.revision);
    return advance_initial_manager(state_,owner_,{owner_.scope,request.revision},resources_.reference);
}

PendingOfferResult Session::resolve_pending(const std::filesystem::path& data,const ui::DeskRequest& request,const PendingOfferHooks& hooks) {
    if(request.kind!=ui::DeskRequestKind::advance_turn)throw FormatError("world12: pending offer is not a desk-exit request");validate_file_owner(request.manager_slot,request.revision);
    const auto texts=load_textd(ui::asset_path(data,"TEXTD.BLK"));
    return resolve_initial_offer(state_,owner_,{owner_.scope,request.revision},resources_.reference,texts,hooks);
}

}
