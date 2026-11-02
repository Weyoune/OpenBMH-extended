// SPDX-License-Identifier: GPL-3.0-only
#include "app/world12_session.hpp"
#include "app/advertising_request_source.hpp"
#include "game/world12/desktop_projection.hpp"
#include "game/world12/economy.hpp"
#include "formats/textd.hpp"
#include <algorithm>
#include <limits>
namespace bmh::app::world12 {
using namespace bmh::world12;
namespace {
ui::AdvertisingState model(const CareerSnapshot& snapshot,const GameState& reference,
    uint64_t revision,uint64_t next,const std::filesystem::path& data) {
    if(!snapshot.world.economy||!snapshot.world.economy->advertising_group)throw FormatError("world12: advertising group counter is unknown in this older career");
    auto p=project_manager_desktop(snapshot,reference);const auto texts=load_textd(ui::asset_path(data,"TEXTD.BLK"));p.state.texts=&texts;
    auto out=ui::advertising_from_game(p.state,p.state.memory.u8(layout::ActiveManager),revision);out.request_id_base=next;
    const auto seat=*snapshot.desktop->active_seat;
    const auto& manager=*std::find_if(snapshot.world.managers.begin(),snapshot.world.managers.end(),[&](const auto& m){return m.seat==seat;});
    const auto& crest=snapshot.world.crests.at(snapshot.world.clubs.at(manager.club).crest);out.badge_sprite=crest.cell;out.badge_atlas=crest.path;return out;
}
}
ui::AdvertisingState Session::open_advertising(const ui::DeskRequest& request,const std::filesystem::path& data) {
    validate_file_owner(request.manager_slot,request.revision);
    if(request.kind!=ui::DeskRequestKind::open_menu||request.menu_kind!=21||advertising_generation_>=UINT32_MAX)
        throw FormatError("world12: advertising has no idle menu owner");
    const auto next=(advertising_generation_+1)<<32;auto out=model(state_,resources_.reference,owner_.revision,next,data);
    ++advertising_generation_;advertising_request_=next-1;preparation_menu_=21;return out;
}
ui::AdvertisingState Session::advertising_view(const std::filesystem::path& data)const {
    if(preparation_menu_!=21||advertising_request_==UINT64_MAX)throw FormatError("world12: advertising menu is not open");
    return model(state_,resources_.reference,owner_.revision,advertising_request_+1,data);
}
AdvertisingSpendingResult Session::apply_advertising(const std::filesystem::path& data,
    const ui::AdvertisingRequest& request,const AdvertisingRenewalPresenter& presenter) {
    if(preparation_menu_!=21||request.revision!=owner_.revision||owner_.revision==UINT64_MAX||
       request.request_id<=advertising_request_||request.request_id==UINT64_MAX||(request.request_id>>32)!=advertising_generation_)
        throw FormatError("world12: stale advertising menu/request");
    const auto before=state_;const auto revision=owner_.revision,generation=advertising_generation_,last=advertising_request_;auto candidate=state_;
    auto p=project_manager_desktop(candidate,resources_.reference);const auto texts=load_textd(ui::asset_path(data,"TEXTD.BLK"));p.state.texts=&texts;
    WatcomRandom random(candidate.watcom_seed);AdvertisingSpendingResult result;std::string error;
    if(request.kind==ui::AdvertisingActionKind::check_renewal) {
        if(!presenter)throw FormatError("world12: advertising renewal requires its visible presenter");
        if(!validate_advertising_renewal_request_source(p.state,request,revision,last,error))throw FormatError(error);
        const auto renewal=check_advertising_renewal(p.state,request.manager_slot,request.renewal_display_slot,random);
        if(renewal.contract_slot!=request.renewal_contract_slot||renewal.status==AdvertisingRenewal::Status::invalid||renewal.status==AdvertisingRenewal::Status::not_expired)
            throw FormatError("world12: advertising renewal no longer applies");
        const auto answer=presenter(p.state,renewal);
        if(renewal.status==AdvertisingRenewal::Status::offered) {
            if((answer!=AdvertisingRenewalAnswer::renew&&answer!=AdvertisingRenewalAnswer::decline)||
               !answer_advertising_renewal(p.state,request.manager_slot,request.renewal_contract_slot,answer==AdvertisingRenewalAnswer::renew))
                throw FormatError("world12: advertising renewal was not confirmed");
        } else if(renewal.status!=AdvertisingRenewal::Status::declined_by_sponsor||answer!=AdvertisingRenewalAnswer::acknowledged)
            throw FormatError("world12: sponsor refusal was not acknowledged");
    } else {
        auto scratch=revision,request_id=last;
        if(!apply_advertising_request_source(p.state,random,request,scratch,request_id,error,&result))throw FormatError(error);
    }
    p.commit_match_state(candidate.world);capture_career_economy(candidate.world,p);candidate.watcom_seed=random.seed();candidate.validate();
    if(state_!=before||owner_.revision!=revision||preparation_menu_!=21||advertising_generation_!=generation||advertising_request_!=last)
        throw FormatError("world12: advertising owner changed during calculation/presentation");
    state_=std::move(candidate);++owner_.revision;advertising_request_=request.request_id;return result;
}
}
