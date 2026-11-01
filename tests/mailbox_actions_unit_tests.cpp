// SPDX-License-Identifier: GPL-3.0-only
#include "game/messages.hpp"
#include <algorithm>
#include <iostream>
using namespace bmh;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void put(Bytes& b,size_t at,uint32_t value){for(size_t n=0;n<4;++n)b.at(at+n)=uint8_t(value>>(n*8));}
std::array<uint8_t,0xa0> entry(const Bytes& b,int row){std::array<uint8_t,0xa0> out;std::copy_n(b.begin()+row*0xa0,out.size(),out.begin());return out;}
}
int main()try{
    Bytes box(kMailboxEntries*kMailboxEntry);put(box,0,0x80000003u);box[10]='A';put(box,0xa0,2);box[0xa0+10]='B';
    const auto before=box;
    check(edit_mailbox_entry(box,0,0,entry(box,0),MailboxEditKind::toggle_keep)==MailboxEditFailure::none,"keep refused");
    const auto kept=box;check(edit_mailbox_entry(box,0,0,entry(box,0),MailboxEditKind::delete_for_manager)==MailboxEditFailure::kept&&box==kept,"kept deletion modified entry");
    check(edit_mailbox_entry(box,0,0,entry(before,0),MailboxEditKind::toggle_keep)==MailboxEditFailure::changed&&box==kept,"stale raw identity accepted");
    check(edit_mailbox_entry(box,0,0,entry(box,0),MailboxEditKind::toggle_keep)==MailboxEditFailure::none,"unkeep refused");
    check(edit_mailbox_entry(box,0,0,entry(box,0),MailboxEditKind::delete_for_manager)==MailboxEditFailure::none&&box[0]==2&&box[3]==128,"delete removed other recipient/broadcast");
    check(edit_mailbox_entry(box,1,0,entry(box,0),MailboxEditKind::delete_for_manager)==MailboxEditFailure::none&&box[0]==0&&box[3]==0&&box[10]=='A'&&box[0xa0]=='\2',"last-recipient deletion compacted or cleared raw text");
    check(edit_mailbox_entry(box,0,1,entry(box,1),MailboxEditKind::delete_for_manager)==MailboxEditFailure::not_recipient,"other actor deleted message");
    Bytes quota(kMailboxEntries*kMailboxEntry);for(int row=0;row<6;++row){put(quota,size_t(row)*0xa0,1);if(row<5)put(quota,size_t(row)*0xa0+4,1);}
    const auto full=quota;check(edit_mailbox_entry(quota,0,5,entry(quota,5),MailboxEditKind::toggle_keep)==MailboxEditFailure::keep_limit&&quota==full,"Source five-kept boundary changed");
    check(edit_mailbox_entry(quota,0,0,entry(quota,0),MailboxEditKind::toggle_keep)==MailboxEditFailure::keep_limit&&quota==full,"Source blocked-unkeep behavior changed");
    for(int row=0;row<6;++row){put(quota,size_t(row)*0xa0,2);put(quota,size_t(row)*0xa0+4,2);}
    check(edit_mailbox_entry(quota,1,5,entry(quota,5),MailboxEditKind::toggle_keep)==MailboxEditFailure::none,"Source manager-one recipient+1 exception lost");
    const auto valid=quota;check(edit_mailbox_entry(quota,4,0,entry(quota,0),MailboxEditKind::delete_for_manager)==MailboxEditFailure::invalid_actor&&quota==valid,"invalid actor modified mailbox");
    std::cout<<"Mailbox raw guards, shared recipients, physical holes, kept deletion and Source per-seat quota passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
