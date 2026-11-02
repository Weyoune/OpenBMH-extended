// SPDX-License-Identifier: GPL-3.0-only
#include "app/world12_startup.hpp"
#include "ui/world12_desktop.hpp"
#include "app/world12_save_load.hpp"
#include <limits>
namespace bmh::app::world12 {
StartupResult serve_startup(ui::StartupFlow& flow,const ui::FlowResult& action,std::unique_ptr<Session>& session,
    const std::filesystem::path& data,uint32_t seed,uint64_t scope,const PendingOfferHooks& hooks,const StartupTurnHooks* turn)try {
    if(action.request==ui::FlowRequest::load_game){
        if(session)throw FormatError("world12: setup load cannot replace an active startup session");
        const auto resources=load_runtime_resources(data);flow.open_world12_file_dialog(ui::SaveLoadMode::load,resources.german,save_info_reader(resources.german));return {};
    }
    if(action.request==ui::FlowRequest::file_action){
        if(!flow.pending_file()||*flow.pending_file()!=action.file_action||action.file_action.file_kind!=ui::SaveLoadFileKind::game)throw FormatError("world12: file command has no matching dialog owner");
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
        const auto& choice=request->managers.back();const auto options=request->options;auto staged=std::make_unique<Session>(*session);staged->confirm_manager(choice,choice.roster_revision);staged->prepare_start(staged->revision());staged->begin_first_event(staged->revision());
        auto desktop=ui::world12_initial_desktop(data,staged->snapshot(),staged->reference(),staged->rules(),options,staged->revision());flow.attach_world12_career(std::move(desktop),staged->rules());session.swap(staged);return {StartupProgress::prepared,{}};
    }
    if(action.request==ui::FlowRequest::manager_confirmed){const auto* choice=flow.pending_manager();if(!session||!choice)throw FormatError("world12: missing manager confirmation owner");
        auto staged=std::make_unique<Session>(*session);staged->confirm_manager(*choice,choice->roster_revision);flow.resume_manager_selection(staged->selection_context());session.swap(staged);return {StartupProgress::selecting,{}};
    }
    if(action.request==ui::FlowRequest::kickoff){
        if(!session||!flow.desktop()||!flow.desktop()->pending()||*flow.desktop()->pending()!=action.desk_action)throw FormatError("world12: manager transition has no matching view owner");
        auto staged=std::make_unique<Session>(*session);bool completed_turn=false;
        if(staged->snapshot().desktop->initial_event->offer){
            staged->resolve_pending(data,action.desk_action,hooks);
            const auto& event=*staged->snapshot().desktop->initial_event;
            if(size_t(event.manager_position)+1<event.manager_order.size()&&!staged->snapshot().desktop->autosave_pending){auto next=action.desk_action;next.revision=staged->revision();staged->advance_manager(next);}
            else if(turn&&!staged->snapshot().desktop->autosave_pending){auto next=action.desk_action;next.revision=staged->revision();staged->advance_to_next_desktop(next,data,turn->clock,turn->matches);completed_turn=true;}
        }else {
            const auto& event=*staged->snapshot().desktop->initial_event;
            if(turn&&size_t(event.manager_position)+1==event.manager_order.size()){staged->advance_to_next_desktop(action.desk_action,data,turn->clock,turn->matches);completed_turn=true;}
            else staged->advance_manager(action.desk_action);
        }
        auto desk=ui::world12_initial_desktop(data,staged->snapshot(),staged->reference(),staged->rules(),staged->options(),staged->revision());
        auto view=completed_turn?flow.prepare_world12_turn_result(std::move(desk),staged->rules(),action.desk_action):
            flow.prepare_world12_manager_result(std::move(desk),staged->rules(),action.desk_action);session.swap(staged);flow.finish_world12_desktop(std::move(view));return {};
    }
    if(action.request==ui::FlowRequest::desk_action){
        if(!session||!flow.desktop()||!flow.desktop()->pending()||*flow.desktop()->pending()!=action.desk_action)throw FormatError("world12: desktop command has no matching view owner");
        if(action.desk_action.kind==ui::DeskRequestKind::open_menu&&action.desk_action.menu_kind==63){
            auto staged=std::make_unique<Session>(*session);staged->open_mailbox(action.desk_action);flow.show_mailbox(staged->mailbox_view());session.swap(staged);return {};
        }
        if(action.desk_action.kind==ui::DeskRequestKind::open_menu&&(action.desk_action.menu_kind==10||action.desk_action.menu_kind==11)){
            session->validate_file_owner(action.desk_action.manager_slot,action.desk_action.revision);
            flow.open_world12_file_dialog(action.desk_action.menu_kind==10?ui::SaveLoadMode::load:ui::SaveLoadMode::save,session->rules(),save_info_reader(session->rules()));return {};
        }
        auto staged=std::make_unique<Session>(*session);staged->open_preparation(action.desk_action);
        auto view=staged->preparation_view(data);flow.show_world12_preparation(std::move(view),action.desk_action.menu_kind);session.swap(staged);return {};
    }
    if(action.request==ui::FlowRequest::submenu_action){
        if(!session||!flow.matches_pending_preparation(action.submenu_action))throw FormatError("world12: preparation command has no matching view owner");
        auto staged=std::make_unique<Session>(*session);
        if(const auto* bank=std::get_if<ui::BankRequest>(&action.submenu_action))staged->apply_bank(data,*bank);
        else staged->apply_preparation(action.submenu_action);
        auto view=staged->preparation_view(data);flow.acknowledge_preparation(std::move(view));session.swap(staged);return {};
    }
    if(action.request==ui::FlowRequest::submenu_closed&&session&&!flow.in_preparation_menu()){if(session->preparation_open())session->close_preparation();return {};}
    if(action.request!=ui::FlowRequest::none&&action.request!=ui::FlowRequest::submenu_opened&&action.request!=ui::FlowRequest::submenu_closed)throw FormatError("world12: this frontend action has no typed owner yet");
    return {};
}catch(const std::exception& error){if(action.request==ui::FlowRequest::mailbox_action&&flow.pending_mailbox())flow.reject_mailbox(error.what());if(action.request==ui::FlowRequest::mailbox_closed&&flow.pending_mailbox_close())flow.reject_mailbox_close(error.what());if(action.request==ui::FlowRequest::file_action&&flow.pending_file()&&*flow.pending_file()==action.file_action)flow.reject_file_action(error.what());if(action.request==ui::FlowRequest::desk_action||action.request==ui::FlowRequest::kickoff)flow.reject_desk_action(error.what());else if(action.request==ui::FlowRequest::submenu_action&&flow.matches_pending_preparation(action.submenu_action))flow.reject_preparation_action(error.what());return {StartupProgress::idle,error.what()};}
}
