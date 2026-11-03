// SPDX-License-Identifier: GPL-3.0-only
#include "app/world12_startup.hpp"
#include "formats/textd.hpp"
#include "game/messages.hpp"
#include "game/save.hpp"
#include "ui/world12_desktop.hpp"
#include "ui/desktop.hpp"
#include "app/world12_save_load.hpp"
#include <limits>
#include <type_traits>
namespace bmh::app::world12 {
namespace {
std::optional<OwnedFilePublication> prepare_automatic_save(Session& session,const std::filesystem::path& data,const std::filesystem::path& directory){
    const auto& state=session.snapshot();if(!state.desktop||!state.desktop->autosave_pending)return {};
    if(!state.desktop->active_seat)throw FormatError("world12: automatic save has no actual desk actor");
    const auto texts=load_textd(ui::asset_path(data,"TEXTD.BLK"));
    const std::filesystem::path name(texts[(layout::CareerSession_Name_TextPointer-layout::TextPointerTable)/4]);
    if(name.empty()||name!=name.filename()||name.extension()!=".FBM")throw FormatError("world12: invalid Original automatic save name");
    ui::SaveLoadRequest request;request.action=ui::SaveLoadAction::save;request.file_kind=ui::SaveLoadFileKind::game;request.manager_slot=*state.desktop->active_seat;request.revision=session.revision();request.path=std::filesystem::absolute(directory/name);request.overwrite=true;
    const auto path=checked_game_path(directory,request);std::optional<Bytes> expected;if(std::filesystem::exists(path))expected=read_file(path);
    const auto save=session.prepare_manual_save(request);(void)bmh::world12::read_save(save);
    return OwnedFilePublication{path,std::move(expected),savegame_bytes(save)};
}
void publish_automatic_save(const std::optional<OwnedFilePublication>& save){if(save)publish_owned_files({*save});}
}
StartupResult serve_startup(ui::StartupFlow& flow,const ui::FlowResult& action,std::unique_ptr<Session>& session,
    const std::filesystem::path& data,uint32_t seed,uint64_t scope,const PendingOfferHooks& hooks,const StartupTurnHooks* turn,const AdvertisingRenewalPresenter& advertising,const ConstructionNoticeHook& construction,const SeasonTicketCompensationNoticeHook& compensation,std::optional<ui::SettingsSoundCapabilities> sound,const SettingsNoticePresenter& settings_notice,const ClubInformationPresenter& club_information,const TableRangePresenter& table_range,const MarketPresenters& market)try {
    if(action.request==ui::FlowRequest::world12_friendly_action){
        if(!session||!flow.world12_friendly_screen()||!flow.pending_world12_friendly()||*flow.pending_world12_friendly()!=action.world12_friendly_action)throw FormatError("world12: friendly action has no matching physical window owner");
        auto* const original=session.get();const auto before=session->snapshot();const auto revision=session->revision();
        auto staged=std::make_unique<Session>(*session);auto screen=std::make_unique<ui::World12FriendlyScreen>(*flow.world12_friendly_screen());bool closed=false;
        std::visit([&](const auto& request){using R=std::decay_t<decltype(request)>;
            if constexpr(std::is_same_v<R,ui::World12FriendlyClose>){staged->close_friendly();closed=true;}
            else if constexpr(std::is_same_v<R,ui::World12TableRequest>){
                if(request.action==ui::World12TableAction::close){staged->close_friendly();closed=true;}
                else if(request.action==ui::World12TableAction::select_opponent)screen->show_selection(staged->select_friendly_opponent(request.team,flow.render(),data));
                else screen->show_picker(staged->select_friendly_league(request,data));
            }else if constexpr(std::is_same_v<R,ui::WorldCupStatisticsRequest>){
                if(request.kind==ui::WorldCupStatisticsRequestKind::close){staged->close_friendly();closed=true;}
                else if(request.kind==ui::WorldCupStatisticsRequestKind::select_opponent){const int country=request.runtime_team-kNoTeam;if(country<0||country>=62||!screen->picker().nation_ids[size_t(country)].valid())throw FormatError("world12: friendly nation token is not an actual country");screen->show_selection(staged->select_friendly_opponent(screen->picker().nation_ids[size_t(country)],flow.render(),data));}
                else throw FormatError("world12: empty friendly nation selection");
            }else if constexpr(std::is_same_v<R,ui::FriendlyMatchRequest>){
                if(request.kind==ui::FriendlyRequestKind::quote){if(!screen->acknowledge_quote(staged->quote_friendly(request,data)))throw FormatError("world12: friendly quote did not match its Source modal");}
                else if(request.kind==ui::FriendlyRequestKind::book){if(!screen->acknowledge_booking(staged->book_friendly(request,data)))throw FormatError("world12: friendly booking did not match its Source modal");if(screen->pending()&&std::holds_alternative<ui::World12FriendlyClose>(*screen->pending())){staged->close_friendly();closed=true;}}
            }
        },action.world12_friendly_action);
        auto parent=staged->preparation_view(data);if(closed)screen.reset();auto view=flow.prepare_world12_friendly_result(std::move(screen),std::move(parent),action.world12_friendly_action,closed);
        if(session.get()!=original||session->revision()!=revision||session->snapshot()!=before||!flow.pending_world12_friendly()||*flow.pending_world12_friendly()!=action.world12_friendly_action)throw FormatError("world12: friendly action changed its outer owner");
        session.swap(staged);flow.finish_world12_friendly(std::move(view));return {};
    }
    if(action.request==ui::FlowRequest::load_game){
        if(session)throw FormatError("world12: setup load cannot replace an active startup session");
        const auto resources=load_runtime_resources(data);flow.open_world12_file_dialog(ui::SaveLoadMode::load,resources.german,save_info_reader(resources.german));return {};
    }
    if(action.request==ui::FlowRequest::settings_action||action.request==ui::FlowRequest::settings_file_action||action.request==ui::FlowRequest::settings_close) {
        if(!session||(!flow.in_settings()&&action.request!=ui::FlowRequest::settings_file_action)||!action.settings_owner_sequence||flow.settings_owner_sequence()!=action.settings_owner_sequence||
           session->settings_sequence()!=action.settings_owner_sequence||flow.settings_owner_revision()!=session->revision())throw FormatError("world12: settings command has no matching window owner");
        auto staged=std::make_unique<Session>(*session);std::optional<std::filesystem::path> path;Bytes bytes;
        if(action.request==ui::FlowRequest::settings_action) {
            if(!flow.pending_settings()||*flow.pending_settings()!=action.settings_action)throw FormatError("world12: settings write differs from pending input");
            if(const auto refusal=staged->settings_refusal(action.settings_action,action.settings_owner_sequence)) {
                if(!settings_notice)throw FormatError(ui::original_to_utf8(message_text(refusal->state,refusal->message_address)));
                const auto* original=session.get();const auto before=session->snapshot();const auto revision=session->revision();
                const auto shown=settings_notice(refusal->state,refusal->message_address);
                if(session.get()!=original||session->snapshot()!=before||session->revision()!=revision||
                   flow.settings_owner_sequence()!=action.settings_owner_sequence||!flow.pending_settings()||*flow.pending_settings()!=action.settings_action)
                    throw FormatError("world12: settings notice changed its live owner");
                if(!shown)throw FormatError("world12: settings notice presentation interrupted");
                flow.reject_settings({});flow.acknowledge_settings_notice_release();return {};
            }
            staged->apply_settings(action.settings_action,action.settings_owner_sequence);
        }else if(action.request==ui::FlowRequest::settings_file_action) {
            if(!flow.pending_file()||*flow.pending_file()!=action.file_action)throw FormatError("world12: settings file differs from pending input");
            path=checked_settings_path(flow.save_directory(),action.file_action);std::optional<Bytes> incoming;
            if(action.file_action.action==ui::SaveLoadAction::load){if(std::filesystem::file_size(*path)!=180)throw FormatError("world12: EIN must contain exactly 180 bytes");incoming=read_file(*path);}
            bytes=staged->prepare_settings_file(action.file_action,action.settings_owner_sequence,incoming);
        }
        // Close does not change settings bytes; project its final state before
        // retiring the window. All view allocation precedes external I/O.
        auto state=staged->settings_view();
        if(action.request==ui::FlowRequest::settings_close){staged->close_settings(action.settings_owner_sequence,staged->revision());state.revision=staged->revision();}
        auto parent=staged->preparation_view(data);auto view=flow.prepare_world12_settings_result(std::move(state),std::move(parent),action);
        if(path&&action.file_action.action==ui::SaveLoadAction::save)publish_settings(*path,bytes,action.file_action.overwrite);
        session.swap(staged);flow.finish_world12_settings(std::move(view));return {};
    }
    if(action.request==ui::FlowRequest::file_action){
        if(!flow.pending_file()||*flow.pending_file()!=action.file_action)throw FormatError("world12: file command has no matching dialog owner");
        if(action.file_action.file_kind==ui::SaveLoadFileKind::menu_layout) {
            if(!session)throw FormatError("world12: layout file has no current career");
            const auto& request=action.file_action;const auto path=checked_menu_layout_path(flow.save_directory(),request);
            auto staged=std::make_unique<Session>(*session);std::optional<Bytes> incoming;if(request.action==ui::SaveLoadAction::load){if(std::filesystem::file_size(path)!=54*16)throw FormatError("world12: MEN file must contain exactly 864 bytes");incoming=read_file(path);}
            const auto payload=staged->prepare_menu_layout(request,incoming);
            auto desk=ui::world12_initial_desktop(data,staged->snapshot(),staged->reference(),staged->rules(),staged->options(),staged->revision());
            auto view=flow.prepare_world12_layout_result(std::move(desk),staged->rules(),request);
            if(request.action==ui::SaveLoadAction::save)publish_menu_layout(path,payload,request.overwrite);
            session.swap(staged);flow.finish_world12_desktop(std::move(view));return {};
        }
        if(action.file_action.file_kind!=ui::SaveLoadFileKind::game)throw FormatError("world12: unsupported file kind");
        const auto& request=action.file_action;const auto path=checked_game_path(flow.save_directory(),request);
        if(session)session->validate_file_owner(request.manager_slot,request.revision);
        if(request.action==ui::SaveLoadAction::save){
            if(!session)throw FormatError("world12: no career to save");
            auto staged=std::make_unique<Session>(*session);const auto save=staged->prepare_manual_save(request);
            auto desk=ui::world12_initial_desktop(data,staged->snapshot(),staged->reference(),staged->rules(),staged->options(),staged->revision());
            auto view=flow.prepare_world12_file_result(std::move(desk),staged->rules(),request);
            publish_game_save(path,save,request.overwrite);session.swap(staged);flow.finish_world12_desktop(std::move(view));return {};
        }
        const auto revision=session?session->revision():request.revision;if(revision==std::numeric_limits<uint64_t>::max())throw FormatError("world12: load revision exhausted");
        auto saved=bmh::world12::read_save(load_savegame(path));auto resources=load_runtime_resources(data);
        auto staged=std::make_unique<Session>(Session::resume(std::move(resources),std::move(saved),session?session->scope():scope,revision+1));
        auto desk=ui::world12_initial_desktop(data,staged->snapshot(),staged->reference(),staged->rules(),staged->options(),staged->revision());
        auto view=flow.prepare_world12_file_result(std::move(desk),staged->rules(),request);session.swap(staged);flow.finish_world12_desktop(std::move(view));return {};
    }
    if(action.request==ui::FlowRequest::world12_readonly_action){
        if(!session||!flow.pending_world12_readonly()||*flow.pending_world12_readonly()!=action.world12_readonly_action)throw FormatError("world12: readonly action has no matching view owner");
        const auto* original=session.get();const auto before=session->snapshot();const auto revision=session->revision();auto staged=std::make_unique<Session>(*session);auto page=staged->readonly_view();
        const bool closed=staged->apply_readonly(action.world12_readonly_action);
        if(session.get()!=original||session->snapshot()!=before||session->revision()!=revision||!flow.pending_world12_readonly()||*flow.pending_world12_readonly()!=action.world12_readonly_action)throw FormatError("world12: readonly presenter changed outer source ownership");
        if(closed){++page.identity.revision;++page.identity.sequence;}else page=staged->readonly_view();
        auto view=flow.prepare_world12_readonly_result(std::move(page),staged->preparation_view(data),action.world12_readonly_action,closed);session.swap(staged);flow.finish_world12_readonly(std::move(view));return {};
    }
    if(action.request==ui::FlowRequest::world12_market_action){
        if(!session||!flow.pending_world12_market()||*flow.pending_world12_market()!=action.world12_market_action)throw FormatError("world12: market action has no matching view");
        const auto* original=session.get();const auto before=session->snapshot();const auto revision=session->revision();auto staged=std::make_unique<Session>(*session);auto page=staged->market_view();
        const bool closed=staged->apply_market(action.world12_market_action,data,market,club_information);
        if(session.get()!=original||session->snapshot()!=before||session->revision()!=revision||!flow.pending_world12_market()||*flow.pending_world12_market()!=action.world12_market_action)throw FormatError("world12: market presenter changed its outer owner");
        if(closed){++page.identity.revision;++page.identity.sequence;++page.market.revision;}else page=staged->market_view();
        auto view=flow.prepare_world12_market_result(std::move(page),staged->preparation_view(data),action.world12_market_action,closed);session.swap(staged);flow.finish_world12_market(std::move(view));return {};
    }
    if(action.request==ui::FlowRequest::world12_table_action){
        if(!session||!flow.pending_world12_table()||*flow.pending_world12_table()!=action.world12_table_action)throw FormatError("world12: table command has no matching view");
        const auto* original=session.get();const auto before=session->snapshot();const auto revision=session->revision();auto staged=std::make_unique<Session>(*session);auto page=staged->tables_view();
        const bool closed=staged->apply_tables(action.world12_table_action,data,club_information,table_range);
        if(session.get()!=original||session->snapshot()!=before||session->revision()!=revision||!flow.pending_world12_table()||*flow.pending_world12_table()!=action.world12_table_action)throw FormatError("world12: table presenter changed its outer owner");
        if(closed){++page.identity.revision;++page.identity.sequence;}else page=staged->tables_view();
        auto view=flow.prepare_world12_table_result(std::move(page),staged->preparation_view(data),action.world12_table_action,closed);session.swap(staged);flow.finish_world12_tables(std::move(view));return {};
    }
    if(action.request==ui::FlowRequest::other_games_action) {
        if(!session||!flow.pending_other_games_action()||*flow.pending_other_games_action()!=action.other_games_action)
            throw FormatError("world12: competition command has no matching view owner");
        auto staged=std::make_unique<Session>(*session);auto page=staged->other_games_view();
        const auto* original=session.get();const auto before=session->snapshot();const auto revision=session->revision();
        const bool closed=staged->apply_other_games(action.other_games_action,data,club_information);
        if(session.get()!=original||session->snapshot()!=before||session->revision()!=revision||
           !flow.pending_other_games_action()||*flow.pending_other_games_action()!=action.other_games_action)
            throw FormatError("world12: club information changed its outer window owner");
        if(closed){++page.world12_identity->sequence;page.world12_identity->revision=staged->revision();}
        else page=staged->other_games_view();
        auto parent=staged->preparation_view(data);auto view=flow.prepare_world12_other_games_result(std::move(page),std::move(parent),action.other_games_action,closed);
        session.swap(staged);flow.finish_world12_other_games(std::move(view));return {};
    }
    if(action.request==ui::FlowRequest::mailbox_action){
        if(!session||!flow.pending_mailbox()||*flow.pending_mailbox()!=action.mailbox_action)throw FormatError("world12: mailbox command has no view owner");
        auto staged=std::make_unique<Session>(*session);staged->edit_mailbox(action.mailbox_action);
        auto view=staged->mailbox_view();auto parent=staged->preparation_view(data);flow.acknowledge_mailbox(std::move(view),std::move(parent));session.swap(staged);return {};
    }
    if(action.request==ui::FlowRequest::mailbox_closed){
        if(!session||!flow.pending_mailbox_close()||*flow.pending_mailbox_close()!=action.mailbox_close)throw FormatError("world12: mailbox close has no view owner");
        auto staged=std::make_unique<Session>(*session);staged->close_mailbox(action.mailbox_close);auto parent=staged->preparation_view(data);auto desk=parent.desktop;
        flow.acknowledge_mailbox_close(action.mailbox_close,std::move(desk),std::move(parent));session.swap(staged);return {};
    }
    if(action.request==ui::FlowRequest::new_career){const auto* request=flow.pending_career();if(!request||!request->options.country_selection)throw FormatError("world12: no typed career request");
        if(request->managers.empty()){
            if(session)throw FormatError("world12: a startup session already exists");
            auto next=std::make_unique<Session>(load_runtime_resources(data),request->options,seed,scope);const auto context=next->selection_context();flow.begin_world_manager_selection(context);session=std::move(next);return {StartupProgress::selecting,{}};
        }
        if(!session||request->managers.size()!=session->snapshot().world.managers.size()+1)throw FormatError("world12: incomplete manager confirmation sequence");
        auto* const original=session.get();const auto before=session->snapshot();const auto before_revision=session->revision();const auto choices=request->managers;
        const auto& choice=request->managers.back();const auto options=request->options;auto staged=std::make_unique<Session>(*session);staged->confirm_manager(choice,choice.roster_revision);staged->prepare_start(staged->revision());if(turn&&turn->initial)turn->initial(*staged);staged->begin_first_event(staged->revision());
        const auto automatic=prepare_automatic_save(*staged,data,flow.save_directory());
        auto desktop=ui::world12_initial_desktop(data,staged->snapshot(),staged->reference(),staged->rules(),options,staged->revision());
        (void)ui::DesktopScreen(data,desktop,staged->rules()).render();
        if(session.get()!=original||session->revision()!=before_revision||session->snapshot()!=before||!flow.pending_career()||flow.pending_career()->options!=options||flow.pending_career()->managers!=choices)throw FormatError("world12: initial presentation changed its outer career or confirmation owner");
        publish_automatic_save(automatic);flow.attach_world12_career(std::move(desktop),staged->rules());session.swap(staged);return {StartupProgress::prepared,{}};
    }
    if(action.request==ui::FlowRequest::manager_confirmed){const auto* choice=flow.pending_manager();if(!session||!choice)throw FormatError("world12: missing manager confirmation owner");
        auto staged=std::make_unique<Session>(*session);staged->confirm_manager(*choice,choice->roster_revision);flow.resume_manager_selection(staged->selection_context());session.swap(staged);return {StartupProgress::selecting,{}};
    }
    if(action.request==ui::FlowRequest::kickoff){
        if(!session||!flow.desktop()||!flow.desktop()->pending()||*flow.desktop()->pending()!=action.desk_action)throw FormatError("world12: manager transition has no matching view owner");
        auto* const original=session.get();const auto before=session->snapshot();const auto before_revision=session->revision();
        const auto unchanged=[&]{if(session.get()!=original||session->revision()!=before_revision||session->snapshot()!=before||!flow.desktop()||!flow.desktop()->pending()||*flow.desktop()->pending()!=action.desk_action)throw FormatError("world12: turn presentation changed its outer career or physical request owner");};
        std::vector<OwnedFilePublication> publications;std::optional<AnnualTransitionHooks> annual;if(turn)annual=turn->annual;
        if(annual)annual->defer_score_write=[&](OwnedFilePublication file){publications.push_back(std::move(file));};
        auto staged=std::make_unique<Session>(*session);bool completed_turn=false,career_complete=false;
        if(staged->snapshot().desktop->initial_event->offer){
            staged->resolve_pending(data,action.desk_action,hooks);
            const auto& event=*staged->snapshot().desktop->initial_event;
            if(size_t(event.manager_position)+1<event.manager_order.size()&&!staged->snapshot().desktop->autosave_pending){auto next=action.desk_action;next.revision=staged->revision();staged->advance_manager(next);}
            else if(turn&&!staged->snapshot().desktop->autosave_pending){auto next=action.desk_action;next.revision=staged->revision();career_complete=staged->advance_to_next_desktop(next,data,turn->clock,turn->matches,annual?&*annual:nullptr,turn->score_file);completed_turn=true;}
        }else {
            const auto& event=*staged->snapshot().desktop->initial_event;
            if(turn&&size_t(event.manager_position)+1==event.manager_order.size()){career_complete=staged->advance_to_next_desktop(action.desk_action,data,turn->clock,turn->matches,annual?&*annual:nullptr,turn->score_file);completed_turn=true;}
            else staged->advance_manager(action.desk_action);
        }
        if(career_complete){unchanged();publish_owned_files(publications);session.swap(staged);return {StartupProgress::complete,{}};}
        const auto automatic=prepare_automatic_save(*staged,data,flow.save_directory());
        if(automatic)publications.push_back(*automatic);
        auto desk=ui::world12_initial_desktop(data,staged->snapshot(),staged->reference(),staged->rules(),staged->options(),staged->revision());
        auto view=completed_turn?flow.prepare_world12_turn_result(std::move(desk),staged->rules(),action.desk_action):
            flow.prepare_world12_manager_result(std::move(desk),staged->rules(),action.desk_action);unchanged();publish_owned_files(publications);session.swap(staged);flow.finish_world12_desktop(std::move(view));return {};
    }
    if(action.request==ui::FlowRequest::desk_action){
        if(!session||!flow.desktop()||!flow.desktop()->pending()||*flow.desktop()->pending()!=action.desk_action)throw FormatError("world12: desktop command has no matching view owner");
        if(action.desk_action.kind==ui::DeskRequestKind::open_menu&&action.desk_action.menu_kind==6){
            auto staged=std::make_unique<Session>(*session);auto picker=staged->open_friendly(action.desk_action,data);auto parent=staged->preparation_view(data);auto view=flow.prepare_world12_friendly_open(std::move(picker),std::move(parent),action.desk_action);session.swap(staged);flow.finish_world12_friendly(std::move(view));return {};
        }
        if(action.desk_action.kind==ui::DeskRequestKind::change_layout||action.desk_action.kind==ui::DeskRequestKind::change_option||action.desk_action.kind==ui::DeskRequestKind::change_ticket_price){
            auto staged=std::make_unique<Session>(*session);staged->apply_desktop(action.desk_action,data);
            auto desk=ui::world12_initial_desktop(data,staged->snapshot(),staged->reference(),staged->rules(),staged->options(),staged->revision());
            flow.acknowledge_desk(std::move(desk));session.swap(staged);return {};
        }
        if(action.desk_action.kind==ui::DeskRequestKind::open_menu&&(action.desk_action.menu_kind==4||action.desk_action.menu_kind==8||action.desk_action.menu_kind==9||action.desk_action.menu_kind==23||action.desk_action.menu_kind==20||action.desk_action.menu_kind==25||action.desk_action.menu_kind==26)){
            auto staged=std::make_unique<Session>(*session);auto page=staged->open_readonly(action.desk_action,data,flow.desktop()->render(),flow.save_directory());
            auto view=flow.prepare_world12_readonly_open(std::move(page),staged->preparation_view(data),action.desk_action);session.swap(staged);flow.finish_world12_readonly(std::move(view));return {};
        }
        if(action.desk_action.kind==ui::DeskRequestKind::open_menu&&action.desk_action.menu_kind==17){
            auto staged=std::make_unique<Session>(*session);auto page=staged->open_market(action.desk_action);auto parent=staged->preparation_view(data);flow.show_world12_market(std::move(page),std::move(parent),action.desk_action);session.swap(staged);return {};
        }
        if(action.desk_action.kind==ui::DeskRequestKind::open_menu&&action.desk_action.menu_kind>=0&&action.desk_action.menu_kind<=2){
            auto staged=std::make_unique<Session>(*session);auto page=staged->open_tables(action.desk_action);auto parent=staged->preparation_view(data);flow.show_world12_tables(std::move(page),std::move(parent),action.desk_action);session.swap(staged);return {};
        }
        if(action.desk_action.kind==ui::DeskRequestKind::open_menu&&action.desk_action.menu_kind==5){
            auto staged=std::make_unique<Session>(*session);auto page=staged->open_other_games(action.desk_action);auto parent=staged->preparation_view(data);
            flow.show_world12_other_games(std::move(page),std::move(parent),action.desk_action);session.swap(staged);return {};
        }
        if(action.desk_action.kind==ui::DeskRequestKind::open_menu&&action.desk_action.menu_kind==19){
            auto staged=std::make_unique<Session>(*session);auto view=staged->open_settings(action.desk_action,sound);
            flow.show_settings(std::move(view),staged->settings_sequence(),action.desk_action);session.swap(staged);return {};
        }
        if(action.desk_action.kind==ui::DeskRequestKind::open_menu&&action.desk_action.menu_kind==53){
            session->validate_file_owner(action.desk_action.manager_slot,action.desk_action.revision);
            if(action.desk_action.value!=0&&action.desk_action.value!=1)throw FormatError("world12: invalid layout file mode");
            flow.open_world12_layout_dialog(action.desk_action.value==0?ui::SaveLoadMode::load:ui::SaveLoadMode::save,action.desk_action,session->rules());return {};
        }
        if(action.desk_action.kind==ui::DeskRequestKind::open_menu&&action.desk_action.menu_kind==63){
            auto staged=std::make_unique<Session>(*session);staged->open_mailbox(action.desk_action);flow.show_mailbox(staged->mailbox_view());session.swap(staged);return {};
        }
        if(action.desk_action.kind==ui::DeskRequestKind::open_menu&&action.desk_action.menu_kind==24){
            auto staged=std::make_unique<Session>(*session);auto view=staged->open_environment(action.desk_action,data);
            auto parent=staged->preparation_view(data);flow.show_environment(std::move(view),action.desk_action,std::move(parent));session.swap(staged);return {};
        }
        if(action.desk_action.kind==ui::DeskRequestKind::open_menu&&action.desk_action.menu_kind==13){
            auto staged=std::make_unique<Session>(*session);staged->open_toto(action.desk_action,data);flow.show_world12_preparation(staged->preparation_view(data),13);session.swap(staged);return {};
        }
        if(action.desk_action.kind==ui::DeskRequestKind::open_menu&&action.desk_action.menu_kind==22){
            auto staged=std::make_unique<Session>(*session);staged->open_stadium(action.desk_action,data);
            flow.show_world12_preparation(staged->preparation_view(data),22);session.swap(staged);return {};
        }
        if(action.desk_action.kind==ui::DeskRequestKind::open_menu&&action.desk_action.menu_kind==21){
            auto staged=std::make_unique<Session>(*session);auto view=staged->open_advertising(action.desk_action,data);auto parent=staged->preparation_view(data);
            flow.show_advertising(std::move(view),std::move(parent));session.swap(staged);return {};
        }
        if(action.desk_action.kind==ui::DeskRequestKind::open_menu&&action.desk_action.menu_kind==27){
            auto staged=std::make_unique<Session>(*session);auto view=staged->open_contracts(action.desk_action,data);auto parent=staged->preparation_view(data);
            flow.show_contracts(std::move(view),std::move(parent));session.swap(staged);return {};
        }
        if(action.desk_action.kind==ui::DeskRequestKind::open_menu&&(action.desk_action.menu_kind==10||action.desk_action.menu_kind==11)){
            session->validate_file_owner(action.desk_action.manager_slot,action.desk_action.revision);
            flow.open_world12_file_dialog(action.desk_action.menu_kind==10?ui::SaveLoadMode::load:ui::SaveLoadMode::save,session->rules(),save_info_reader(session->rules()));return {};
        }
        if(action.desk_action.kind==ui::DeskRequestKind::open_menu&&
           (action.desk_action.menu_kind==7||action.desk_action.menu_kind==29)) {
            session->validate_file_owner(action.desk_action.manager_slot,action.desk_action.revision);
            throw FormatError(action.desk_action.menu_kind==7?
                "Spielverlegung ist in dieser Beta noch nicht verfuegbar.":
                "Neue Mitspieler bitte beim Spielstart festlegen (1-4).");
        }
        auto staged=std::make_unique<Session>(*session);staged->open_preparation(action.desk_action);
        auto view=staged->preparation_view(data);flow.show_world12_preparation(std::move(view),action.desk_action.menu_kind);session.swap(staged);return {};
    }
    if(action.request==ui::FlowRequest::submenu_action){
        if(!session||!flow.matches_pending_preparation(action.submenu_action))throw FormatError("world12: preparation command has no matching view owner");
        const auto* original=session.get();const auto revision=session->revision();std::optional<bmh::world12::CareerSnapshot> before;
        std::optional<ui::StadiumState> stadium_owner;if(std::holds_alternative<ui::StadiumRequest>(action.submenu_action)){before=session->snapshot();stadium_owner=session->stadium_view(data);}
        auto staged=std::make_unique<Session>(*session);
        if(const auto* bank=std::get_if<ui::BankRequest>(&action.submenu_action))staged->apply_bank(data,*bank);
        else if(const auto* stadium=std::get_if<ui::StadiumRequest>(&action.submenu_action))staged->apply_stadium(data,*stadium,construction,compensation);
        else if(const auto* toto=std::get_if<ui::TotoRequest>(&action.submenu_action))staged->apply_toto(*toto);
        else staged->apply_preparation(action.submenu_action);
        if(stadium_owner) {
            if(session.get()!=original||session->revision()!=revision||session->snapshot()!=*before||!flow.matches_pending_preparation(action.submenu_action))
                throw FormatError("world12: construction presenter changed its live owner");
            const auto current=session->stadium_view(data);if(current.scope!=stadium_owner->scope||current.sequence!=stadium_owner->sequence)
                throw FormatError("world12: construction presenter changed its stadium window");
        }
        auto view=staged->preparation_view(data);flow.acknowledge_preparation(std::move(view));if(!flow.in_preparation_menu())staged->close_preparation();session.swap(staged);return {};
    }
    if(action.request==ui::FlowRequest::environment_action){
        if(!session||!flow.pending_environment()||*flow.pending_environment()!=action.environment_action)
            throw FormatError("world12: environment request has no matching view owner");
        auto staged=std::make_unique<Session>(*session);const auto changed=staged->apply_environment(action.environment_action);
        auto view=staged->environment_view(data);std::optional<ui::PreparationState> parent;
        if(changed)parent=staged->preparation_view(data);
        flow.acknowledge_environment(std::move(view),std::move(parent));session.swap(staged);return {};
    }
    if(action.request==ui::FlowRequest::advertising_action){
        if(!session||!flow.pending_advertising_action()||*flow.pending_advertising_action()!=action.advertising_action)
            throw FormatError("world12: advertising command has no matching view owner");
        const auto* original=session.get();const auto before=session->snapshot();const auto revision=session->revision();
        const auto next=session->advertising_view(data).request_id_base;
        auto staged=std::make_unique<Session>(*session);const auto result=staged->apply_advertising(data,action.advertising_action,advertising);
        if(session.get()!=original||session->revision()!=revision||session->snapshot()!=before||
           session->advertising_view(data).request_id_base!=next||!flow.pending_advertising_action()||*flow.pending_advertising_action()!=action.advertising_action)
            throw FormatError("world12: advertising presenter changed its live session or view owner");
        auto view=staged->advertising_view(data);auto parent=staged->preparation_view(data);auto desk=parent.desktop;
        if(action.advertising_action.kind==ui::AdvertisingActionKind::set_spending&&result.message_address) {
            const auto texts=load_textd(ui::asset_path(data,"TEXTD.BLK"));view.notification_text=ui::original_to_utf8(texts[(result.message_address-layout::TextPointerTable)/4]);
            view.notification_warning=result.status==AdvertisingSpendingStatus::insufficient_funds;
        }
        flow.acknowledge_advertising(std::move(view),std::move(desk),std::move(parent));session.swap(staged);return {};
    }
    if(action.request==ui::FlowRequest::contract_action){
        if(!session||!flow.pending_contract_action()||*flow.pending_contract_action()!=action.contract_action)
            throw FormatError("world12: contract command has no matching view owner");
        auto staged=std::make_unique<Session>(*session);const auto result=staged->apply_contract(data,action.contract_action);
        auto view=staged->contracts_view(data);auto parent=staged->preparation_view(data);auto desk=parent.desktop;
        view.selected_row=action.contract_action.row;view.feedback=result.feedback;
        switch(action.contract_action.kind){
        case ui::ContractActionKind::request_demands:flow.acknowledge_contract_quote(std::move(view),std::move(desk),std::move(parent));break;
        case ui::ContractActionKind::submit_offer:flow.acknowledge_contract_offer(std::move(view),std::move(desk),std::move(parent));break;
        case ui::ContractActionKind::submit_bonuses:flow.acknowledge_contract_bonuses(std::move(view),std::move(desk),std::move(parent));break;
        case ui::ContractActionKind::insure_player:flow.acknowledge_contract_insurance(std::move(view),std::move(desk),std::move(parent));break;
        }
        session.swap(staged);return {};
    }
    if(action.request==ui::FlowRequest::contracts_closed&&session&&!flow.in_preparation_menu()){session->close_contracts();return {};}
    if(action.request==ui::FlowRequest::submenu_closed&&session&&flow.in_settings())return {};
    if(action.request==ui::FlowRequest::submenu_closed&&session&&!flow.in_preparation_menu()){if(session->preparation_open())session->close_preparation();return {};}
    if(action.request!=ui::FlowRequest::none&&action.request!=ui::FlowRequest::submenu_opened&&action.request!=ui::FlowRequest::submenu_closed)throw FormatError("world12: this frontend action has no typed owner yet");
    return {};
}catch(const std::exception& error){if(action.request==ui::FlowRequest::world12_friendly_action&&flow.pending_world12_friendly()&&*flow.pending_world12_friendly()==action.world12_friendly_action)flow.reject_world12_friendly(error.what());if(action.request==ui::FlowRequest::world12_readonly_action&&flow.pending_world12_readonly()&&*flow.pending_world12_readonly()==action.world12_readonly_action)flow.reject_world12_readonly(error.what());if(action.request==ui::FlowRequest::world12_market_action&&flow.pending_world12_market()&&*flow.pending_world12_market()==action.world12_market_action)flow.reject_world12_market(error.what());if(action.request==ui::FlowRequest::world12_table_action&&flow.pending_world12_table()&&*flow.pending_world12_table()==action.world12_table_action)flow.reject_world12_table(error.what());if(action.request==ui::FlowRequest::other_games_action&&flow.pending_other_games_action()&&*flow.pending_other_games_action()==action.other_games_action)flow.reject_other_games(error.what());if(action.request==ui::FlowRequest::settings_action&&flow.settings_owner_sequence()==action.settings_owner_sequence&&flow.pending_settings()&&*flow.pending_settings()==action.settings_action)flow.reject_settings(error.what());if(action.request==ui::FlowRequest::settings_file_action&&flow.pending_file())flow.reject_settings_file(error.what());if(action.request==ui::FlowRequest::settings_close)flow.reject_settings_close(error.what());if(action.request==ui::FlowRequest::advertising_action&&flow.pending_advertising_action())flow.reject_advertising_action(error.what());if(action.request==ui::FlowRequest::contract_action&&flow.pending_contract_action())flow.reject_contract_action(error.what());if(action.request==ui::FlowRequest::environment_action&&flow.pending_environment())flow.reject_environment_action(error.what());if(action.request==ui::FlowRequest::mailbox_action&&flow.pending_mailbox())flow.reject_mailbox(error.what());if(action.request==ui::FlowRequest::mailbox_closed&&flow.pending_mailbox_close())flow.reject_mailbox_close(error.what());if(action.request==ui::FlowRequest::file_action&&flow.pending_file()&&*flow.pending_file()==action.file_action)flow.reject_file_action(error.what());if(action.request==ui::FlowRequest::desk_action||action.request==ui::FlowRequest::kickoff)flow.reject_desk_action(error.what());else if(action.request==ui::FlowRequest::submenu_action&&flow.matches_pending_preparation(action.submenu_action))flow.reject_preparation_action(error.what());return {StartupProgress::idle,error.what()};}
}
