// SPDX-License-Identifier: GPL-3.0-only
#include "app/world12_session.hpp"
#include "app/world12_visible_menu.hpp"
#include "game/world12/manager_join.hpp"
#include "game/world12/national_coach.hpp"
#include "ui/world12_manager_selection.hpp"
#include "ui/world12_desktop.hpp"
#include <algorithm>
namespace bmh::app::world12 {
using namespace bmh::world12;
std::vector<ui::ManagerChoice> Session::joining_choices()const {
    const auto& state=join_?join_->candidate:state_;std::vector<ui::ManagerChoice> choices;
    for(const auto& manager:state.world.managers){
        ui::ManagerChoice choice;choice.slot=int(choices.size());choice.name=ui::utf8_to_original(manager.name);choice.master_club=-1;choice.world_club=manager.club;choice.country=state.world.clubs.at(manager.club).country;
        choice.start_league=state.world.catalogue.league(state.world.clubs.at(manager.club).league).local_index;choice.strength=1;choice.portrait=manager.portrait;choice.roster_revision=join_?join_->context_revision:owner_.revision;choices.push_back(std::move(choice));
    }return choices;
}
ui::SelectionContext Session::begin_manager_join(const ui::DeskRequest& request,const std::filesystem::path& data) {
    validate_file_owner(request.manager_slot,request.revision);
    if(join_||preparation_menu_||request.kind!=ui::DeskRequestKind::open_menu||request.menu_kind!=29||!state_.world.catalogue.scope.club_play()||national_roles_active(state_.world)||state_.world.managers.size()>=4||!state_.desktop->initial_event||state_.desktop->initial_event->offer||state_.desktop->autosave_pending)
        throw FormatError("world12: another event, offer or full roster owns the manager entry");
    const auto desk=ui::world12_initial_desktop(data,state_,resources_.reference,resources_.german,options_,owner_.revision);
    if(!visible_world12_menu(desk,request.item_id,29))throw FormatError("world12: new-manager control is not currently available");
    auto draft=std::make_shared<ManagerJoinDraft>();draft->source=state_;draft->candidate=state_;draft->options=options_;draft->origin=request;draft->context_revision=owner_.revision;join_=std::move(draft);preparation_menu_=29;
    return manager_join_context();
}
ui::SelectionContext Session::manager_join_context()const {
    if(!join_||preparation_menu_!=29||state_!=join_->source)throw FormatError("world12: manager entry lost its accepted source");
    return ui::world12_manager_selection_context(join_->candidate.world,join_->context_revision,true);
}
void Session::choose_join_options(const ui::SetupOptions& options) {
    (void)manager_join_context();if(join_->options_chosen)throw FormatError("world12: manager options already accepted");const auto& current=options_;
    if(options.managers<int(state_.world.managers.size())||options.managers>4||options.difficulty<0||options.difficulty>9||(current.difficulty==0&&options.difficulty!=0)||
       options.mode!=current.mode||options.duration!=current.duration||options.country_selection!=current.country_selection||options.germany!=current.germany||options.england!=current.england||options.league!=current.league||options.world!=current.world)
        throw FormatError("world12: running manager options changed locked career settings");
    auto draft=std::make_shared<ManagerJoinDraft>(*join_);draft->options=options;draft->options_chosen=true;draft->candidate.world.settings->planned_managers=uint8_t(options.managers);draft->candidate.world.settings->difficulty=uint8_t(options.difficulty);
    const auto signature=uint16_t(options.mode+options.duration*4+options.difficulty*64);
    if(draft->candidate.world.highscore_signature&&*draft->candidate.world.highscore_signature!=signature)draft->candidate.world.highscore_signature=0;
    draft->candidate.validate();join_=std::move(draft);
}
void Session::confirm_join_manager(const ui::ManagerChoice& choice) {
    (void)manager_join_context();if(!join_->options_chosen)throw FormatError("world12: manager options not confirmed");const auto& world=join_->candidate.world;
    if(choice.slot!=int(world.managers.size())||choice.roster_revision!=join_->context_revision||choice.master_club!=-1||!choice.world_club.valid()||!world.clubs.contains(choice.world_club)||choice.strength!=1||world.clubs.at(choice.world_club).country!=choice.country||
       world.catalogue.league(world.clubs.at(choice.world_club).league).local_index!=choice.start_league)
        throw FormatError("world12: stale or altered running club choice");
    uint8_t seat=0;while(seat<4&&std::any_of(world.managers.begin(),world.managers.end(),[&](const auto& m){return m.seat==seat;}))++seat;
    if(seat==4||join_->context_revision==UINT64_MAX)throw FormatError("world12: no canonical player seat remains");
    auto draft=std::make_shared<ManagerJoinDraft>(*join_);
    const InitialManagerChoice request{seat,choice.world_club,world.clubs.at(choice.world_club).league,ui::original_to_utf8(choice.name),uint8_t(choice.portrait),1};
    add_running_manager(draft->candidate,request,resources_.reference,choice.country==Country::germany?resources_.german:resources_.english);++draft->context_revision;join_=std::move(draft);
}
bool Session::manager_join_complete()const {return join_&&join_->options_chosen&&join_->candidate.world.managers.size()==size_t(join_->options.managers);}
void Session::commit_manager_join() {
    (void)manager_join_context();if(!manager_join_complete()||owner_.revision==UINT64_MAX)throw FormatError("world12: manager selection is not complete");
    auto candidate=join_->candidate;candidate.validate();state_=std::move(candidate);options_=join_->options;join_.reset();preparation_menu_.reset();++owner_.revision;
}
void Session::cancel_manager_join(){(void)manager_join_context();join_.reset();preparation_menu_.reset();}
}
