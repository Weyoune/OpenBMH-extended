// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <memory>
#include "formats/textd.hpp"
#include "app/world12_initial_season.hpp"
#include "app/world12_first_event.hpp"
#include "app/world12_clock.hpp"
#include "app/world12_national.hpp"
#include "app/world12_ai_day.hpp"
#include "app/world12_weather.hpp"
#include "app/world12_attendance.hpp"
#include "app/world12_event.hpp"
#include "app/contract_source_types.hpp"
#include "app/stadium_source_types.hpp"
#include "ui/stadium.hpp"
#include "ui/toto.hpp"
#include "ui/settings.hpp"
#include "app/world12_advertising.hpp"
#include "ui/advertising.hpp"
#include "formats/le.hpp"
#include "ui/manager.hpp"
#include "ui/world12_manager_selection.hpp"
#include "ui/preparation.hpp"
#include "ui/environment.hpp"
#include "ui/contracts.hpp"
#include "game/pending_offer.hpp"
namespace bmh::ui {struct DeskRequest;struct SaveLoadRequest;struct MailboxState;struct MailboxActionRequest;struct MailboxCloseRequest;}
namespace bmh::app::world12 {
struct RuntimeResources {
    bmh::world12::World master;
    LeImage german,english;
    GameState reference;
    std::vector<uint8_t> referee_properties;
    std::vector<std::string> referee_names;
    bmh::world12::SeasonCalendar calendar;
    std::shared_ptr<const TextDb> texts;
};
// Explicit reference layout is a Core workspace, never legacy admission of the
// dual-world package. Actual club/player data comes exclusively from WORLD12.
RuntimeResources load_runtime_resources(const std::filesystem::path& directory);
class Session {
public:
    Session(RuntimeResources,ui::SetupOptions,uint32_t seed,uint64_t scope);
    static Session resume(RuntimeResources,bmh::world12::CareerSnapshot,uint64_t scope,uint64_t revision);
    const ui::SetupOptions& options()const{return options_;}
    Bytes prepare_menu_layout(const ui::SaveLoadRequest&,const std::optional<Bytes>& incoming=std::nullopt);
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
    NationalAppointmentOutcome prepare_national_day(uint64_t expected_revision);
    NationalAppointmentOutcome finish_uncoached_national_day(uint64_t expected_revision);
    CupDayOutcome play_scheduled_domestic_cups(uint64_t expected_revision,
        const std::vector<CupDayConditions>&,const bmh::world12::HumanRoundHooks&);
    AiDayOutcome play_scheduled_leagues(uint64_t expected_revision,
        const std::vector<AiDayLeagueConditions>&,const bmh::world12::HumanRoundHooks&);
    WeatherPreparation prepare_scheduled_weather(uint64_t expected_revision);
    DesktopEntryOutcome begin_scheduled_desks(uint64_t expected_revision);
    void finish_scheduled_desks(const ui::DeskRequest&);
    // One staged transition from the last desk to the next owned desktop.
    // Synchronous questions/matches need actual presenters; missing owners reject.
    void advance_to_next_desktop(const ui::DeskRequest&,const std::filesystem::path& data,
        const ClockHooks&,const bmh::world12::HumanRoundHooks&);
    AttendanceOutcome book_scheduled_attendance(uint64_t expected_revision,const std::filesystem::path& data);
    void play_prepared_matches(uint64_t expected_revision,const bmh::world12::HumanRoundHooks&);

    PendingOfferResult resolve_pending(const std::filesystem::path&,const ui::DeskRequest&,const PendingOfferHooks&);
    void open_mailbox(const ui::DeskRequest&);
    ui::MailboxState mailbox_view()const;
    void edit_mailbox(const ui::MailboxActionRequest&);
    void close_mailbox(const ui::MailboxCloseRequest&);
    ui::EnvironmentView open_environment(const ui::DeskRequest&,const std::filesystem::path& data);
    ui::EnvironmentView environment_view(const std::filesystem::path& data)const;
    bool apply_environment(const ui::EnvironmentRequest&);
    ui::ContractsState open_contracts(const ui::DeskRequest&,const std::filesystem::path& data);
    ui::ContractsState contracts_view(const std::filesystem::path& data)const;
    ui::ContractActionResult apply_contract(const std::filesystem::path& data,const ui::ContractRequest&);
    void close_contracts();
    ui::SettingsState open_settings(const ui::DeskRequest&,std::optional<ui::SettingsSoundCapabilities> sound=std::nullopt);
    ui::SettingsState settings_view()const;
    uint64_t settings_sequence()const{return settings_generation_;}
    void apply_settings(const ui::SettingsWriteRequest&,uint64_t sequence);
    Bytes prepare_settings_file(const ui::SaveLoadRequest&,uint64_t sequence,const std::optional<Bytes>& incoming=std::nullopt);
    void close_settings(uint64_t sequence,uint64_t revision);
    void apply_desktop(const ui::DeskRequest&,const std::filesystem::path& data);
    void open_toto(const ui::DeskRequest&,const std::filesystem::path& data);
    ui::TotoState toto_view(const std::filesystem::path& data)const;
    void apply_toto(const ui::TotoRequest&);
    void open_stadium(const ui::DeskRequest&,const std::filesystem::path& data);
    ui::StadiumState stadium_view(const std::filesystem::path& data)const;
    void apply_stadium(const std::filesystem::path& data,const ui::StadiumRequest&,const ConstructionNoticeHook& notice={},const SeasonTicketCompensationNoticeHook& compensation={});
    ui::AdvertisingState open_advertising(const ui::DeskRequest&,const std::filesystem::path& data);
    ui::AdvertisingState advertising_view(const std::filesystem::path& data)const;
    AdvertisingSpendingResult apply_advertising(const std::filesystem::path& data,const ui::AdvertisingRequest&,const AdvertisingRenewalPresenter& presenter={});
    void open_preparation(const ui::DeskRequest&);
    void apply_preparation(const ui::PreparationRequest&);
    void apply_bank(const std::filesystem::path& data,const ui::BankRequest&);
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
    std::optional<int> bank_tip_;
    std::optional<ui::BankState::PropertyNotice> bank_property_notice_;
    std::optional<ui::BankState::CreditNotice> bank_credit_notice_;
    std::optional<ui::DeskRequest> environment_owner_;
    uint64_t environment_request_=0;
    uint64_t environment_generation_=0;
    std::optional<ContractSourceQuote> contract_quote_;
    bmh::world12::PlayerId contract_player_;
    bmh::world12::ClubId contract_club_;
    std::optional<uint8_t> contract_seat_;
    uint64_t contract_generation_=0,contract_request_=0,next_contract_quote_=1;
    uint64_t advertising_generation_=0,advertising_request_=0;
    StadiumSourceRuntime stadium_;
    uint64_t next_stadium_scope_=1;
    uint64_t toto_generation_=0,toto_request_=0;
    uint64_t settings_generation_=0;
    std::optional<ui::DeskRequest> settings_owner_;
    std::optional<ui::SettingsSoundCapabilities> settings_sound_;
    void validate_settings_owner(uint64_t sequence,uint64_t revision)const;
};
}
