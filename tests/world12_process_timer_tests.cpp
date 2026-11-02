// SPDX-License-Identifier: GPL-3.0-only
#include "app/world12_process_timer.hpp"
#include "formats/executable_layout.hpp"
#include <iostream>
#include <stdexcept>
using namespace bmh;
namespace {void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}}
int main()try {
    ui::ProcessTimer clock;app::world12::DesktopProcessTimer host(clock);
    app::world12::DesktopClockContext desk{71,9,10,111,1,1};
    host.observe(desk,false,10);check(clock.sample(10)->remaining_seconds==60,"new desk did not receive its Source minute");
    ++desk.revision;host.observe(desk,false,30);check(clock.sample(30)->remaining_seconds==40,"staged copy/revision reset the running desk budget");
    host.observe(desk,true,30);desk.minutes=3;host.observe(desk,true,900);desk.minutes=2;host.observe(desk,true,1000);
    check(!clock.sample(1000),"settings/file pause consumed or displayed time");
    host.observe(desk,false,1000);check(clock.sample(1000)==ui::ProcessTimer::Sample{100,83,false},"settings close lost elapsed time or applied intermediate H8 edits");
    host.observe(desk,false,1001);check(clock.sample(1001)->remaining_seconds==99,"ordinary menu observation reset the timer");
    desk.seat=3;host.observe(desk,false,1002);check(clock.sample(1002)->remaining_seconds==120,"canonical actor change did not start a fresh budget");
    ++desk.event;host.observe(desk,false,1010);check(clock.sample(1010)->remaining_seconds==120,"new event reused an old desk budget");
    ++desk.date;host.observe(desk,false,1020);check(clock.sample(1020)->remaining_seconds==120,"new date reused an old desk budget");
    ++desk.scope;host.observe(desk,false,1030);check(clock.sample(1030)->remaining_seconds==120,"another session scope inherited the timer");
    host.observe(desk,false,1150);check(clock.sample(1150)->expired&&host.may_advance(desk.revision),"expired desk did not offer its owned transition");
    host.attempted_advance(desk.revision);
    for(uint64_t now=1151;now<1250;++now){host.observe(desk,false,now);check(!host.may_advance(desk.revision),"failed transition repeated on an unchanged owner revision");}
    ++desk.revision;host.observe(desk,false,1250);check(clock.sample(1250)->expired&&host.may_advance(desk.revision),"new revision silently restarted time or retained an old rejected command");
    host.reset();host.observe(desk,false,2000);check(clock.sample(2000)->remaining_seconds==120,"successful reload did not reset runtime time");
    host.observe(desk,false,2020);host.observe(desk,true,2020);desk.minutes=0;host.observe(desk,false,9000);check(!clock.sample(9001),"H8 zero did not disable the process clock");
    host.observe(desk,true,9001);desk.minutes=1;host.observe(desk,false,10000);check(clock.sample(10000)->remaining_seconds==60,"enabling a previously zero clock inherited stale time");
    desk.minutes=200;host.observe(desk,false,10001);check(!clock.sample(11000),"Original blank numeric gate was treated as an active budget");
    desk.minutes=1;host.observe(desk,false,11000);check(clock.sample(11000)->remaining_seconds==59,"nonzero blank H8 gate lost the elapsed Source second");
    auto rules=LeImage::from_bytes(layout::ProcessTimer_FormatError_RuntimeField,Bytes{11,22,33});
    for(unsigned percent:{0u,1u,33u,34u,67u,68u,99u,100u}){
        Image image(320,240,77);ui::ProcessTimer::paint(image,{1,percent,false},rules);
        for(unsigned x=0;x<320;++x)check(image.at(x,0)==(x<=percent*319/100?uint8_t(11+11*(percent/34)):0x44),"Source percentage row/colour band changed");
        for(size_t i=320;i<image.pixels.size();++i)check(image.pixels[i]==77,"process clock changed pixels below the top row");
    }
    host.observe(std::nullopt,false,12000);check(!clock.sample(12000)&&!host.may_advance(desk.revision),"missing/unknown saved desk borrowed a runtime timer");
    std::cout<<"Owned desktop time: revisions, canonical actors, event/date/scope, pause/H8 deltas, reload, expiry latch and Source raster passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
