// SPDX-License-Identifier: GPL-3.0-only
#include "app/world12_startup.hpp"
namespace bmh::app::world12 {
StartupResult serve_startup(ui::StartupFlow& flow,const ui::FlowResult& action,std::unique_ptr<Session>& session,
    const std::filesystem::path& data,uint32_t seed,uint64_t scope)try {
    if(action.request==ui::FlowRequest::new_career){const auto* request=flow.pending_career();if(!request||!request->options.country_selection)throw FormatError("world12: no typed career request");
        if(request->managers.empty()){
            if(session)throw FormatError("world12: a startup session already exists");
            auto next=std::make_unique<Session>(load_runtime_resources(data),request->options,seed,scope);const auto context=next->selection_context();flow.begin_world_manager_selection(context);session=std::move(next);return {StartupProgress::selecting,{}};
        }
        if(!session||request->managers.size()!=session->snapshot().world.managers.size()+1)throw FormatError("world12: incomplete manager confirmation sequence");
        const auto& choice=request->managers.back();auto staged=std::make_unique<Session>(*session);staged->confirm_manager(choice,choice.roster_revision);staged->prepare_start(staged->revision());session.swap(staged);return {StartupProgress::prepared,{}};
    }
    if(action.request==ui::FlowRequest::manager_confirmed){const auto* choice=flow.pending_manager();if(!session||!choice)throw FormatError("world12: missing manager confirmation owner");
        auto staged=std::make_unique<Session>(*session);staged->confirm_manager(*choice,choice->roster_revision);flow.resume_manager_selection(staged->selection_context());session.swap(staged);return {StartupProgress::selecting,{}};
    }
    if(action.request!=ui::FlowRequest::none&&action.request!=ui::FlowRequest::submenu_opened&&action.request!=ui::FlowRequest::submenu_closed)throw FormatError("world12: this frontend action has no typed owner yet");
    return {};
}catch(const std::exception& error){return {StartupProgress::idle,error.what()};}
}
