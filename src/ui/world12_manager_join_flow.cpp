// SPDX-License-Identifier: GPL-3.0-only
#include "ui/flow.hpp"
#include <algorithm>
namespace bmh::ui {
void StartupFlow::show_world12_manager_join(std::vector<ManagerChoice> choices,SelectionContext context){
    if(!desktop_||!engine_attached_||!world12_reference_||joining_||in_preparation_menu()||in_readonly_view()||!desktop_->pending()||desktop_->pending()->kind!=DeskRequestKind::open_menu||desktop_->pending()->menu_kind!=29||desktop_->state().national_team||!context.world||!context.world->joining||context.revision!=desktop_->state().revision||choices.size()!=desktop_->state().world12_career_roster.size())
        throw FormatError("world12: manager join has no accepted running desktop");
    for(size_t i=0;i<choices.size();++i){const auto& choice=choices[i];const auto& roster=desktop_->state().world12_career_roster;
        if(choice.slot!=int(i)||choice.master_club!=-1||std::none_of(roster.begin(),roster.end(),[&](const auto& actor){return actor.club==choice.world_club&&actor.name==original_to_utf8(choice.name)&&actor.portrait==choice.portrait;}))throw FormatError("world12: joining picker changed an existing player");
    }
    auto setup=std::make_unique<SetupScreen>(data_,desktop_->state().options,setup_symbol_);
    join_origin_=*desktop_->pending();join_choices_=std::move(choices);join_context_revision_=context.revision;join_setup_=std::move(setup);manager_.reset();joining_=true;join_options_pending_=join_choice_pending_=false;
}
bool StartupFlow::matches_world12_join(const FlowResult& action)const{
    if(!joining_||!world12_reference_||!desktop_||!desktop_->pending()||*desktop_->pending()!=join_origin_||action.desk_action!=join_origin_)return false;
    if(action.request==FlowRequest::join_options)return join_options_pending_&&!join_choice_pending_&&join_setup_&&action.join_options==join_setup_->options();
    if(action.request==FlowRequest::join_manager)return join_choice_pending_&&!join_options_pending_&&!join_choices_.empty()&&action.join_manager==join_choices_.back();
    return action.request==FlowRequest::join_cancel;
}
StartupFlow::World12JoinView StartupFlow::prepare_world12_join_result(PreparationState parent,const DeskRequest& origin)const{
    if(!joining_||(!join_options_pending_&&!join_choice_pending_)||!desktop_||origin!=join_origin_||!desktop_->pending()||*desktop_->pending()!=origin||!world12_reference_||parent.desktop.source!="world12"||parent.desktop.revision!=origin.revision+1||desk_actor(parent.desktop)!=desk_actor(desktop_->state())||parent.desktop.day_counter!=desktop_->state().day_counter||parent.desktop.options!=join_setup_->options()||parent.desktop.world12_career_roster.size()!=join_choices_.size())
        throw FormatError("world12: joining reply changed its host, options or calendar");
    for(const auto& old:desktop_->state().world12_career_roster)if(std::find(parent.desktop.world12_career_roster.begin(),parent.desktop.world12_career_roster.end(),old)==parent.desktop.world12_career_roster.end())throw FormatError("world12: joining reply replaced an existing human");
    for(const auto& choice:join_choices_)if(std::none_of(parent.desktop.world12_career_roster.begin(),parent.desktop.world12_career_roster.end(),[&](const auto& actor){return actor.club==choice.world_club&&actor.name==original_to_utf8(choice.name)&&actor.portrait==choice.portrait;}))throw FormatError("world12: joining reply differs from the accepted picker choices");
    validate_preparation_context(parent);World12JoinView out;out.desktop=std::make_unique<DesktopScreen>(data_,parent.desktop,render_reference());out.preparation=std::make_unique<PreparationState>(std::move(parent));return out;
}
void StartupFlow::finish_world12_join(World12JoinView view)noexcept{
    desktop_=std::move(view.desktop);preparation_=std::move(view.preparation);manager_.reset();join_setup_.reset();join_choices_.clear();confirmed_.clear();confirmed_runtime_teams_.clear();joining_=join_options_pending_=join_choice_pending_=false;
}
}
