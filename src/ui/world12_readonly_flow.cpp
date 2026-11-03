// SPDX-License-Identifier: GPL-3.0-only
#include "ui/flow.hpp"
namespace bmh::ui {
namespace {
void same_readonly_desktop(const DeskState& before,const DeskState& after){auto expected=before;expected.revision=after.revision;if(desk_json(expected)!=desk_json(after))throw FormatError("world12: readonly result changed unrelated desktop facts");}
}
StartupFlow::World12ReadonlyView StartupFlow::prepare_world12_readonly_open(World12ReadonlyState page,PreparationState parent,const DeskRequest& request)const{
    if(!desktop_||!world12_reference_||!engine_attached_||in_preparation_menu()||in_readonly_view()||!desktop_->pending()||*desktop_->pending()!=request||request.kind!=DeskRequestKind::open_menu||request.manager_slot!=desk_actor(desktop_->state())||request.revision!=desktop_->state().revision||page.identity.manager!=request.manager_slot||page.identity.menu!=request.menu_kind||page.identity.revision!=request.revision+1||page.identity.career_roster!=desktop_->state().world12_career_roster||parent.desktop.revision!=page.identity.revision||desk_actor(parent.desktop)!=request.manager_slot)
        throw FormatError("world12: readonly opening lacks its canonical desktop owner");
    same_readonly_desktop(desktop_->state(),parent.desktop);validate_preparation_context(parent);World12ReadonlyView out;
    out.screen=std::make_unique<World12ReadonlyScreen>(data_,std::move(page),render_reference());out.desktop=std::make_unique<DesktopScreen>(data_,parent.desktop,render_reference());out.preparation=std::make_unique<PreparationState>(std::move(parent));return out;
}
StartupFlow::World12ReadonlyView StartupFlow::prepare_world12_readonly_result(World12ReadonlyState page,PreparationState parent,const World12ReadonlyRequest& request,bool closed)const{
    if(!world12_readonly_||!pending_world12_readonly()||*pending_world12_readonly()!=request||world12_readonly_->state().identity!=request.identity||!desktop_||!world12_reference_||closed!=(request.action==World12ReadonlyAction::close))throw FormatError("world12: readonly result has no matching window");
    auto next=request.identity;++next.revision;++next.sequence;if(page.identity!=next||parent.desktop.revision!=next.revision||desk_actor(parent.desktop)!=next.manager||page.identity.career_roster!=parent.desktop.world12_career_roster)throw FormatError("world12: readonly result changed actor/generation/revision");
    same_readonly_desktop(desktop_->state(),parent.desktop);validate_preparation_context(parent);World12ReadonlyView out;if(!closed)out.screen=std::make_unique<World12ReadonlyScreen>(data_,std::move(page),render_reference());
    out.desktop=std::make_unique<DesktopScreen>(data_,parent.desktop,render_reference());out.preparation=std::make_unique<PreparationState>(std::move(parent));out.release_barrier=world12_readonly_->owns_pending_release();return out;
}
void StartupFlow::finish_world12_readonly(World12ReadonlyView view)noexcept{desktop_=std::move(view.desktop);preparation_=std::move(view.preparation);world12_readonly_=std::move(view.screen);world12_readonly_release_barrier_=view.release_barrier;}
void StartupFlow::reject_world12_readonly(std::string reason){
    if(!world12_readonly_||!pending_world12_readonly())throw FormatError("world12: readonly rejection has no pending owner");
    const bool barrier=world12_readonly_->owns_pending_release();auto next=std::make_unique<World12ReadonlyScreen>(data_,world12_readonly_->state(),render_reference());next->reject(std::move(reason));world12_readonly_=std::move(next);world12_readonly_release_barrier_=barrier;
}
FlowResult StartupFlow::world12_readonly_down(int x,int y,bool right){
    if(world12_readonly_release_barrier_)return {};
    if(!world12_readonly_)return {};const bool changed=world12_readonly_->pointer_down(x,y,right);FlowResult out{changed};if(const auto* request=pending_world12_readonly()){out.request=FlowRequest::world12_readonly_action;out.world12_readonly_action=*request;}return out;
}
FlowResult StartupFlow::world12_readonly_up(int x,int y,bool right){
    if(world12_readonly_release_barrier_){world12_readonly_release_barrier_=false;return {};}
    if(!world12_readonly_)return {};const bool changed=world12_readonly_->pointer_up(x,y,right);FlowResult out{changed};if(const auto* request=pending_world12_readonly()){out.request=FlowRequest::world12_readonly_action;out.world12_readonly_action=*request;}return out;
}
FlowResult StartupFlow::world12_readonly_key(std::string_view key){
    if(world12_readonly_release_barrier_||!world12_readonly_)return {};const bool changed=world12_readonly_->key(key);FlowResult out{changed};if(const auto* request=pending_world12_readonly()){out.request=FlowRequest::world12_readonly_action;out.world12_readonly_action=*request;}return out;
}

}
