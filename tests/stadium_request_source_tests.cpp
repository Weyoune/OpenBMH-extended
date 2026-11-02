// SPDX-License-Identifier: GPL-3.0-only
#include "app/stadium_request_source.hpp"
#include <iostream>
#include <stdexcept>
namespace {void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}}
int main()try {
    using namespace bmh;GameState state;state.level=3;state.manager_teams={0};state.club_ids={0};state.manager_records.resize(0xb0c);
    state.memory.add_block(layout::ActiveManager,1);state.memory.add_block(layout::GameMode,1);state.memory.set8(layout::GameMode,3);
    state.memory.add_block(layout::CareerSession_Invalid_RuntimeField2,4);state.memory.set32(layout::CareerSession_Invalid_RuntimeField2,17);
    auto rules=LeImage::from_bytes(layout::GermanDataSegmentBegin,Bytes(0x6000));
    GameRandom game(19);WatcomRandom random(71);app::StadiumSourceRuntime runtime;uint64_t next=1,revision=5;std::string error;
    ui::StadiumRequest request;request.kind=ui::StadiumRequestKind::open;request.manager_slot=request.runtime_club=0;request.stable_club_identity=0;request.revision=revision;
    auto expected=game;const auto discount=construction_price_percent(state,0,expected);
    check(app::apply_stadium_request_source(state,rules,game,random,runtime,next,request,revision,error)&&runtime.opened&&runtime.scope==1&&runtime.sequence==1&&runtime.price_percent==discount&&revision==6&&game.seed()==expected.seed()&&random.calls()==0,"shared stadium opening lost Core discount, owner or RNG");
    const auto unchanged=state.manager_records;const auto seed=game.seed();check(!app::apply_stadium_request_source(state,rules,game,random,runtime,next,request,revision,error)&&state.manager_records==unchanged&&game.seed()==seed&&revision==6,"stale stadium opening mutated state");
    request.kind=ui::StadiumRequestKind::quote_periphery;request.scope=runtime.scope;request.sequence=runtime.sequence;request.revision=revision;request.object=6;
    expected=game;const auto price=periphery_offer(state,6,expected);
    check(app::apply_stadium_request_source(state,rules,game,random,runtime,next,request,revision,error)&&runtime.periphery_quote==std::make_pair(6,price)&&game.seed()==expected.seed()&&random.calls()==0,"shared periphery quote differs from Core or draws W");
    const auto put=[&](size_t at,uint32_t value){for(size_t n=0;n<4;++n)state.manager_records[at+n]=uint8_t(value>>(8*n));};put(0x314,2000000);state.manager_records[0x171]=0x20;
    request.kind=ui::StadiumRequestKind::build_periphery;request.price=price;request.sequence=runtime.sequence;request.revision=revision;
    const auto before=state.manager_records;const auto before_revision=revision,before_sequence=runtime.sequence;const auto before_random=random;
    bool interrupted=false;int shown=0;try{app::apply_stadium_request_source(state,rules,game,random,runtime,next,request,revision,error,[&](const GameState&,const ConstructionNotice&){++shown;throw std::runtime_error("cancelled notice");});}catch(const std::runtime_error&){interrupted=true;}
    check(interrupted&&shown==1&&state.manager_records==before&&revision==before_revision&&runtime.sequence==before_sequence&&runtime.periphery_quote==std::make_pair(6,price)&&random.seed()==before_random.seed()&&random.calls()==before_random.calls(),"notice exception leaked subsidy, money, quote owner or W draw");
    auto oracle=state;auto expected_random=random;check(build_periphery(oracle,0,6,uint32_t(price),expected_random),"synthetic periphery Core build unavailable");
    check(app::apply_stadium_request_source(state,rules,game,random,runtime,next,request,revision,error)&&state.manager_records==oracle.manager_records&&random.seed()==expected_random.seed()&&random.calls()==expected_random.calls()&&!runtime.periphery_quote,"shared periphery cost, subsidy, construction or RNG differ from Core");
    request={};request.kind=ui::StadiumRequestKind::close;request.manager_slot=request.runtime_club=0;request.stable_club_identity=0;request.scope=runtime.scope;request.sequence=runtime.sequence;request.revision=revision;
    check(app::apply_stadium_request_source(state,rules,game,random,runtime,next,request,revision,error)&&!runtime.opened,"owned stadium close rejected");
    request.kind=ui::StadiumRequestKind::open;request.scope=request.sequence=0;request.revision=revision;
    check(app::apply_stadium_request_source(state,rules,game,random,runtime,next,request,revision,error)&&runtime.scope==2,"new stadium opening reused old window token");
    std::cout<<"Shared stadium discount, periphery quotation/build/subsidy, cancelled-notice rollback and window scopes passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
