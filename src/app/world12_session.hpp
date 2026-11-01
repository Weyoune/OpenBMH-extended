// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "app/world12_initial_season.hpp"
#include "app/world12_first_event.hpp"
#include "app/world12_clock.hpp"
#include "formats/le.hpp"
#include "ui/manager.hpp"
#include "ui/world12_manager_selection.hpp"
#include "ui/preparation.hpp"
#include "game/pending_offer.hpp"
namespace bmh::ui {struct DeskRequest;struct SaveLoadRequest;struct MailboxState;struct MailboxActionRequest;struct MailboxCloseRequest;}
namespace bmh::app::world12 {
struct RuntimeResources {
    bmh::world12::World master;
    LeImage german,english;
    GameState reference;
    bmh::world12::SeasonCalendar calendar;
};
// Explicit reference layout is a Core workspace, never legacy admission of the
// dual-world package. Actual club/player data comes exclusively from WORLD12.
RuntimeResources load_runtime_resources(const std::filesystem::path& directory);
class Session {
public:
    Session(RuntimeResources,ui::SetupOptions,uint32_t seed,uint64_t scope);
    static Session resume(RuntimeResources,bmh::world12::CareerSnapshot,uint64_t scope,uint64_t revision);
    const ui::SetupOptions& options()const{return options_;}
    Savegame prepare_manual_save(const ui::SaveLoadRequest&);
    void validate_file_owner(int manager_slot,uint64_t revision)const;
    bool preparation_open()const{return preparation_menu_.has_value();}
    ui::SelectionContext selection_context()const;
    void confirm_manager(const ui::ManagerChoice&,uint64_t expected_revision);
    void prepare_start(uint64_t expected_revision);
    DesktopEntryOutcome begin_first_event(uint64_t expected_revision);
    DesktopEntryOutcome advance_manager(const ui::DeskRequest&);
    void finish_initial_day(const ui::DeskRequest&);
    ClockStepOutcome advance_saved_clock(uint64_t expected_revision,const std::filesystem::path&,const ClockHooks&);
    ClockStepOutcome seek_saved_event(uint64_t expected_revision,const std::filesystem::path&,const ClockHooks&);

    PendingOfferResult resolve_pending(const std::filesystem::path&,const ui::DeskRequest&,const PendingOfferHooks&);
    void open_mailbox(const ui::DeskRequest&);
    ui::MailboxState mailbox_view()const;
    void edit_mailbox(const ui::MailboxActionRequest&);
    void close_mailbox(const ui::MailboxCloseRequest&);
    void open_preparation(const ui::DeskRequest&);
    void apply_preparation(const ui::PreparationRequest&);
    void close_preparation();
    ui::PreparationState preparation_view(const std::filesystem::path&)const;
    const bmh::world12::CareerSnapshot& snapshot()const{return state_;}
    uint64_t revision()const{return owner_.revision;}
    uint64_t scope()const{return owner_.scope;}
    const InitialCompetitionOutcome& initial_draws()const{return draws_;}
    const InitialManagerServicesOutcome& initial_services()const{return services_;}
    const GameState& reference()const{return resources_.reference;}
    const LeImage& rules()const{return resources_.german;}
    bool prepared()const{return state_.world.transfer_market.has_value();}
private:
    struct ResumeTag {};
    Session(RuntimeResources,bmh::world12::CareerSnapshot,uint64_t scope,uint64_t revision,ResumeTag);
    RuntimeResources resources_;ui::SetupOptions options_;
    bmh::world12::CareerSnapshot state_;ManagerActionOwner owner_;
    InitialCompetitionOutcome draws_;InitialManagerServicesOutcome services_;
    std::optional<int> preparation_menu_;
};
}
