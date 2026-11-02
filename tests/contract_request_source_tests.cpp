// SPDX-License-Identifier: GPL-3.0-only
#include "app/contract_request_source.hpp"
#include "app/contract_source_types.hpp"
#include <iostream>
#include <stdexcept>
namespace {
using namespace bmh;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
}
int main()try {
    using namespace bmh;GameState g;g.level=3;g.manager_teams={0};g.manager_records.resize(0xb0c);g.tactics.resize(0x840);
    g.league_rounds.assign(9,0);g.squad_slots.assign(size_t(runtime_dimensions.total_slots),0xfff);g.squad_slots[0]=0x8000;
    g.players.resize(1);g.players[0].name="Synthetic";g.players[0].w35c=60u|(70u<<7)|(75u<<14);g.players[0].w364=5u<<21;
    g.memory.add_block(layout::ActiveManager,1);g.memory.add_block(layout::GameMode,1);g.memory.set8(layout::GameMode,3);
    g.tactics[0xc]=12;g.tactics[0xd]=100;g.tactics[0x11]=12;g.tactics[0x50]=0xa0;g.tactics[0x51]=0x0f;
    TextDb texts;texts.strings.assign(TEXT_COUNT,"Synthetic contract result");
    WatcomRandom random(91);uint64_t revision=7,next=1,last=0;std::optional<app::ContractSourceQuote> quote;
    ui::ContractActionResult result;std::string error;
    const auto unchanged=[](const GameState& a,const GameState& b){return a.manager_records==b.manager_records&&a.tactics==b.tactics&&a.squad_slots==b.squad_slots;};
    ui::ContractRequest request;request.manager_slot=0;request.runtime_club=0;request.row=0;request.player_id=0;request.request_id=1;request.revision=7;
    auto oracle=g;auto expected=random;const auto demands=contract_demands(oracle,0,0,true,expected);
    check(app::apply_contract_request_source(g,texts,random,request,revision,quote,next,last,result,error,unchanged)&&
          result.demands==demands&&quote&&quote->quote_id==1&&revision==8&&random.seed()==expected.seed()&&g.tactics==oracle.tactics,
          "shared contract demands lost Core amounts, quote owner or random stream");
    auto forged=request;forged.kind=ui::ContractActionKind::submit_offer;forged.request_id=2;forged.revision=revision;forged.quote_id=99;forged.demand=demands[0];
    const auto seed=random.seed();const auto calls=random.calls();
    check(!app::apply_contract_request_source(g,texts,random,forged,revision,quote,next,last,result,error,unchanged)&&
          revision==8&&random.seed()==seed&&random.calls()==calls,"unbound quote consumed contract state or RNG");
    request.kind=ui::ContractActionKind::insure_player;request.request_id=2;request.revision=revision;request.quote_id=quote->quote_id;request.insurance_months=12;
    oracle=g;check(contract_insurance(oracle,0,0,0,12).accepted,"synthetic Core insurance unavailable");
    check(app::apply_contract_request_source(g,texts,random,request,revision,quote,next,last,result,error,unchanged)&&
          g.tactics==oracle.tactics&&quote&&quote->revision==revision&&random.seed()==seed,"shared insurance lost Core fee, quote or RNG neutrality");
    request.kind=ui::ContractActionKind::submit_offer;request.request_id=3;request.revision=revision;request.insurance_months=0;
    request.demand=demands[0];request.monthly_salary=std::min<uint32_t>(200000,std::max<uint32_t>(1000,demands[0]*2));
    oracle=g;expected=random;const auto accepted=contract_offer(oracle,0,0,0,request.monthly_salary,request.demand,false,false,expected,&request.proposed_bonuses);
    check(app::apply_contract_request_source(g,texts,random,request,revision,quote,next,last,result,error,unchanged)&&
          result.accepted==accepted&&g.tactics==oracle.tactics&&random.seed()==expected.seed()&&!quote,
          "shared salary offer lost Core acceptance, loyalty, board or random stream");
    request={};request.kind=ui::ContractActionKind::submit_bonuses;request.request_id=4;request.revision=revision;request.manager_slot=0;request.runtime_club=0;request.proposed_bonuses.fill(1);
    oracle=g;const auto bonuses=contract_bonuses(oracle,0,-1,request.proposed_bonuses);
    check(app::apply_contract_request_source(g,texts,random,request,revision,quote,next,last,result,error,unchanged)&&
          result.accepted==bonuses&&g.manager_records==oracle.manager_records&&g.tactics==oracle.tactics,
          "shared team bonuses differ from Core");
    const auto done=revision;check(!app::apply_contract_request_source(g,texts,random,request,revision,quote,next,last,result,error,unchanged)&&revision==done,
          "accepted contract request replayed");
    std::cout<<"Shared Core contract demands/offers, insurance, team premiums, quote/revision ownership and RNG passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
