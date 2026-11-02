// SPDX-License-Identifier: GPL-3.0-only
#include "app/training_camp_request_source.hpp"
#include <iostream>
#include <stdexcept>
namespace {
using namespace bmh;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
}
int main()try {
    using namespace bmh;GameState g;g.level=5;g.manager_teams={0,1};g.manager_records.resize(2*0xb0c);g.tactics.resize(2*0x840);
    g.team_morale.assign(445,25);g.players.resize(40);g.squad_slots.assign(size_t(runtime_dimensions.total_slots),0xfff);
    for(size_t team=0;team<2;++team)for(size_t row=0;row<20;++row){const auto id=team*20+row;g.squad_slots[team*24+row]=uint16_t(id);g.players[id].w35c=60u|(70u<<7)|(75u<<14);g.players[id].w364=uint32_t(10u<<21|row%4);}
    g.memory.add_block(layout::ActiveManager,1);g.memory.set8(layout::ActiveManager,1);g.manager_records[0xb0c+0x290]=1;
    const uint32_t balance=5000000;for(size_t b=0;b<4;++b)g.manager_records[0xb0c+0x314+b]=uint8_t(balance>>(b*8));
    g.training_camp_base_prices.fill(10000);for(auto& grades:g.training_camp_quality)grades={3,4,5,3};
    ui::TrainingCampRequest request;request.kind=ui::TrainingCampRequestKind::set_focus;request.manager_slot=1;request.runtime_club=1;request.revision=7;request.focus={7,5,3,10};
    uint64_t revision=7;WatcomRandom random(91);std::string error;auto oracle=g;
    check(set_training_camp_focus(oracle,1,request.focus),"synthetic Core focus unavailable");
    check(app::apply_training_camp_request_source(g,random,request,revision,error)&&g.manager_records==oracle.manager_records&&revision==8&&!random.calls(),
          "shared camp focus differs from Core or draws RNG");
    auto invalid=request;invalid.revision=revision;invalid.focus[3]=9;const auto before=g.manager_records;
    check(!app::apply_training_camp_request_source(g,random,invalid,revision,error)&&g.manager_records==before&&revision==8&&!random.calls(),
          "invalid derived fourth slider changed camp state");
    request.kind=ui::TrainingCampRequestKind::book;request.revision=revision;request.resort=0;oracle=g;auto expected=random;
    check(book_training_camp(oracle,1,0,expected),"synthetic Core booking unavailable");
    check(app::apply_training_camp_request_source(g,random,request,revision,error)&&g.manager_records==oracle.manager_records&&g.tactics==oracle.tactics&&
          g.team_morale==oracle.team_morale&&random.seed()==expected.seed()&&random.calls()==expected.calls(),"shared camp booking lost Source cash, morale, board or random stream");
    for(size_t i=0;i<g.players.size();++i)check(g.players[i].w35c==oracle.players[i].w35c&&g.players[i].w364==oracle.players[i].w364&&g.players[i].w368==oracle.players[i].w368,"shared camp player effects differ from Core");
    const auto accepted=g.manager_records;const auto seed=random.seed();const auto calls=random.calls();request.revision=revision;
    check(!app::apply_training_camp_request_source(g,random,request,revision,error)&&error=="booked_this_week"&&g.manager_records==accepted&&random.seed()==seed&&random.calls()==calls,
          "duplicate camp booking changed money, visits or RNG");
    g.manager_records[0xb0c+0x21f]=3;
    check(!app::apply_training_camp_request_source(g,random,request,revision,error)&&error=="visit_limit"&&random.seed()==seed,"camp visit limit was bypassed");
    std::cout<<"Shared Original camp focus, immediate player/morale/board effects, fees, visit limits, actor and RNG passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
