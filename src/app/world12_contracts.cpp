// SPDX-License-Identifier: GPL-3.0-only
#include "app/world12_session.hpp"
#include "app/contract_request_source.hpp"
#include "game/world12/desktop_projection.hpp"
#include "game/world12/economy.hpp"
#include "formats/textd.hpp"
#include <algorithm>
#include <limits>
namespace bmh::app::world12 {
using namespace bmh::world12;
namespace {
ui::ContractsState model(const CareerSnapshot& snapshot,const GameState& reference,uint64_t revision,
    uint64_t base,const std::filesystem::path& data) {
    auto p=project_manager_desktop(snapshot,reference);const auto texts=load_textd(ui::asset_path(data,"TEXTD.BLK"));p.state.texts=&texts;
    auto out=ui::contracts_from_game(p.state,p.state.memory.u8(layout::ActiveManager),revision);out.request_id_base=base;
    const auto seat=*snapshot.desktop->active_seat;
    const auto& manager=*std::find_if(snapshot.world.managers.begin(),snapshot.world.managers.end(),[&](const auto& m){return m.seat==seat;});
    const auto& crest=snapshot.world.crests.at(snapshot.world.clubs.at(manager.club).crest);out.badge_sprite=crest.cell;out.badge_atlas=crest.path;return out;
}
}
ui::ContractsState Session::open_contracts(const ui::DeskRequest& request,const std::filesystem::path& data) {
    validate_file_owner(request.manager_slot,request.revision);
    if(request.kind!=ui::DeskRequestKind::open_menu||request.menu_kind!=27||contract_generation_>=UINT32_MAX)
        throw FormatError("world12: contracts have no current menu owner");
    auto out=model(state_,resources_.reference,owner_.revision,(contract_generation_+1)<<32,data);
    ++contract_generation_;contract_request_=out.request_id_base;contract_quote_.reset();contract_player_={};contract_club_={};contract_seat_.reset();preparation_menu_=27;return out;
}
ui::ContractsState Session::contracts_view(const std::filesystem::path& data)const {
    if(preparation_menu_!=27)throw FormatError("world12: contracts menu is not open");
    auto out=model(state_,resources_.reference,owner_.revision,contract_generation_<<32,data);
    if(contract_quote_){out.selected_row=out.quote_row=contract_quote_->row;out.quote_id=contract_quote_->quote_id;out.demands=contract_quote_->demands;}
    return out;
}
ui::ContractActionResult Session::apply_contract(const std::filesystem::path& data,const ui::ContractRequest& request) {
    if(preparation_menu_!=27||request.revision!=owner_.revision||request.request_id<=contract_request_||
       (request.request_id>>32)!=contract_generation_||owner_.revision==UINT64_MAX||next_contract_quote_==UINT64_MAX)
        throw FormatError("world12: stale contract menu/request");
    const auto before=state_;const auto revision=owner_.revision;const auto old_quote=contract_quote_;
    const auto old_generation=contract_generation_,old_request=contract_request_,old_next=next_contract_quote_;
    const auto old_player=contract_player_;const auto old_club=contract_club_;const auto old_seat=contract_seat_;auto candidate=state_;
    auto p=project_manager_desktop(candidate,resources_.reference);auto quote=contract_quote_;
    const auto texts=load_textd(ui::asset_path(data,"TEXTD.BLK"));p.state.texts=&texts;
    if(quote){
        if(!contract_seat_||contract_seat_!=candidate.desktop->active_seat||!contract_club_.valid()||!contract_player_.valid()||
           quote->row<0||quote->row>=24||candidate.world.clubs.at(contract_club_).roster[size_t(quote->row)].player!=contract_player_)
            throw FormatError("world12: contract quote lost its canonical player/club owner");
        quote->player_id=int(p.local_player(contract_player_));quote->runtime_club=int(p.local_club(contract_club_));
        size_t manager=0;while(manager<p.human_managers()&&p.global_manager(uint8_t(manager))!=*contract_seat_)++manager;
        if(manager==p.human_managers())throw FormatError("world12: contract quote manager is unavailable");quote->manager_slot=int(manager);
    }
    auto same_owned_state=[&](const GameState& a,const GameState& b){
        auto pa=p;pa.state=a;auto left=before.world;pa.commit_match_state(left);capture_career_economy(left,pa);
        auto pb=p;pb.state=b;auto right=before.world;pb.commit_match_state(right);capture_career_economy(right,pb);
        return left==right&&a.manager_records==b.manager_records&&a.tactics==b.tactics;
    };
    WatcomRandom random(candidate.watcom_seed);auto scratch=revision,next=next_contract_quote_,last=contract_request_;
    ui::ContractActionResult result;std::string error;
    if(!apply_contract_request_source(p.state,texts,random,request,scratch,quote,next,last,result,error,same_owned_state))
        throw FormatError(error.empty()?"world12: contract request rejected":error);
    if(request.kind!=ui::ContractActionKind::request_demands){p.commit_match_state(candidate.world);capture_career_economy(candidate.world,p);}
    candidate.watcom_seed=random.seed();candidate.validate();
    PlayerId player;ClubId club;std::optional<uint8_t> seat;
    if(quote){player=p.global_player(uint32_t(quote->player_id));club=p.global_club(uint32_t(quote->runtime_club));seat=p.global_manager(uint8_t(quote->manager_slot));}
    if(state_!=before||owner_.revision!=revision||preparation_menu_!=27||contract_generation_!=old_generation||contract_quote_!=old_quote||
       contract_request_!=old_request||next_contract_quote_!=old_next||contract_player_!=old_player||contract_club_!=old_club||contract_seat_!=old_seat)
        throw FormatError("world12: contract owner changed during calculation");
    state_=std::move(candidate);++owner_.revision;contract_quote_=std::move(quote);contract_request_=last;next_contract_quote_=next;
    contract_player_=player;contract_club_=club;contract_seat_=seat;return result;
}
void Session::close_contracts() {
    if(preparation_menu_!=27)throw FormatError("world12: no contract menu to close");
    contract_quote_.reset();contract_player_={};contract_club_={};contract_seat_.reset();preparation_menu_.reset();
}
}
