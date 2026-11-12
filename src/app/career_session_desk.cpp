// SPDX-License-Identifier: GPL-3.0-only
// CareerSession: the desk: mailbox, settings, advertising, contracts, desk settings, search profile, widgets, layout.
// (moved out of career_session.cpp unchanged; the class is declared in app/career_session.hpp)
#include "app/career_session_internal.hpp"

namespace bmh::app {
// Mailbox actions on one of the 40 entries for the active manager: keep toggle or delete. The
// request must carry the current revision, the active manager slot and the expected entry bytes
// (stale entries are refused); the manager's bit in the recipient or kept word is flipped and the
// revision advances. Returns false with a player-visible German text in 'error' otherwise.
bool CareerSession::dispatch_mailbox(const ui::MailboxActionRequest& request, uint64_t& revision,
                                     std::string& error) {
    if (balance_draft_ || balance_dispatch_active_ || strengths_owner_ || strengths_dispatch_active_ ||
        top_scorers_owner_ || top_scorers_dispatch_active_ || stadium_.block_expansion || other_games_owner_ || other_games_dispatch_active_) { error = "strengths_active"; return false; }
    const int active = int(g_.memory.u8(::bmh::layout::ActiveManager));
    if (active < 0 || active > 3 || size_t(active) >= g_.manager_teams.size() ||
        request.manager_slot != active || request.revision != revision ||
        revision == std::numeric_limits<uint64_t>::max()) {
        error = "Diese Nachricht gehört zu einem veralteten Postfach.";
        return false;
    }
    MailboxEditKind kind=static_cast<MailboxEditKind>(255);
    if(request.kind==ui::MailboxActionKind::toggle_keep)kind=MailboxEditKind::toggle_keep;
    else if(request.kind==ui::MailboxActionKind::delete_for_manager)kind=MailboxEditKind::delete_for_manager;
    switch(edit_mailbox_entry(g_.mailbox,active,request.entry_index,request.expected_entry,kind)){
    case MailboxEditFailure::none:break;
    case MailboxEditFailure::missing:error="Diese Nachricht ist nicht mehr vorhanden.";return false;
    case MailboxEditFailure::changed:error="Diese Nachricht wurde inzwischen geändert.";return false;
    case MailboxEditFailure::not_recipient:error="Diese Nachricht ist nicht an diesen Manager gerichtet.";return false;
    case MailboxEditFailure::keep_limit:error="Die Originalgrenze für gemerkte Nachrichten ist erreicht.";return false;
    case MailboxEditFailure::kept:error="Diese Nachricht ist noch gemerkt.";return false;
    default:error="Unbekannte Postfachaktion.";return false;
    }
    ++revision;
    return true;
}

// Returns the mailbox page when the active human manager still has the notification bit set,
// nothing otherwise (no change of state).
std::optional<ui::MailboxState> CareerSession::pending_mailbox_notification(uint64_t revision) const {
    if (!g_.memory.has(::bmh::layout::ActiveManager)) return std::nullopt;
    const int manager = int(g_.memory.u8(::bmh::layout::ActiveManager));
    if (manager < 0 || manager >= 4 || size_t(manager) >= g_.manager_teams.size() ||
        g_.manager_teams[size_t(manager)] == kNoTeam || !g_.memory.has(::bmh::layout::MailboxRecipientMask) ||
        (g_.memory.u32(::bmh::layout::MailboxRecipientMask) & (uint32_t(1) << manager)) == 0)
        return std::nullopt;
    return ui::mailbox_state_from_game(g_, manager, revision);
}

// Acknowledges closing the mailbox for the active manager: clears only that manager's
// notification bit (kept outside the saved data, so the revision is not advanced).
bool CareerSession::acknowledge_mailbox_close(const ui::MailboxCloseRequest& request,
                                               uint64_t revision, std::string& error) {
    if (!g_.memory.has(::bmh::layout::ActiveManager) || request.manager_slot < 0 || request.manager_slot >= 4 ||
        size_t(request.manager_slot) >= g_.manager_teams.size() ||
        request.manager_slot != int(g_.memory.u8(::bmh::layout::ActiveManager)) || request.revision != revision) {
        error = "Diese Nachrichtenansicht gehört zu einem veralteten Manager oder Spielstand.";
        return false;
    }
    // MailboxStateFromGame clears only this manager's MailboxRecipientMask bit on normal exit.
    // The latch lives outside the FBM data blocks and does not advance game revision.
    if (g_.memory.has(::bmh::layout::MailboxRecipientMask)) {
        const uint32_t notifications = g_.memory.u32(::bmh::layout::MailboxRecipientMask);
        const uint32_t bit = uint32_t(1) << request.manager_slot;
        if (notifications & bit) g_.memory.set32(::bmh::layout::MailboxRecipientMask, notifications & ~bit);
    }
    error.clear();
    return true;
}

// Settings group (desk settings, live settings, settings files). The options grid has 20 rows x 9
// columns stored as 180 bytes: columns 0-6 are two-state switches, 7 and 8 numeric steppers.
// Write of one cell: refuses while a scoped settings owner is open unless called by it, checks
// the active manager, revision and that the posted raw block still equals the saved one, then
// applies the allowed edit (locks and original limits respected) and bumps the revision.
// Returns false with a player-visible German text in 'error' otherwise.
bool CareerSession::dispatch_settings(const ui::SettingsWriteRequest& request, uint64_t& revision,
                                      std::string& error) {
    if ((live_settings_owner_ && !live_settings_dispatch_active_) ||
        (desk_settings_owner_ && !desk_settings_dispatch_active_)) {
        error = "Diese Einstellung gehört zum geöffneten Einstellungsmenü.";
        return false;
    }
    if (balance_draft_ || balance_dispatch_active_ || strengths_owner_ || strengths_dispatch_active_ ||
        top_scorers_owner_ || top_scorers_dispatch_active_ || stadium_.block_expansion || other_games_owner_ || other_games_dispatch_active_) { error = "strengths_active"; return false; }
    const bool scoped = (live_settings_owner_ && live_settings_dispatch_active_) ||
        (desk_settings_owner_ && desk_settings_dispatch_active_);
    return apply_settings_request_source(g_,exe_,sound_capabilities_,scoped,request,revision,error);
}

// Opens the settings page from a running match (the 'S' request): only inside a calendar advance
// with the engine bound to this session, the manager at the active desk and no other modal
// owner. Works on a staged copy of the state and runtime bytes, commits it together with the new
// owner and revision on success; empty optional with 'error' on failure.
std::optional<CareerSession::LiveSettingsProjection> CareerSession::open_live_settings(
    const LiveSettingsRequest& request,MatchEngine& engine,uint64_t& revision,std::string& error) {
    error.clear();
    if(!calendar_advance_active_||live_settings_owner_||request.minute<0||request.minute>255||
       request.manager_slot<0||request.manager_slot>=int(g_.manager_teams.size())||
       request.revision!=revision||revision==UINT64_MAX||next_live_settings_scope_==UINT64_MAX||
       &engine.state!=&g_||&engine.game!=&game_||&engine.rand!=&rand_||
       engine.groups.empty()||engine.groups.size()!=turn_driver_.groups.size()||
       !g_.memory.has(::bmh::layout::ActiveManager)||g_.memory.u8(::bmh::layout::ActiveManager)!=request.manager_slot||
       size_t(request.manager_slot)>=g_.manager_teams.size()||
       g_.manager_teams[size_t(request.manager_slot)]!=request.runtime_team||
       request.runtime_team<0||size_t(request.runtime_team)>=g_.team_flags.size()||
       live_overview_owner_||live_intervention_||balance_draft_||balance_dispatch_active_||
       strengths_owner_||strengths_dispatch_active_||league_family_owner_||league_pairings_draft_||
       league_pairings_dispatch_active_||other_games_owner_||other_games_dispatch_active_||
       top_scorers_owner_||top_scorers_dispatch_active_||pending_offer_dispatch_active_||market_dispatch_active_||
       transfer_offer_editor_||manager_join_||contract_quote_||stadium_.investment||stadium_.tip||
       stadium_.periphery_quote||stadium_.block_expansion) {
        error="live settings: stale, nested or unauthenticated Source S request";return {};
    }
    for(size_t i=0;i<engine.groups.size();++i)
        if(engine.groups[i].desc!=turn_driver_.groups[i].desc||engine.groups[i].fixtures!=turn_driver_.groups[i].fixtures) {
            error="live settings: match descriptors changed outside the observer";return {};
        }
    if(!g_.memory.has(::bmh::layout::LeaguePresentationOptions+179)||!g_.memory.has(::bmh::layout::GameMode)||!g_.memory.has(::bmh::layout::CompetitionSelection)) {
        error="live settings: Source switch block is incomplete";return {};
    }
    GameState staged=g_;
    auto runtime=club_modal_runtime_;
    try {
        std::array<uint8_t,180> raw{};
        for(size_t i=0;i<raw.size();++i) {
            const uint32_t address=::bmh::layout::LeaguePresentationOptions+uint32_t(i);
            if(!staged.memory.has(address))throw FormatError("live settings: Source switch block is incomplete");
            raw[i]=staged.memory.u8(address);
        }
        const auto entry=ui::project_settings_availability(raw,
            {staged.memory.u8(::bmh::layout::GameMode),staged.memory.u32(::bmh::layout::CompetitionSelection),0,0},exe_);
        for(size_t i=0;i<entry.size();++i) {
            // A6/A7 availability is projected from the host's runtime capability.
            // Keep their saved switches intact until an explicit user write.
            const uint8_t value=(i==5||i==6)?raw[i]:entry[i];
            staged.memory.set8(::bmh::layout::LeaguePresentationOptions+uint32_t(i),value);
        }
        source_runtime_set_byte(staged,runtime,::bmh::layout::CareerSession_LiveBoundaryFamilyScope_ExecutableField,0); // SavePathInDirectory, before SettingsSourceRefusal.
        auto state=ui::settings_from_game(staged,request.manager_slot,revision+1,exe_,sound_capabilities_);
        ui::SettingsScreen validate(data_directory_,state);
        std::vector<uint8_t> buttons(180);
        for(size_t i=0;i<buttons.size();++i)buttons[i]=staged.memory.u8(::bmh::layout::LeaguePresentationOptions+uint32_t(i));
        const bool live_desk_disabled=source_runtime_byte(staged,runtime,exe_,::bmh::layout::LiveDeskDisabled)!=0;
        LiveSettingsRequest owner=request;owner.sequence=next_live_settings_scope_;owner.revision=revision+1;
        LiveSettingsOwner next;next.request=owner;next.native=save_stream(staged);
        next.expected_buttons=buttons;next.expected_live_desk_disabled=live_desk_disabled;next.expected_live_scene_visible=engine.live_scene_visible;
        next.expected_runtime=runtime;
        next.rand_seed=rand_.seed();next.game_seed=game_.seed();next.rand_calls=rand_.calls();next.game_calls=game_.calls();
        for(const auto& group:engine.groups)next.active_groups.emplace_back(group.desc,group.fixtures);
        next.referees=engine.referees;
        LiveSettingsProjection projection{owner,std::move(state)};
        g_=std::move(staged);club_modal_runtime_=std::move(runtime);
        engine.buttons=std::move(buttons);engine.live_desk_disabled=live_desk_disabled;
        live_settings_owner_=std::move(next);++next_live_settings_scope_;revision=owner.revision;
        return projection;
    } catch(const std::exception& e) {
        error=e.what();return {};
    }
}

// Applies one action of the open live settings page (cell write or settings file request). The
// owner snapshot, engine view, match groups and random counters must be unchanged since the
// last page; failures return an empty optional with 'error' and roll the write back.
std::optional<CareerSession::LiveSettingsDispatch> CareerSession::dispatch_live_settings(
    const LiveSettingsAction& action,MatchEngine& engine,uint64_t& revision,std::string& error) {
    error.clear();
    if(!calendar_advance_active_||!live_settings_owner_||revision==UINT64_MAX||
       live_settings_owner_->request.revision!=revision||&engine.state!=&g_||
       &engine.game!=&game_||&engine.rand!=&rand_||engine.groups.size()!=live_settings_owner_->active_groups.size()||
       engine.referees!=live_settings_owner_->referees||save_stream(g_)!=live_settings_owner_->native||
       engine.buttons!=live_settings_owner_->expected_buttons||engine.live_desk_disabled!=live_settings_owner_->expected_live_desk_disabled||
       engine.live_scene_visible!=live_settings_owner_->expected_live_scene_visible||club_modal_runtime_!=live_settings_owner_->expected_runtime||
       rand_.seed()!=live_settings_owner_->rand_seed||rand_.calls()!=live_settings_owner_->rand_calls||
       game_.seed()!=live_settings_owner_->game_seed||game_.calls()!=live_settings_owner_->game_calls||
       !g_.memory.has(::bmh::layout::ActiveManager)||g_.memory.u8(::bmh::layout::ActiveManager)!=live_settings_owner_->request.manager_slot||
       size_t(live_settings_owner_->request.manager_slot)>=g_.manager_teams.size()||
       g_.manager_teams[size_t(live_settings_owner_->request.manager_slot)]!=live_settings_owner_->request.runtime_team) {
        error="live settings: stale owner or no longer at the Source observer";return {};
    }
    for(size_t i=0;i<engine.groups.size();++i)
        if(engine.groups[i].desc!=live_settings_owner_->active_groups[i].first||
           engine.groups[i].fixtures!=live_settings_owner_->active_groups[i].second||
           engine.groups[i].desc!=turn_driver_.groups[i].desc||engine.groups[i].fixtures!=turn_driver_.groups[i].fixtures) {
            error="live settings: active Source match list changed";return {};
        }
    const auto current=*live_settings_owner_;
    // Scope guard: calendar_advance_active_ is off while the nested settings code runs and is
    // restored on exit.
    struct LiveSettingsBoundaryScope {
        bool& active;bool previous;
        explicit LiveSettingsBoundaryScope(bool& value):active(value),previous(value){active=false;}
        ~LiveSettingsBoundaryScope(){active=previous;}
    } boundary(calendar_advance_active_);
    std::optional<GameState> write_rollback;
    const uint64_t revision_before_action=revision;
    try {
        if(const auto* write=std::get_if<LiveSettingsWrite>(&action)) {
            if(write->owner_sequence!=current.request.sequence||write->write.manager_slot!=current.request.manager_slot||
               write->write.revision!=revision)
                throw FormatError("live settings: write belongs to another Source owner");
            write_rollback=g_;
            live_settings_dispatch_active_=true;
            struct Reset { bool& active;~Reset(){active=false;} } reset{live_settings_dispatch_active_};
            uint64_t staged_revision=revision;
            if(!dispatch_settings(write->write,staged_revision,error))
                throw FormatError(error.empty()?"live settings: Source cell write rejected":error);
            GameState staged=std::move(g_);
            g_=*write_rollback;
            std::array<uint8_t,180> raw{};
            for(size_t i=0;i<raw.size();++i)raw[i]=staged.memory.u8(::bmh::layout::LeaguePresentationOptions+uint32_t(i));
            const auto projected=ui::project_settings_availability(raw,
                {staged.memory.u8(::bmh::layout::GameMode),staged.memory.u32(::bmh::layout::CompetitionSelection),0,0},exe_);
            for(size_t i=0;i<projected.size();++i)
                if(i!=5&&i!=6)staged.memory.set8(::bmh::layout::LeaguePresentationOptions+uint32_t(i),projected[i]);
            std::vector<uint8_t> buttons(180);
            for(size_t i=0;i<buttons.size();++i)buttons[i]=staged.memory.u8(::bmh::layout::LeaguePresentationOptions+uint32_t(i));
            const bool live_desk_disabled=source_runtime_byte(staged,club_modal_runtime_,exe_,::bmh::layout::LiveDeskDisabled)!=0;
            auto state=ui::settings_from_game(staged,current.request.manager_slot,staged_revision,exe_,sound_capabilities_);
            ui::SettingsScreen validate(data_directory_,state);
            LiveSettingsOwner updated=current;
            updated.request.revision=staged_revision;updated.native=save_stream(staged);
            updated.expected_buttons=buttons;updated.expected_live_desk_disabled=live_desk_disabled;
            updated.expected_live_scene_visible=engine.live_scene_visible;updated.expected_runtime=club_modal_runtime_;
            updated.rand_seed=rand_.seed();updated.game_seed=game_.seed();
            updated.rand_calls=rand_.calls();updated.game_calls=game_.calls();
            LiveSettingsDispatch result{updated.request,false,std::move(state)};
            g_=std::move(staged);revision=staged_revision;
            engine.buttons=std::move(buttons);engine.live_desk_disabled=live_desk_disabled;
            live_settings_owner_=std::move(updated);
            write_rollback.reset();
            return result;
        }
        if(const auto* file=std::get_if<LiveSettingsFile>(&action)) {
            if(file->owner_sequence!=current.request.sequence||file->request.manager_slot!=current.request.manager_slot||
               file->request.origin_item_id!=-1||file->request.revision!=revision||revision==UINT64_MAX)
                throw FormatError("live settings: file request belongs to another owner");
            GameState staged=g_;bool loaded=false;
            if(!apply_settings_file(file->request,current.request.manager_slot,-1,staged,loaded,error))
                throw FormatError(error.empty()?"live settings: .EIN request rejected":error);
            auto state=ui::settings_from_game(staged,current.request.manager_slot,revision+1,exe_,sound_capabilities_);
            ui::SettingsScreen validate(data_directory_,state);
            std::vector<uint8_t> buttons(180);
            for(size_t i=0;i<buttons.size();++i)buttons[i]=staged.memory.u8(::bmh::layout::LeaguePresentationOptions+uint32_t(i));
            const bool live_desk_disabled=source_runtime_byte(staged,club_modal_runtime_,exe_,::bmh::layout::LiveDeskDisabled)!=0;
            LiveSettingsOwner updated=current;updated.request.revision=revision+1;
            updated.native=save_stream(staged);updated.expected_buttons=buttons;
            updated.expected_live_desk_disabled=live_desk_disabled;updated.expected_live_scene_visible=engine.live_scene_visible;
            updated.expected_runtime=club_modal_runtime_;
            LiveSettingsDispatch result{updated.request,false,std::move(state)};
            g_=std::move(staged);engine.buttons=std::move(buttons);engine.live_desk_disabled=live_desk_disabled;
            live_settings_owner_=std::move(updated);++revision;
            return result;
        }
        // Close: the owner, sequence and revision must match. The live-scene flag and a few runtime
        // latches are derived from the settings switches, then the owner is dropped and the revision
        // advances.
        const auto& close=std::get<LiveSettingsCloseRequest>(action);
        if(close.owner!=current.request||close.owner.sequence!=current.request.sequence||close.owner.revision!=revision)
            throw FormatError("live settings: stale or mismatched Source close");
        if(revision==UINT64_MAX)throw FormatError("live settings: revision exhausted");
        GameState next=g_;auto runtime=club_modal_runtime_;
        std::array<uint8_t,180> raw{};
        for(size_t i=0;i<raw.size();++i)raw[i]=next.memory.u8(::bmh::layout::LeaguePresentationOptions+uint32_t(i));
        next.live_scene=uint8_t(1-(raw[13]&1))!=0; // SettingsSourceRefusal: LiveSceneVisible=1-(CareerSession_DispatchLiveTactic_RuntimeField3&1).
        source_runtime_set_byte(next,runtime,::bmh::layout::CareerSession_DispatchLiveSubstitution_ExecutableField,next.memory.u8(::bmh::layout::CareerSession_FormatError_RuntimeField2)); // Source max-substitution latch.
        uint32_t code=0;
        for(uint32_t i=0;i<6;++i)code=code*10+next.memory.u8(::bmh::layout::CareerSession_FormatError_RuntimeField + (i+14)*9);
        for(uint32_t i=0;i<4;++i)source_runtime_set_byte(next,runtime,::bmh::layout::CareerSession_FormatError_RuntimeField9+i,uint8_t(code>>(8*i)));
        source_runtime_set_byte(next,runtime,::bmh::layout::CareerSession_FormatError_ExecutableField8,1); // EndPhase requests its normal redraw on return.
        auto state=ui::settings_from_game(next,current.request.manager_slot,revision+1,exe_,sound_capabilities_);
        ui::SettingsScreen validate(data_directory_,state);
        std::vector<uint8_t> buttons(180);
        for(size_t i=0;i<buttons.size();++i)buttons[i]=next.memory.u8(::bmh::layout::LeaguePresentationOptions+uint32_t(i));
        const bool live_desk_disabled=source_runtime_byte(next,runtime,exe_,::bmh::layout::LiveDeskDisabled)!=0;
        LiveSettingsDispatch result;result.owner=current.request;result.owner.revision=revision+1;
        result.closed=true;result.state=std::move(state);
        g_=std::move(next);club_modal_runtime_=std::move(runtime);
        engine.buttons=std::move(buttons);engine.live_desk_disabled=live_desk_disabled;engine.live_scene_visible=g_.live_scene;
        live_settings_owner_.reset();++revision;return result;
    } catch(const std::exception& e) {
        if(write_rollback) {
            g_=std::move(*write_rollback);revision=revision_before_action;
            live_settings_owner_=current;
        }
        error=e.what();return {};
    }
}

// Desk settings group (menu 19) shares the cell write and file code with the live settings but
// keeps its own owner (desk_settings_owner_). Pattern: open on a staged copy -> write / file /
// close actions that must find the owner snapshot unchanged -> close.
// Captures state, runtime bytes, owner and scope counter so the UI flow can roll back.
CareerSession::DeskSettingsCheckpoint CareerSession::desk_settings_checkpoint(uint64_t revision) const {
    return {g_,club_modal_runtime_,desk_settings_owner_ ?
        std::optional<DeskSettingsOwner>(desk_settings_owner_->owner) : std::nullopt,
        desk_settings_owner_ ? desk_settings_owner_->initial_h8 : uint8_t(0),
        next_desk_settings_scope_,revision};
}

// Loads or saves the 180-byte settings block as a .ein file on a staged state (const: it never
// touches the live state). Validates owner fields, extension, symlink and size; save refuses an
// existing file without the overwrite flag. Returns false with a short error code in 'error';
// 'loaded' tells whether a load changed the staged state.
bool CareerSession::apply_settings_file(const ui::SaveLoadRequest& request,int manager_slot,int origin_item_id,
                                       GameState& staged,bool& loaded,std::string& error) const {
    constexpr size_t kBytes=180;
    loaded=false;
    if(request.file_kind!=ui::SaveLoadFileKind::settings||request.manager_slot!=manager_slot||
       request.origin_item_id!=origin_item_id||request.path.filename().empty()) {
        error="invalid_settings_file_owner";return false;
    }
    std::string extension=request.path.extension().string();
    std::transform(extension.begin(),extension.end(),extension.begin(),
        [](unsigned char c){return char(std::tolower(c));});
    if(extension!=".ein") {error="invalid_settings_file_extension";return false;}
    const uint32_t base=::bmh::layout::LeaguePresentationOptions;
    if(!staged.memory.has(base+uint32_t(kBytes-1))) {error="settings_file_memory_unavailable";return false;}
    std::error_code ec;
    const auto status=std::filesystem::symlink_status(request.path,ec);
    if(!ec&&std::filesystem::is_symlink(status)) {error="settings_file_target_is_symlink";return false;}
    if(request.action==ui::SaveLoadAction::load) {
        Bytes bytes;
        try {bytes=read_file(request.path);}catch(const std::exception&) {error="settings_file_read_failed";return false;}
        if(bytes.size()!=kBytes) {error="settings_file_wrong_size";return false;}
        for(size_t i=0;i<kBytes;++i)staged.memory.set8(base+uint32_t(i),bytes[i]);
        std::array<uint8_t,kBytes> raw{};
        for(size_t i=0;i<kBytes;++i)raw[i]=bytes[i];
        const auto availability=ui::project_settings_availability(raw,
            {staged.memory.u8(::bmh::layout::GameMode),staged.memory.u32(::bmh::layout::CompetitionSelection),0,0},exe_);
        for(size_t i=0;i<kBytes;++i)
            if(i!=5&&i!=6)staged.memory.set8(base+uint32_t(i),availability[i]);
        loaded=true;error.clear();return true;
    }
    if(request.action!=ui::SaveLoadAction::save) {error="invalid_settings_file_action";return false;}
    if(!std::filesystem::is_directory(request.path.parent_path(),ec)||ec) {
        error="settings_file_directory_unavailable";return false;
    }
    ec.clear();const bool exists=std::filesystem::exists(request.path,ec);
    if(ec||(exists&&!request.overwrite)) {
        error=exists?"settings_file_overwrite_confirmation_required":"settings_file_target_unavailable";
        return false;
    }
    Bytes bytes(kBytes);
    for(size_t i=0;i<kBytes;++i)bytes[i]=staged.memory.u8(base+uint32_t(i));
    try {
        std::ofstream file(request.path,std::ios::binary|std::ios::trunc);
        if(!file) {error="settings_file_write_failed";return false;}
        file.write(reinterpret_cast<const char*>(bytes.data()),std::streamsize(bytes.size()));file.flush();
        if(!file) {error="settings_file_write_failed";return false;}
    } catch(const std::exception&) {error="settings_file_write_failed";return false;}
    error.clear();return true;
}

// Restores a desk settings checkpoint: state, runtime bytes, owner (with a fresh snapshot),
// scope counter and revision.
void CareerSession::restore_desk_settings_checkpoint(DeskSettingsCheckpoint checkpoint,uint64_t& revision) {
    g_=std::move(checkpoint.state);club_modal_runtime_=std::move(checkpoint.runtime);
    desk_settings_owner_.reset();
    if(checkpoint.owner) {
        DeskSettingsOwnerState restored;restored.owner=*checkpoint.owner;
        restored.initial_h8=checkpoint.initial_h8;restored.native=save_stream(g_);
        restored.expected_runtime=club_modal_runtime_;restored.rand_seed=rand_.seed();
        restored.game_seed=game_.seed();restored.rand_calls=rand_.calls();restored.game_calls=game_.calls();
        desk_settings_owner_=std::move(restored);
    }
    next_desk_settings_scope_=checkpoint.next_scope;revision=checkpoint.revision;
}

// Opens the desk settings page from menu item 19: the request must name an enabled menu-19 item
// of the current revision for the active manager and no other owner, dispatch or calendar advance
// may be active. Works on a staged copy that is committed with the owner on success; empty
// optional with 'error' otherwise.
std::optional<CareerSession::DeskSettingsProjection> CareerSession::open_desk_settings(
    const ui::DeskRequest& request,uint64_t& revision,std::string& error) {
    error.clear();
    if(request.kind!=ui::DeskRequestKind::open_menu||request.menu_kind!=19||request.value!=-1||
       request.item_id<0||request.item_id>=54||request.revision!=revision||revision==UINT64_MAX||
       next_desk_settings_scope_==UINT64_MAX||request.manager_slot<0||request.manager_slot>3||
       !g_.memory.has(::bmh::layout::ActiveManager)||g_.memory.u8(::bmh::layout::ActiveManager)!=request.manager_slot||
       size_t(request.manager_slot)>=g_.manager_teams.size()||
       live_settings_owner_||desk_settings_owner_||live_overview_owner_||live_intervention_||
       balance_draft_||balance_dispatch_active_||strengths_owner_||strengths_dispatch_active_||
       league_family_owner_||league_family_teaminfo_active_||league_pairings_draft_||
       league_pairings_dispatch_active_||other_games_owner_||other_games_dispatch_active_||
       top_scorers_owner_||top_scorers_dispatch_active_||opportunities_owner_||opportunities_dispatch_active_||
       pending_offer_dispatch_active_||market_dispatch_active_||transfer_offer_editor_||manager_join_||
       contract_quote_||advertising_renewal_dispatch_active_||stadium_.investment||stadium_.tip||
       stadium_.periphery_quote||stadium_.block_expansion||
       calendar_advance_active_||career_complete()) {
        error="desk settings: stale, nested or unauthenticated menu-19 request";return {};
    }
    try {
        const auto desk=desktop_projection(revision);
        const auto source=std::find_if(desk.items.begin(),desk.items.end(),[&](const ui::DeskItem& item) {
            return item.id==request.item_id;
        });
        if(source==desk.items.end()||source->kind!=19||source->disabled||desk.manager_slot!=request.manager_slot) {
            error="desk settings: menu-19 source item is not currently enabled";return {};
        }
        if(!g_.memory.has(::bmh::layout::LeaguePresentationOptions+179)||!g_.memory.has(::bmh::layout::GameMode)||!g_.memory.has(::bmh::layout::CompetitionSelection)) {
            error="desk settings: Source settings block is incomplete";return {};
        }
        GameState staged=g_;auto runtime=club_modal_runtime_;
        std::array<uint8_t,180> raw{};
        for(size_t i=0;i<raw.size();++i) {
            const uint32_t address=::bmh::layout::LeaguePresentationOptions+uint32_t(i);
            if(!staged.memory.has(address))throw FormatError("desk settings: Source settings block is incomplete");
            raw[i]=staged.memory.u8(address);
        }
        const auto projected=ui::project_settings_availability(raw,
            {staged.memory.u8(::bmh::layout::GameMode),staged.memory.u32(::bmh::layout::CompetitionSelection),0,0},exe_);
        for(size_t i=0;i<projected.size();++i)
            staged.memory.set8(::bmh::layout::LeaguePresentationOptions+uint32_t(i),(i==5||i==6)?raw[i]:projected[i]);
        auto state=ui::settings_from_game(staged,request.manager_slot,revision+1,exe_,sound_capabilities_);
        ui::SettingsScreen validate(data_directory_,state);
        DeskSettingsOwner owner{next_desk_settings_scope_,revision+1,request.manager_slot,request.item_id};
        DeskSettingsOwnerState next;next.owner=owner;next.initial_h8=raw[7*9+7];next.native=save_stream(staged);
        next.expected_runtime=runtime;next.rand_seed=rand_.seed();next.game_seed=game_.seed();
        next.rand_calls=rand_.calls();next.game_calls=game_.calls();
        DeskSettingsProjection result{owner,std::move(state)};
        g_=std::move(staged);club_modal_runtime_=std::move(runtime);
        desk_settings_owner_=std::move(next);++next_desk_settings_scope_;++revision;
        return result;
    } catch(const std::exception& e) {error=e.what();return {};}
}

// Applies a cell write, a settings file request or the close of the desk settings page. The
// owner snapshot, random counters and active manager must be unchanged. A failed write is rolled
// back and the revision restored.
std::optional<CareerSession::DeskSettingsDispatch> CareerSession::dispatch_desk_settings(
    const DeskSettingsAction& action,uint64_t& revision,std::string& error) {
    error.clear();
    if(!desk_settings_owner_||revision==UINT64_MAX||desk_settings_owner_->owner.revision!=revision||
       save_stream(g_)!=desk_settings_owner_->native||club_modal_runtime_!=desk_settings_owner_->expected_runtime||
       rand_.seed()!=desk_settings_owner_->rand_seed||rand_.calls()!=desk_settings_owner_->rand_calls||
       game_.seed()!=desk_settings_owner_->game_seed||game_.calls()!=desk_settings_owner_->game_calls||
       !g_.memory.has(::bmh::layout::ActiveManager)||g_.memory.u8(::bmh::layout::ActiveManager)!=desk_settings_owner_->owner.manager_slot) {
        error="desk settings: stale owner or changed session";return {};
    }
    const auto current=*desk_settings_owner_;
    const uint64_t revision_before=revision;
    std::optional<GameState> rollback;
    try {
        if(const auto* write=std::get_if<DeskSettingsWrite>(&action)) {
            if(write->owner_sequence!=current.owner.sequence||write->write.manager_slot!=current.owner.manager_slot||
               write->write.revision!=revision)throw FormatError("desk settings: write belongs to another owner");
            rollback=g_;desk_settings_dispatch_active_=true;
            struct Reset { bool& active;~Reset(){active=false;} } reset{desk_settings_dispatch_active_};
            uint64_t staged_revision=revision;
            if(!dispatch_settings(write->write,staged_revision,error))
                throw FormatError(error.empty()?"desk settings: Source write rejected":error);
            GameState staged=std::move(g_);g_=*rollback;
            std::array<uint8_t,180> raw{};
            for(size_t i=0;i<raw.size();++i)raw[i]=staged.memory.u8(::bmh::layout::LeaguePresentationOptions+uint32_t(i));
            const auto projected=ui::project_settings_availability(raw,
                {staged.memory.u8(::bmh::layout::GameMode),staged.memory.u32(::bmh::layout::CompetitionSelection),0,0},exe_);
            for(size_t i=0;i<projected.size();++i)
                if(i!=5&&i!=6)staged.memory.set8(::bmh::layout::LeaguePresentationOptions+uint32_t(i),projected[i]);
            auto state=ui::settings_from_game(staged,current.owner.manager_slot,staged_revision,exe_,sound_capabilities_);
            ui::SettingsScreen validate(data_directory_,state);
            DeskSettingsOwnerState updated=current;updated.owner.revision=staged_revision;
            updated.native=save_stream(staged);updated.expected_runtime=club_modal_runtime_;
            updated.rand_seed=rand_.seed();updated.game_seed=game_.seed();
            updated.rand_calls=rand_.calls();updated.game_calls=game_.calls();
            DeskSettingsDispatch result{updated.owner,false,std::move(state)};
            g_=std::move(staged);revision=staged_revision;desk_settings_owner_=std::move(updated);
            rollback.reset();return result;
        }
        if(const auto* file=std::get_if<DeskSettingsFile>(&action)) {
            if(file->owner_sequence!=current.owner.sequence||file->request.manager_slot!=current.owner.manager_slot||
               file->request.revision!=revision||file->request.origin_item_id!=current.owner.source_item_id||
               revision==UINT64_MAX)
                throw FormatError("desk settings: file request belongs to another owner");
            GameState staged=g_;bool loaded=false;
            if(!apply_settings_file(file->request,current.owner.manager_slot,current.owner.source_item_id,
                                    staged,loaded,error))
                throw FormatError(error.empty()?"desk settings: .EIN request rejected":error);
            auto state=ui::settings_from_game(staged,current.owner.manager_slot,revision+1,exe_,sound_capabilities_);
            ui::SettingsScreen validate(data_directory_,state);
            DeskSettingsOwnerState updated=current;updated.owner.revision=revision+1;
            updated.native=save_stream(staged);updated.expected_runtime=club_modal_runtime_;
            updated.rand_seed=rand_.seed();updated.game_seed=game_.seed();
            updated.rand_calls=rand_.calls();updated.game_calls=game_.calls();
            DeskSettingsDispatch result{updated.owner,false,std::move(state)};
            g_=std::move(staged);desk_settings_owner_=std::move(updated);++revision;
            return result;
        }
        // Close: validates owner and revision. When the stepper at row 7 column 7 changed, the 32-bit
        // runtime counter after it is reset (new value 0) or shifted by 60 per step; the live-scene
        // flag and other latches are derived from the switches and the owner is dropped.
        const auto& close=std::get<DeskSettingsClose>(action);
        if(close.owner!=current.owner||close.owner.revision!=revision)
            throw FormatError("desk settings: stale close request");
        GameState next=g_;auto runtime=club_modal_runtime_;
        std::array<uint8_t,180> raw{};
        for(size_t i=0;i<raw.size();++i)raw[i]=next.memory.u8(::bmh::layout::LeaguePresentationOptions+uint32_t(i));
        const uint8_t new_h8=raw[7*9+7];
        if(new_h8==0) {
            source_runtime_set_byte(next,runtime,::bmh::layout::CareerSession_FormatError_ExecutableField2,1);
            source_runtime_set_byte(next,runtime,::bmh::layout::CareerSession_FormatError_ExecutableField2+1,0);
            source_runtime_set_byte(next,runtime,::bmh::layout::CareerSession_FormatError_ExecutableField2+2,0);
            source_runtime_set_byte(next,runtime,::bmh::layout::CareerSession_FormatError_ExecutableField2+3,0);
        } else if(current.initial_h8!=0) {
            const int16_t delta=static_cast<int16_t>(int(new_h8)-int(current.initial_h8));
            const uint32_t remaining=source_runtime_u32(next,runtime,exe_,::bmh::layout::CareerSession_FormatError_ExecutableField2);
            const uint32_t adjusted=remaining+uint32_t(int32_t(delta)*60);
            for(uint32_t i=0;i<4;++i)
                source_runtime_set_byte(next,runtime,::bmh::layout::CareerSession_FormatError_ExecutableField2+i,uint8_t(adjusted>>(8*i)));
        }
        next.live_scene=uint8_t(1-(raw[13]&1))!=0;
        source_runtime_set_byte(next,runtime,::bmh::layout::CareerSession_DispatchLiveSubstitution_ExecutableField,next.memory.u8(::bmh::layout::CareerSession_FormatError_RuntimeField2));
        uint32_t code=0;
        for(uint32_t i=0;i<6;++i)code=code*10+next.memory.u8(::bmh::layout::CareerSession_FormatError_RuntimeField + (i+14)*9);
        for(uint32_t i=0;i<4;++i)source_runtime_set_byte(next,runtime,::bmh::layout::CareerSession_FormatError_RuntimeField9+i,uint8_t(code>>(8*i)));
        auto state=ui::settings_from_game(next,current.owner.manager_slot,revision+1,exe_,sound_capabilities_);
        ui::SettingsScreen validate(data_directory_,state);
        DeskSettingsDispatch result{current.owner,true,std::move(state)};result.owner.revision=revision+1;
        g_=std::move(next);club_modal_runtime_=std::move(runtime);desk_settings_owner_.reset();++revision;
        return result;
    } catch(const std::exception& e) {
        if(rollback) {g_=std::move(*rollback);revision=revision_before;desk_settings_owner_=current;}
        error=e.what();return {};
    }
}

// Advertising group: renewal check/answer, offers and spending. Pattern: the UI sends an
// AdvertisingRequest with request id (must increase), revision, manager slot and club; the
// session validates against the active manager, applies the change on the live state inside a
// checkpoint (apply_advertising rolls everything back if validation, the action or the
// acknowledgement fails) and bumps the revision.
// Simple renewal check for the active manager; the revision is bumped unless the contract has
// not expired. Returns nothing with a German 'error' text if stale or invalid.
std::optional<AdvertisingRenewal> CareerSession::check_advertising_renewal(int manager_slot, int display_slot,
                                                                         uint64_t& revision, std::string& error) {
    error.clear();
    if (advertising_renewal_dispatch_active_) {
        error = "advertising_renewal_active";
        return std::nullopt;
    }
    if (!g_.memory.has(::bmh::layout::ActiveManager) || manager_slot < 0 || size_t(manager_slot) >= g_.manager_teams.size() ||
        int(g_.memory.u8(::bmh::layout::ActiveManager)) != manager_slot || revision == std::numeric_limits<uint64_t>::max()) {
        error = "Diese Werbeaktion ist nicht mehr aktuell.";
        return std::nullopt;
    }
    AdvertisingRenewal renewal = bmh::check_advertising_renewal(g_, manager_slot, display_slot, rand_);
    if (renewal.status == AdvertisingRenewal::Status::invalid) {
        error = "Diese Werbefläche gibt es nicht.";
        return std::nullopt;
    }
    if (renewal.status != AdvertisingRenewal::Status::not_expired) ++revision;
    return renewal;
}

// Answers an offered renewal (renew or decline) for the active manager and bumps the revision;
// false with a German 'error' text if the request is stale.
bool CareerSession::answer_advertising_renewal(int manager_slot, int contract_slot, bool renew, uint64_t& revision,
                                               std::string& error) {
    error.clear();
    if (advertising_renewal_dispatch_active_) {
        error = "advertising_renewal_active";
        return false;
    }
    if (!g_.memory.has(::bmh::layout::ActiveManager) || manager_slot < 0 || size_t(manager_slot) >= g_.manager_teams.size() ||
        int(g_.memory.u8(::bmh::layout::ActiveManager)) != manager_slot || revision == std::numeric_limits<uint64_t>::max() ||
        !bmh::answer_advertising_renewal(g_, manager_slot, contract_slot, renew)) {
        error = "Diese Vertragsverlängerung ist nicht mehr aktuell.";
        return false;
    }
    ++revision;
    return true;
}

// Copies nearly the whole session (state, random generators, turn driver, every modal owner,
// dispatch flags, scope counters, runtime bytes and the given revision) for rollback and for the
// "nothing changed" comparison below. Also used outside advertising, e.g. around the kickoff.
CareerSession::AdvertisingSessionCheckpoint CareerSession::advertising_session_checkpoint(uint64_t revision) const {
    AdvertisingSessionCheckpoint c;
    c.state=g_; c.game=game_; c.rand=rand_; c.driver=turn_driver_;
    c.desk_callargs=source_desk_callargs_; c.saved_turn_error=saved_turn_error_;
    c.completed_results=completed_results_; c.lineup_manager=lineup_entry_manager_;
    c.lineup_club=lineup_entry_club_; c.managers=managers_; c.stock_tip=last_stock_tip_;
    c.property_notice=last_property_notice_; c.credit_notice=last_credit_notice_;
    c.stadium=stadium_; c.next_stadium_scope=next_stadium_scope_;
    c.contract_quote=contract_quote_; c.next_contract_quote_id=next_contract_quote_id_;
    c.last_contract_request_id=last_contract_request_id_; c.options_applied=options_applied_;
    c.pending_offer_active=pending_offer_dispatch_active_; c.market_active=market_dispatch_active_;
    c.pairings_active=league_pairings_dispatch_active_; c.family_teaminfo_active=league_family_teaminfo_active_;
    c.strengths_active=strengths_dispatch_active_; c.top_scorers_active=top_scorers_dispatch_active_;
    c.other_games_active=other_games_dispatch_active_; c.balance_active=balance_dispatch_active_;
    c.calendar_active=calendar_advance_active_; c.next_other_games_scope=next_other_games_scope_;
    c.other_games_owner=other_games_owner_; c.other_games_selection=other_games_selection_;
    c.next_balance_scope=next_balance_scope_; c.balance=balance_draft_;
    c.next_strengths_scope=next_strengths_scope_; c.strengths=strengths_owner_;
    c.family=league_family_owner_; c.next_family_id=next_league_family_id_;
    c.next_top_scorers_scope=next_top_scorers_scope_; c.top_scorers=top_scorers_owner_;
    c.pairings_scope=league_pairings_scope_; c.pairings=league_pairings_draft_;
    c.transfer_editor=transfer_offer_editor_; c.poach=poach_;
    c.transfer_offer_revision=transfer_offer_revision_; c.pending_hooks=pending_offer_hooks_;
    c.pending_policy=pending_offer_entry_policy_; c.runtime=club_modal_runtime_;
    c.last_request_id=last_advertising_request_id_; c.manager_join=manager_join_;
    c.revision=revision; c.session_id=session_id_;
    return c;
}

// Puts back everything advertising_session_checkpoint captured (the caller restores the
// revision itself).
void CareerSession::restore_advertising_session_checkpoint(AdvertisingSessionCheckpoint c) {
    g_=std::move(c.state); game_=c.game; rand_=c.rand; turn_driver_=std::move(c.driver);
    source_desk_callargs_=std::move(c.desk_callargs); saved_turn_error_=std::move(c.saved_turn_error);
    completed_results_=std::move(c.completed_results); lineup_entry_manager_=c.lineup_manager;
    lineup_entry_club_=c.lineup_club; managers_=std::move(c.managers); last_stock_tip_=c.stock_tip;
    last_property_notice_=std::move(c.property_notice); last_credit_notice_=std::move(c.credit_notice);
    stadium_=std::move(c.stadium); next_stadium_scope_=c.next_stadium_scope;
    contract_quote_=std::move(c.contract_quote); next_contract_quote_id_=c.next_contract_quote_id;
    last_contract_request_id_=c.last_contract_request_id; options_applied_=c.options_applied;
    pending_offer_dispatch_active_=c.pending_offer_active; market_dispatch_active_=c.market_active;
    league_pairings_dispatch_active_=c.pairings_active; league_family_teaminfo_active_=c.family_teaminfo_active;
    strengths_dispatch_active_=c.strengths_active; top_scorers_dispatch_active_=c.top_scorers_active;
    other_games_dispatch_active_=c.other_games_active; balance_dispatch_active_=c.balance_active;
    calendar_advance_active_=c.calendar_active; next_other_games_scope_=c.next_other_games_scope;
    other_games_owner_=std::move(c.other_games_owner); other_games_selection_=c.other_games_selection;
    next_balance_scope_=c.next_balance_scope; balance_draft_=std::move(c.balance);
    next_strengths_scope_=c.next_strengths_scope; strengths_owner_=std::move(c.strengths);
    league_family_owner_=std::move(c.family); next_league_family_id_=c.next_family_id;
    next_top_scorers_scope_=c.next_top_scorers_scope; top_scorers_owner_=std::move(c.top_scorers);
    league_pairings_scope_=c.pairings_scope; league_pairings_draft_=std::move(c.pairings);
    transfer_offer_editor_=std::move(c.transfer_editor); poach_=std::move(c.poach);
    transfer_offer_revision_=c.transfer_offer_revision; pending_offer_hooks_=std::move(c.pending_hooks);
    pending_offer_entry_policy_=c.pending_policy; club_modal_runtime_=std::move(c.runtime);
    last_advertising_request_id_=c.last_request_id; manager_join_=std::move(c.manager_join);
}

// True when the live session equals the checkpoint in saved state, random counters, turn driver
// stage, owners, flags, scope counters and revision; used to detect a callback that changed the
// session behind the engine's back.
bool CareerSession::advertising_session_unchanged(const AdvertisingSessionCheckpoint& c,
                                                  uint64_t revision) const {
    const auto byte_same=[&](uint32_t at) {
        const bool live_has=g_.memory.has(at), saved_has=c.state.memory.has(at);
        return live_has==saved_has && (!live_has || g_.memory.u8(at)==c.state.memory.u8(at));
    };
    const bool manager_ids_same = managers_.size()==c.managers.size() &&
        std::equal(managers_.begin(),managers_.end(),c.managers.begin(),[](const auto& a,const auto& b) {
            return a.slot==b.slot && a.master_club==b.master_club && a.runtime_club==b.runtime_club;
        });
    return save_stream(g_)==save_stream(c.state) && same_strengths_runtime_state(g_,c.state) &&
        g_.manager_teams==c.state.manager_teams && byte_same(::bmh::layout::CareerSession_AdvanceLeagueFamily_ExecutableField) && byte_same(::bmh::layout::CareerSession_AdvertisingSessionUnchanged_ExecutableField) &&
        rand_.seed()==c.rand.seed() && rand_.calls()==c.rand.calls() &&
        game_.seed()==c.game.seed() && game_.calls()==c.game.calls() &&
        turn_driver_.stage==c.driver.stage && source_desk_callargs_==c.desk_callargs &&
        lineup_entry_manager_==c.lineup_manager && lineup_entry_club_==c.lineup_club && manager_ids_same &&
        session_id_==c.session_id && last_advertising_request_id_==c.last_request_id && revision==c.revision &&
        club_modal_runtime_==c.runtime &&
        pending_offer_dispatch_active_==c.pending_offer_active && market_dispatch_active_==c.market_active &&
        league_pairings_dispatch_active_==c.pairings_active && league_family_teaminfo_active_==c.family_teaminfo_active &&
        strengths_dispatch_active_==c.strengths_active && top_scorers_dispatch_active_==c.top_scorers_active &&
        other_games_dispatch_active_==c.other_games_active && balance_dispatch_active_==c.balance_active &&
        calendar_advance_active_==c.calendar_active &&
        bool(other_games_owner_)==bool(c.other_games_owner) && bool(balance_draft_)==bool(c.balance) &&
        bool(strengths_owner_)==bool(c.strengths) && bool(league_family_owner_)==bool(c.family) &&
        bool(top_scorers_owner_)==bool(c.top_scorers) && bool(league_pairings_draft_)==bool(c.pairings) &&
        bool(transfer_offer_editor_)==bool(c.transfer_editor) && bool(poach_)==bool(c.poach) &&
        bool(manager_join_)==bool(c.manager_join) && bool(stadium_.opened)==bool(c.stadium.opened) &&
        bool(contract_quote_)==bool(c.contract_quote) &&
        next_other_games_scope_==c.next_other_games_scope && next_balance_scope_==c.next_balance_scope &&
        next_strengths_scope_==c.next_strengths_scope && next_league_family_id_==c.next_family_id &&
        next_top_scorers_scope_==c.next_top_scorers_scope && league_pairings_scope_==c.pairings_scope &&
        next_stadium_scope_==c.next_stadium_scope && next_contract_quote_id_==c.next_contract_quote_id &&
        last_contract_request_id_==c.last_contract_request_id && transfer_offer_revision_==c.transfer_offer_revision;
}

// Renewal question with a UI presenter. Refuses a competing owner or dispatch (competing_owner)
// and a stale request (increasing request id, active manager, club, revision, contract bytes).
// The presenter runs on a staged copy; the session is always restored after it, and only the
// staged result of the confirmed answer becomes live (revision plus one). Returns nothing with a
// German 'error' text on failure.
std::optional<AdvertisingRenewal> CareerSession::dispatch_advertising_renewal(
    const ui::AdvertisingRequest& request, uint64_t& revision, std::string& error,
    const AdvertisingRenewalPresenter& presenter) {
    error.clear();
    const auto competing_owner = [&] {
        return pending_offer_dispatch_active_ || market_dispatch_active_ || league_pairings_dispatch_active_ ||
            league_family_teaminfo_active_ || strengths_dispatch_active_ || top_scorers_dispatch_active_ ||
            other_games_dispatch_active_ || balance_dispatch_active_ || calendar_advance_active_ ||
            balance_draft_.has_value() || strengths_owner_.has_value() || top_scorers_owner_.has_value() ||
            other_games_owner_.has_value() || league_family_owner_.has_value() || league_pairings_draft_.has_value() ||
            manager_join_.has_value() || transfer_offer_editor_.has_value() || poach_.has_value() ||
            stadium_.opened || stadium_.investment.has_value() || stadium_.tip.has_value() ||
            stadium_.periphery_quote.has_value() || stadium_.selection_status.has_value() ||
            stadium_.block_expansion.has_value() || contract_quote_.has_value() ||
            (turn_driver_.stage!=TurnDriver::Stage::MatchdayDesks &&
             turn_driver_.stage!=TurnDriver::Stage::SeasonEndDesk);
    };
    if (advertising_renewal_dispatch_active_ || competing_owner()) {
        error = "Eine andere Session-Aktion oder ein konkurrierender Screen ist aktiv.";
        return std::nullopt;
    }
    if(!presenter||!validate_advertising_renewal_request_source(g_,request,revision,last_advertising_request_id_,error)) {
        if(error.empty())error="Die Original-Verlängerungsfrage ist nicht verfügbar.";
        return std::nullopt;
    }

    const auto checkpoint = advertising_session_checkpoint(revision);
    const auto restore = [&] {
        restore_advertising_session_checkpoint(checkpoint);
        revision = checkpoint.revision;
    };
    advertising_renewal_dispatch_active_ = true;
    struct ActiveReset { bool& active; ~ActiveReset() { active = false; } } active_reset{advertising_renewal_dispatch_active_};
    try {
        GameState staged = g_;
        WatcomRandom staged_rand = rand_;
        auto renewal = bmh::check_advertising_renewal(staged, request.manager_slot,
                                                       request.renewal_display_slot, staged_rand);
        if (renewal.status == AdvertisingRenewal::Status::invalid ||
            renewal.contract_slot != request.renewal_contract_slot ||
            renewal.status == AdvertisingRenewal::Status::not_expired) {
            restore(); error = "Diese Vertragsverlängerung ist nicht mehr aktuell."; return std::nullopt;
        }
        const auto presentation = presenter(staged, renewal);
        if (!advertising_session_unchanged(checkpoint, revision)) {
            restore(); error = "Die Session wurde während des Werbedialogs verändert."; return std::nullopt;
        }
        // Always restore the snapshot after presentation. This makes the rollback
        // unconditional, including hidden owners, IDs and runtime maps; only the
        // staged Source result below is allowed to become live.
        restore();
        if (renewal.status == AdvertisingRenewal::Status::offered) {
            if (presentation != AdvertisingRenewalPresentation::renew &&
                presentation != AdvertisingRenewalPresentation::decline) {
                error = "Die Vertragsverlängerung wurde nicht bestätigt."; return std::nullopt;
            }
            const bool renew = presentation == AdvertisingRenewalPresentation::renew;
            if (!bmh::answer_advertising_renewal(staged, request.manager_slot,
                                                  request.renewal_contract_slot, renew)) {
                error = "Diese Vertragsverlängerung ist nicht mehr aktuell."; return std::nullopt;
            }
        } else if (renewal.status == AdvertisingRenewal::Status::declined_by_sponsor) {
            if (presentation != AdvertisingRenewalPresentation::acknowledged) {
                error = "Die Sponsorabsage wurde nicht bestätigt."; return std::nullopt;
            }
        } else {
            error = "Unbekannter Source-Verlängerungsstatus."; return std::nullopt;
        }
        g_ = std::move(staged);
        rand_ = staged_rand;
        last_advertising_request_id_ = request.request_id;
        ++revision;
        error.clear();
        return renewal;
    } catch (const std::exception&) {
        restore(); error = "Die Vertragsverlängerung konnte nicht bestätigt werden."; return std::nullopt;
    } catch (...) {
        restore(); error = "Die Vertragsverlängerung konnte nicht bestätigt werden."; return std::nullopt;
    }
}

// Entry point for advertising requests: runs the renewal or the offer/spending dispatch and then
// the acknowledgement callback; any failure or exception restores the advertising checkpoint and
// the old revision (exceptions are rethrown).
bool CareerSession::apply_advertising(const ui::AdvertisingRequest& request, uint64_t& revision,
    std::string& error, const AdvertisingAcknowledge& acknowledge,
    const AdvertisingRenewalPresenter& presenter) {
    auto before = advertising_checkpoint();
    const auto old_revision = revision;
    const auto restore = [&] {
        restore_advertising_checkpoint(std::move(before));
        revision = old_revision;
    };
    try {
        AdvertisingActionResult result;
        if (request.kind == ui::AdvertisingActionKind::check_renewal) {
            if (!presenter) {
                error = "Die Original-Verlängerungsfrage ist nicht verfügbar.";
                restore(); return false;
            }
            result.renewal = dispatch_advertising_renewal(request, revision, error, presenter);
            if (!result.renewal) { restore(); return false; }
        } else if (!dispatch_advertising(request, revision, error, &result.spending)) {
            restore(); return false;
        }
        if (acknowledge) acknowledge(result);
        return true;
    } catch (...) {
        restore();
        throw;
    }
}

// Builds an accept_offer request from the page the player saw and applies it; false with 'error'
// if the displayed offer no longer exists.
bool CareerSession::accept_advertising_selection(const ui::AdvertisingState& observed,
    int display_slot, int raw_offer, uint64_t request_id, uint64_t& revision, std::string& error) {
    if (display_slot < 0 || display_slot >= int(observed.offers.size())) {
        error = "Dieses Werbeangebot kann nicht übernommen werden."; return false;
    }
    const auto& view = observed.offers[size_t(display_slot)];
    const auto candidate = std::find_if(view.offers.begin(), view.offers.end(),
        [&](const auto& offer) { return offer.raw_slot == raw_offer; });
    if (candidate == view.offers.end()) {
        error = "Dieses Werbeangebot kann nicht übernommen werden."; return false;
    }
    ui::AdvertisingRequest request;
    request.request_id = request_id;
    request.manager_slot = observed.manager_slot;
    request.runtime_club = observed.runtime_team_id;
    request.revision = observed.revision;
    request.kind = ui::AdvertisingActionKind::accept_offer;
    request.offer.manager = observed.manager_slot;
    request.offer.display_slot = display_slot;
    request.offer.contract_slot = view.contract_slot;
    request.offer.expected_contract = view.contract_bytes;
    request.offer.raw_slot = candidate->raw_slot;
    request.offer.expected_offer = candidate->bytes;
    return apply_advertising(request, revision, error);
}

// Builds a set_spending request for budget slots 16-18 from the observed page and applies it;
// the observed amount and delivery weeks are sent as the expected values.
bool CareerSession::set_advertising_budget(const ui::AdvertisingState& observed, int target_slot,
    uint32_t amount, uint64_t request_id, uint64_t& revision, std::string& error,
    AdvertisingSpendingResult& result) {
    if (target_slot < 16 || target_slot > 18) {
        error = "Dieser Werbeausgabenauftrag ist nicht mehr gültig."; return false;
    }
    const auto& view = observed.spending[size_t(target_slot - 16)];
    ui::AdvertisingRequest request;
    request.request_id = request_id;
    request.manager_slot = observed.manager_slot;
    request.runtime_club = observed.runtime_team_id;
    request.revision = observed.revision;
    request.kind = ui::AdvertisingActionKind::set_spending;
    request.spending.manager = observed.manager_slot;
    request.spending.target_slot = target_slot;
    request.spending.amount = amount;
    request.spending.expected_amount = view.current_amount;
    request.spending.expected_delivery_weeks = view.delivery_weeks;
    return apply_advertising(request, revision, error,
        [&](const AdvertisingActionResult& applied) { result = applied.spending; });
}

// Validates an offer or spending request (no modal owner open, active manager and club, current
// revision, a request id greater than the last one) and applies it to the live state. The
// renewal kind is refused here; it goes through dispatch_advertising_renewal. False with a
// German 'error' text otherwise.
bool CareerSession::dispatch_advertising(const ui::AdvertisingRequest& request, uint64_t& revision,
                                          std::string& error, AdvertisingSpendingResult* spending_result) {
    if (advertising_renewal_dispatch_active_) { error = "advertising_renewal_active"; return false; }
    if (balance_draft_ || balance_dispatch_active_ || strengths_owner_ || strengths_dispatch_active_ ||
        top_scorers_owner_ || top_scorers_dispatch_active_ || stadium_.block_expansion || other_games_owner_ || other_games_dispatch_active_) { error = "strengths_active"; return false; }
    return apply_advertising_request_source(g_,rand_,request,revision,last_advertising_request_id_,error,spending_result);
}

// Contract group (negotiation at the desk). Flow: request_demands creates a quote (contract_quote_)
// that binds the follow-up requests (offer, bonuses, insurance) by quote id, row and player;
// each follow-up must carry a higher request id and the quote's revision. State changes happen on
// staged copies and are committed together with the revision bump.
// Validates request id (must increase), revision, active manager and club (not the national team)
// and the squad row against the player id; rejects with a German 'error' text and returns false.
// On success it fills 'result' (quote id, demands, accepted flag and original feedback text).
bool CareerSession::dispatch_contract(const ui::ContractRequest& request, uint64_t& revision,
                                      ui::ContractActionResult& result, std::string& error) {
    if (balance_draft_ || balance_dispatch_active_ || strengths_owner_ || strengths_dispatch_active_ ||
        top_scorers_owner_ || top_scorers_dispatch_active_ || stadium_.block_expansion || other_games_owner_ || other_games_dispatch_active_) { error = "strengths_active"; return false; }
    return apply_contract_request_source(g_,texts_,rand_,request,revision,contract_quote_,
        next_contract_quote_id_,last_contract_request_id_,result,error,
        [](const GameState& a,const GameState& b){return save_stream(a)==save_stream(b);});
}

// Common check for desk setting changes (ticket price, automation): no modal owner open, career
// not complete, optionally the desk phase (no calendar advance or pending dispatch), and the
// identity must match the active manager, revision, club and stable club identity. Sets a short
// error code and returns false otherwise.
bool CareerSession::validate_desk_setting_owner(const DeskSettingIdentity& identity, uint64_t revision,
                                                 std::string& error, bool require_desk_phase) const {
    if (balance_draft_ || balance_dispatch_active_ || strengths_owner_ || strengths_dispatch_active_ ||
        top_scorers_owner_ || top_scorers_dispatch_active_ || stadium_.block_expansion || other_games_owner_ || other_games_dispatch_active_) { error = "strengths_active"; return false; }
    if (balance_draft_ || balance_dispatch_active_ || league_pairings_draft_ || league_pairings_dispatch_active_) {
        error = "league_pairings_active"; return false;
    }
    if (career_complete()) { error = "career_complete"; return false; }
    if (require_desk_phase && ((turn_driver_.stage != TurnDriver::Stage::MatchdayDesks &&
         turn_driver_.stage != TurnDriver::Stage::SeasonEndDesk) ||
        pending_offer_dispatch_active_ || market_dispatch_active_ || calendar_advance_active_ ||
        transfer_offer_editor_ || manager_join_ || contract_quote_ || stadium_.investment ||
        stadium_.tip || stadium_.periphery_quote)) {
        error = "desk_setting_unavailable"; return false;
    }
    const int active = g_.memory.has(::bmh::layout::ActiveManager) ? int(g_.memory.u8(::bmh::layout::ActiveManager)) : -1;
    if (identity.manager_slot != active || active < 0 || active > 3 ||
        size_t(active) >= g_.manager_teams.size() || identity.revision != revision || revision == UINT64_MAX) {
        error = "stale_manager_or_revision"; return false;
    }
    const int club = g_.manager_teams[size_t(active)];
    const size_t rec = size_t(active) * 0xb0c;
    if (identity.runtime_club != club || club < 0 || club > kNoTeam || size_t(club) >= g_.club_ids.size() ||
        identity.stable_club_identity != std::optional<int>(int(g_.club_ids[size_t(club)])) ||
        rec + 0xb0c > g_.manager_records.size() || size_t(active + 1) * 5 > g_.manager_options.size() ||
        (int(g_.manager_records[rec + 0x290]) | int(g_.manager_records[rec + 0x291]) << 8) != club) {
        error = "invalid_desk_setting_owner"; return false;
    }
    return true;
}

// Writes one desk setting for 'manager': the ticket price (admission 8-25, not for the national
// team) or automation option 'index' (0 = saved formation 0-8 limited to the saved formation
// mask, 1-3 flags, 4 training automation not for the national team). Sets 'error' on a rejected
// value; does not bump the revision (the caller does).
bool CareerSession::apply_desk_setting(int manager, bool price, int index, int value, std::string& error) {
    return apply_desk_setting_source(g_,manager,price,index,value,error);
}

// The stored search profile of a manager slot (all zero for an invalid slot).
SearchProfile CareerSession::search_profile(int manager) const { return bmh::search_profile(g_, manager); }

// The profile "Vorschlag" proposes for the manager's squad; nothing is stored.
SearchProfile CareerSession::suggest_search_profile(int manager, bool whole_squad) const {
    return bmh::suggest_search_profile(g_, manager, whole_squad);
}

// Stores an edited search profile like the closing dialog does and, with `apply`, returns the transfer-list entries
// that fit it. Only the active manager of a club with a transfer market may do this, and not while another window
// owns the session. The revision rises only when the stored bytes changed.
std::optional<CareerSession::SearchProfileDispatch> CareerSession::dispatch_search_profile(int manager,
    const SearchProfile& profile, bool cancelled, bool apply, uint64_t& revision, std::string& error) {
    if (balance_draft_ || balance_dispatch_active_ || strengths_owner_ || strengths_dispatch_active_ ||
        top_scorers_owner_ || top_scorers_dispatch_active_ || stadium_.block_expansion || other_games_owner_ ||
        other_games_dispatch_active_ || transfer_offer_editor_ || market_dispatch_active_ ||
        pending_offer_dispatch_active_ || calendar_advance_active_) { error = "search_profile_blocked"; return std::nullopt; }
    if (manager < 0 || size_t(manager) >= g_.manager_teams.size() || g_.manager_teams[size_t(manager)] == kNoTeam ||
        !g_.memory.has(::bmh::layout::ActiveManager) || g_.memory.u8(::bmh::layout::ActiveManager) != manager ||
        revision == std::numeric_limits<uint64_t>::max()) { error = "invalid_search_profile_owner"; return std::nullopt; }
    if (!search_profile_valid(profile)) { error = "invalid_search_profile"; return std::nullopt; }
    const auto before = bmh::search_profile(g_, manager);
    SearchProfileDispatch result;
    result.active = set_search_profile(g_, manager, profile, cancelled);
    const auto stored = bmh::search_profile(g_, manager);
    result.profile = stored;
    if (stored != before) ++revision;
    if (apply) result.hits = search_profile_hits(g_, manager);
    error.clear();
    return result;
}

// Validates the owner and the price range (8-25) and applies the ticket price; revision plus one.
bool CareerSession::dispatch_ticket_price(const TicketPriceRequest& request, uint64_t& revision,
                                          std::string& error) {
    if (!validate_desk_setting_owner(request.identity, revision, error)) return false;
    if (request.price < 8 || request.price > 25) { error = "invalid_ticket_price"; return false; }
    if (!apply_desk_setting(request.identity.manager_slot, true, -1, request.price, error)) return false;
    ++revision;
    return true;
}

// Validates the owner and applies one automation switch (formation automation needs a formation
// 1-8 exactly when enabled); revision plus one.
bool CareerSession::dispatch_automation(const AutomationRequest& request, uint64_t& revision,
                                       std::string& error) {
    if (!validate_desk_setting_owner(request.identity, revision, error)) return false;
    const int index = int(request.automation);
    if (index < 0 || index > 4) { error = "invalid_automation_index"; return false; }
    int value = int(request.enabled);
    if (index == 0) {
        if (request.enabled != request.formation.has_value() ||
            (request.formation && (*request.formation < 1 || *request.formation > 8))) {
            error = "invalid_saved_formation_automation"; return false;
        }
        value = request.formation.value_or(0);
    } else if (request.formation) { error = "unexpected_formation_selection"; return false; }
    if (!apply_desk_setting(request.identity.manager_slot, false, index, value, error)) return false;
    ++revision;
    return true;
}

// Desk widget click (ticket price or automation option): the request must belong to a visible,
// enabled desk item of the matching kind; the new value is derived here (formation automation
// cycles to the next available formation, flags toggle) and applied; revision plus one.
bool CareerSession::dispatch_desk_widget(const ui::DeskRequest& request, uint64_t& revision,
                                         std::string& error) {
    const int active = g_.memory.has(::bmh::layout::ActiveManager) ? int(g_.memory.u8(::bmh::layout::ActiveManager)) : -1;
    DeskSettingIdentity identity{request.manager_slot, -1, std::nullopt, request.revision};
    if (active >= 0 && size_t(active) < g_.manager_teams.size()) {
        identity.runtime_club = g_.manager_teams[size_t(active)];
        if (identity.runtime_club >= 0 && size_t(identity.runtime_club) < g_.club_ids.size())
            identity.stable_club_identity = int(g_.club_ids[size_t(identity.runtime_club)]);
    }
    if (!validate_desk_setting_owner(identity, revision, error, false)) return false;
    // Refuse malformed saved formation state before projecting widget ownership.
    // The desktop renderer cannot project indices outside the stored 0..8 range.
    if (g_.manager_options[size_t(active) * 5] > 8) {
        error = "invalid_saved_formation_automation"; return false;
    }
    return apply_desk_widget_source(g_,desktop_projection(revision),request,revision,error);
}

// Desk layout editing (move an item, open or close a quick-bar drawer, send items to or
// restore them from a folder or bin). The desk is 54 item records of 16 bytes per manager. The
// request carries the layout before and after; this function checks that "before" equals the live
// bytes, recomputes the expected "after" for the operation itself, requires an exact match, and
// verifies the resulting desktop projection before committing (revision plus one). Any
// mismatch returns false with a short error code and changes nothing.
bool CareerSession::dispatch_desktop_layout(const ui::DeskRequest& request,uint64_t& revision,std::string& error) {
    if (request.kind != ui::DeskRequestKind::change_layout || request.revision != revision ||
        request.manager_slot < 0 || size_t(request.manager_slot) >= g_.manager_teams.size() ||
        !g_.memory.has(::bmh::layout::ActiveManager) || int(g_.memory.u8(::bmh::layout::ActiveManager)) != request.manager_slot ||
        turn_driver_.stage==TurnDriver::Stage::Failed || turn_driver_.stage==TurnDriver::Stage::CareerComplete ||
        career_complete()) {
        error="stale_or_invalid_desk_layout_owner"; return false;
    }
    if (transfer_offer_editor_ || pending_offer_dispatch_active_ || market_dispatch_active_ ||
        balance_draft_ || balance_dispatch_active_ || strengths_owner_ || strengths_dispatch_active_ ||
        top_scorers_owner_ || top_scorers_dispatch_active_ || other_games_owner_ || other_games_dispatch_active_ ||
        stadium_.block_expansion || advertising_renewal_dispatch_active_) {
        error="desk_layout_blocked_during_active_flow"; return false;
    }
    return apply_desktop_layout_source(g_,request,revision,error,[&](const GameState& g,uint64_t rev){return desktop_projection(g,rev);});
}

}  // namespace bmh::app
