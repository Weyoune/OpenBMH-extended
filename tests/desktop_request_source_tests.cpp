// SPDX-License-Identifier: GPL-3.0-only
#include "app/desktop_request_source.hpp"
#include <iostream>
#include <stdexcept>
namespace {void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}}
int main()try {
    using namespace bmh;GameState game;game.manager_teams={0};game.manager_records.resize(0xb0c);game.manager_options={0,0xa0,0xb1,0xc0,0xd1};game.manager_records[0x218]=5;
    game.memory.add_block(layout::ActiveManager,1);ui::DeskState desk;desk.manager_slot=0;desk.runtime_club=0;desk.revision=7;
    desk.items={{1,20,20,173,40,64,0,0,true,false},{2,20,65,100,54,55,0,0,true,false}};
    ui::DeskRequest request;request.kind=ui::DeskRequestKind::change_option;request.manager_slot=0;request.item_id=1;request.menu_kind=64;request.value=1;request.revision=7;uint64_t revision=7;std::string error;
    check(app::apply_desk_widget_source(game,desk,request,revision,error)&&game.manager_options[1]==0xa1&&revision==8,"automation changed high bits or wrong byte");
    desk.revision=request.revision=revision;request.value=0;
    for(int expected:{1,3,0}){check(app::apply_desk_widget_source(game,desk,request,revision,error)&&game.manager_options[0]==expected,"automatic formation ignored the saved mask");desk.revision=request.revision=revision;}
    const auto before=game.manager_options;auto invalid=request;invalid.item_id=2;
    check(!app::apply_desk_widget_source(game,desk,invalid,revision,error)&&game.manager_options==before,"wrong visible drawer accepted an automation mutation");
    request.kind=ui::DeskRequestKind::change_ticket_price;request.item_id=2;request.menu_kind=55;request.value=12;
    check(app::apply_desk_widget_source(game,desk,request,revision,error)&&game.manager_records[0x26e]==12,"ticket scale did not update its Original field");
    desk.revision=request.revision=revision;request.value=26;check(!app::apply_desk_widget_source(game,desk,request,revision,error)&&game.manager_records[0x26e]==12,"out-of-range price changed ticket state");
    game.memory.add_block(layout::Desktops,54*16);game.memory.add_block(layout::CareerSession_DispatchDesktopLayout_RuntimeField,1);game.memory.add_block(layout::CareerSession_DispatchDesktopLayout_RuntimeField2,1);
    std::array<uint8_t,54*16> original{};original[5]=1;original[16]=20;original[18]=20;original[19]=32;original[20]=25;original[28]=19;
    for(size_t i=0;i<original.size();++i)game.memory.set8(layout::Desktops+uint32_t(i),original[i]);
    request={};request.kind=ui::DeskRequestKind::change_layout;request.manager_slot=0;request.item_id=1;request.menu_kind=19;request.revision=revision;request.layout_operation=ui::DeskLayoutOperation::move_item;request.layout_before=original;request.layout_after=original;request.layout_after[16]=25;request.layout_after[18]=30;
    const auto project=[](const GameState& state,uint64_t rev){ui::DeskState out;out.manager_slot=0;out.revision=rev;for(size_t row=0;row<54;++row)for(size_t b=0;b<16;++b)out.layout_records[row][b]=state.memory.u8(layout::Desktops+uint32_t(row*16+b));return out;};
    check(app::apply_desktop_layout_source(game,request,revision,error,project)&&game.memory.u16(layout::Desktops+16)==25&&game.memory.u8(layout::Desktops+18)==30,"Source move did not preserve its accepted coordinates");
    const auto done=revision;request.revision=revision;request.layout_before=request.layout_after;request.layout_after[28]^=1;
    check(!app::apply_desktop_layout_source(game,request,revision,error,project)&&revision==done&&game.memory.u8(layout::Desktops+28)==19,"unowned layout-byte change accepted");
    std::cout<<"Shared desktop formation/Boolean controls, ticket limits and owned layout mutations passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
