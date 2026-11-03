// SPDX-License-Identifier: GPL-3.0-only
#include "ui/flow.hpp"
namespace bmh::ui {
namespace {
void same_friendly_career(const DeskState& before,const DeskState& after){
    if(after.source!="world12"||after.revision<before.revision||desk_actor(after)!=desk_actor(before)||after.runtime_club!=before.runtime_club||after.day_counter!=before.day_counter||after.options!=before.options||after.world12_career_roster!=before.world12_career_roster)
        throw FormatError("world12: friendly window changed its career/actor/date owner");
}
FlowResult friendly_result(bool changed,const World12FriendlyRequest* request){FlowResult result{changed};if(request){result.request=FlowRequest::world12_friendly_action;result.world12_friendly_action=*request;}return result;}
}
StartupFlow::World12FriendlyView StartupFlow::prepare_world12_friendly_open(World12FriendlyPicker picker,PreparationState parent,const DeskRequest& request)const{
    if(!desktop_||!world12_reference_||!engine_attached_||in_preparation_menu()||in_readonly_view()||!desktop_->pending()||*desktop_->pending()!=request||request.kind!=DeskRequestKind::open_menu||request.menu_kind!=6||request.manager_slot!=desk_actor(desktop_->state())||request.revision!=desktop_->state().revision||picker.canonical_actor!=request.manager_slot||picker.revision!=parent.desktop.revision)
        throw FormatError("world12: friendly picker lacks its accepted canonical desktop request");
    same_friendly_career(desktop_->state(),parent.desktop);validate_preparation_context(parent);World12FriendlyView out;
    out.screen=std::make_unique<World12FriendlyScreen>(data_,std::move(picker),desktop_->render(),render_reference());out.desktop=std::make_unique<DesktopScreen>(data_,parent.desktop,render_reference());out.preparation=std::make_unique<PreparationState>(std::move(parent));return out;
}
StartupFlow::World12FriendlyView StartupFlow::prepare_world12_friendly_result(std::unique_ptr<World12FriendlyScreen> screen,PreparationState parent,const World12FriendlyRequest& request,bool closed)const{
    if(!world12_friendly_||!pending_world12_friendly()||*pending_world12_friendly()!=request||!desktop_||!world12_reference_||closed==bool(screen))throw FormatError("world12: friendly reply has no matching pending window");
    same_friendly_career(desktop_->state(),parent.desktop);validate_preparation_context(parent);
    if(screen&&screen->picker().canonical_actor!=desk_actor(parent.desktop))throw FormatError("world12: friendly reply changed its canonical person");
    World12FriendlyView out;out.screen=std::move(screen);out.desktop=std::make_unique<DesktopScreen>(data_,parent.desktop,render_reference());out.preparation=std::make_unique<PreparationState>(std::move(parent));out.release_barrier=world12_friendly_->owns_pending_release();return out;
}
void StartupFlow::finish_world12_friendly(World12FriendlyView view)noexcept{desktop_=std::move(view.desktop);preparation_=std::move(view.preparation);world12_friendly_=std::move(view.screen);world12_friendly_release_barrier_=view.release_barrier;}
void StartupFlow::reject_world12_friendly(std::string reason){if(!world12_friendly_||!pending_world12_friendly())throw FormatError("world12: friendly rejection has no pending owner");const bool barrier=world12_friendly_->owns_pending_release();auto screen=std::make_unique<World12FriendlyScreen>(*world12_friendly_);screen->reject(std::move(reason));world12_friendly_=std::move(screen);world12_friendly_release_barrier_=barrier;}
FlowResult StartupFlow::world12_friendly_down(int x,int y,bool right){if(!world12_friendly_||world12_friendly_release_barrier_)return {};const bool changed=world12_friendly_->pointer_down(x,y,right);return friendly_result(changed,pending_world12_friendly());}
FlowResult StartupFlow::world12_friendly_up(int x,int y,bool right){if(world12_friendly_release_barrier_){world12_friendly_release_barrier_=false;return {};}if(!world12_friendly_)return {};const bool changed=world12_friendly_->pointer_up(x,y,right);return friendly_result(changed,pending_world12_friendly());}
FlowResult StartupFlow::world12_friendly_key(std::string_view key){if(!world12_friendly_||world12_friendly_release_barrier_)return {};const bool changed=world12_friendly_->key(key);return friendly_result(changed,pending_world12_friendly());}
}
