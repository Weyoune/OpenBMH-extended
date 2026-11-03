// SPDX-License-Identifier: GPL-3.0-only
#include "app/world12_session.hpp"
#include "game/world12/desktop_projection.hpp"
#include "game/world12/economy.hpp"
#include "ui/mailbox_state.hpp"
#include "ui/desk_state.hpp"
#include "ui/desktop.hpp"
#include <algorithm>
#include <limits>
namespace bmh::app::world12 {
using namespace bmh::world12;
namespace {
int active(const AiMatchProjection& p,int actor,uint64_t requested,uint64_t revision){
    const auto local=p.state.memory.u8(layout::ActiveManager);
    if(actor<0||local>=p.human_managers()||p.global_manager(local)!=actor||requested!=revision)
        throw FormatError("world12: stale mailbox manager or revision");return local;
}
}
void Session::open_mailbox(const ui::DeskRequest& request){
    if(preparation_menu_||request.kind!=ui::DeskRequestKind::open_menu||request.menu_kind!=63||!state_.world.economy)
        throw FormatError("world12: mailbox has no available desktop owner");
    const auto p=project_manager_desktop(state_,resources_.reference);active(p,request.manager_slot,request.revision,owner_.revision);
    preparation_menu_=63;
}
ui::MailboxState Session::mailbox_view()const {
    if(!preparation_menu_||*preparation_menu_!=63)throw FormatError("world12: mailbox view is not owned");
    const auto p=project_manager_desktop(state_,resources_.reference);
    const auto local=p.state.memory.u8(layout::ActiveManager);auto view=ui::mailbox_state_from_game(p.state,local,owner_.revision);
    view.manager_slot=p.global_manager(local);const auto raw=career_mailbox_bytes(*state_.world.economy);
    for(auto& message:view.messages){const auto at=size_t(message.entry_index)*kMailboxEntry;
        std::copy_n(raw.begin()+ptrdiff_t(at),kMailboxEntry,message.raw_entry.begin());
        message.recipient_mask=le32(raw,at);message.kept_by_mask=le32(raw,at+4);
        message.kept_for_manager=(message.kept_by_mask&(1u<<view.manager_slot))!=0;
        message.general_broadcast=(message.recipient_mask&0x80000000u)!=0;message.recipient_managers.clear();
        for(const auto& person:state_.world.managers)if(message.recipient_mask&(1u<<person.seat))message.recipient_managers.push_back(person.seat);
    }return view;
}
void Session::edit_mailbox(const ui::MailboxActionRequest& request){
    if(!preparation_menu_||*preparation_menu_!=63||owner_.revision==std::numeric_limits<uint64_t>::max()||request.entry_index<0||request.entry_index>=40)
        throw FormatError("world12: unavailable mailbox action owner");
    const auto before=state_;const auto revision=owner_.revision;const auto p=project_manager_desktop(state_,resources_.reference);
    const auto local=active(p,request.manager_slot,request.revision,revision);const auto at=size_t(request.entry_index)*kMailboxEntry;
    const auto current=career_mailbox_bytes(*state_.world.economy);
    if(!std::equal(request.expected_entry.begin(),request.expected_entry.end(),current.begin()+ptrdiff_t(at)))throw FormatError("world12: message was changed");
    MailboxEditKind kind;if(request.kind==ui::MailboxActionKind::toggle_keep)kind=MailboxEditKind::toggle_keep;
    else if(request.kind==ui::MailboxActionKind::delete_for_manager)kind=MailboxEditKind::delete_for_manager;else throw FormatError("world12: unknown mailbox action");
    auto candidate=state_;auto& economy=*candidate.world.economy;const auto raw=career_mailbox_bytes(economy);std::array<uint8_t,kMailboxEntry> expected;std::copy_n(raw.begin()+ptrdiff_t(at),expected.size(),expected.begin());
    // Preserve Source's global-seat quota arithmetic, never a temporary local bit.
    switch(edit_career_message(economy,p.global_manager(uint8_t(local)),request.entry_index,expected,kind)){
    case MailboxEditFailure::none:break;
    case MailboxEditFailure::keep_limit:throw FormatError("Die Originalgrenze für gemerkte Nachrichten ist erreicht.");
    case MailboxEditFailure::kept:throw FormatError("Diese Nachricht ist noch gemerkt.");
    default:throw FormatError("world12: stale or unauthorized mailbox entry");
    }
    candidate.validate();if(state_!=before||owner_.revision!=revision)throw FormatError("world12: mailbox owner changed during edit");
    state_=std::move(candidate);++owner_.revision;
}
void Session::close_mailbox(const ui::MailboxCloseRequest& request){
    if(!preparation_menu_||*preparation_menu_!=63||!state_.world.economy)throw FormatError("world12: mailbox close is not owned");
    const auto p=project_manager_desktop(state_,resources_.reference);const auto actor=active(p,request.manager_slot,request.revision,owner_.revision);
    state_.world.economy->recipient_mask&=~(1u<<p.global_manager(uint8_t(actor)));preparation_menu_.reset();
}
}
