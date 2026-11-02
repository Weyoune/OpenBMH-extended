// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "ui/process_timer.hpp"
#include <optional>
namespace bmh::app::world12 {
struct DesktopClockContext {
    uint64_t scope=0,revision=0;
    uint32_t date=0;
    uint16_t event=0;
    uint8_t seat=0,minutes=0;
    bool operator==(const DesktopClockContext&)const=default;
};
// The host owns monotonic time; the career supplies only its authenticated
// desk identity and saved H8. A staged Session copy is still the same desk.
class DesktopProcessTimer {
public:
    explicit DesktopProcessTimer(ui::ProcessTimer& clock):clock_(clock){}
    void reset(){clock_.stop();owner_.reset();attempted_revision_.reset();}
    void observe(const std::optional<DesktopClockContext>& next,bool suspended,uint64_t now) {
        if(!next){reset();return;}
        const bool same=owner_&&owner_->scope==next->scope&&owner_->date==next->date&&owner_->event==next->event&&owner_->seat==next->seat;
        if(!same){clock_.start(next->minutes,now);attempted_revision_.reset();}
        if(suspended)clock_.pause(now);
        else if(clock_.paused())clock_.resume(next->minutes,now);
        else if(same&&owner_->minutes!=next->minutes){clock_.pause(now);clock_.resume(next->minutes,now);}
        owner_=next;
        if(const auto sample=clock_.sample(now);sample&&!sample->expired)attempted_revision_.reset();
    }
    bool may_advance(uint64_t revision)const {
        return owner_&&owner_->revision==revision&&attempted_revision_!=revision;
    }
    void attempted_advance(uint64_t revision) {
        if(owner_&&owner_->revision==revision)attempted_revision_=revision;
    }
private:
    ui::ProcessTimer& clock_;
    std::optional<DesktopClockContext> owner_;
    std::optional<uint64_t> attempted_revision_;
};
}
