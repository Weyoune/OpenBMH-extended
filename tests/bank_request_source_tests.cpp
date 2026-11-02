// SPDX-License-Identifier: GPL-3.0-only
#include "app/bank_request_source.hpp"
#include <iostream>
#include <stdexcept>
namespace {
using namespace bmh;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void put(Bytes& bytes,size_t at,uint32_t value){for(size_t b=0;b<4;++b)bytes.at(at+b)=uint8_t(value>>(b*8));}
}
int main()try {
    using namespace bmh;GameState g;g.level=5;g.manager_teams={0,1};g.manager_records.resize(2*0xb0c);g.stocks.resize(12*0x5c);
    g.memory.add_block(layout::ActiveManager,1);g.memory.set8(layout::ActiveManager,1);
    g.memory.add_block(layout::ManagerCount,1);g.memory.set8(layout::ManagerCount,2);
    g.memory.add_block(layout::FundBlock,6*0x1c);g.memory.set32(layout::FundBlock,10000);
    put(g.manager_records,0xb0c+0x314,5000000);put(g.manager_records,0xb0c+0x318,100000);put(g.manager_records,0xb0c+0x324,5000000);
    put(g.stocks,0,100000);g.stocks[0x4e]=0xe8;g.stocks[0x4f]=3;
    const auto exe=LeImage::from_bytes(layout::GermanDataSegmentBegin,Bytes(0x6000));
    GameRandom game(17);WatcomRandom random(91);uint64_t revision=7;std::string error;app::BankSourcePresentation notices;
    ui::BankRequest request;request.manager_slot=1;request.runtime_club=1;request.revision=revision;
    request.kind=ui::BankRequestKind::move_to_deposit;request.amount=10000;
    auto oracle=g;move_to_deposit(oracle,1,10000);
    check(app::apply_bank_request_source(g,exe,game,random,request,revision,notices,error)&&
          g.manager_records==oracle.manager_records&&revision==8&&!game.calls()&&!random.calls(),"shared deposit differs from Core or consumes RNG");
    const auto before=g;auto stale=request;stale.manager_slot=0;
    check(!app::apply_bank_request_source(g,exe,game,random,stale,revision,notices,error)&&g.manager_records==before.manager_records&&revision==8,
          "stale bank actor/revision changed cash");
    request.revision=revision;request.amount=std::numeric_limits<int32_t>::min();
    check(!app::apply_bank_request_source(g,exe,game,random,request,revision,notices,error)&&g.manager_records==before.manager_records,
          "extreme negative deposit bypassed its amount limit");
    request={};request.manager_slot=1;request.runtime_club=1;request.revision=revision;request.kind=ui::BankRequestKind::buy_shares;request.asset_index=0;request.count=10;
    oracle=g;check(buy_shares(oracle,1,0,10),"synthetic Core share purchase unavailable");
    check(app::apply_bank_request_source(g,exe,game,random,request,revision,notices,error)&&g.manager_records==oracle.manager_records&&g.stocks==oracle.stocks,
          "shared share purchase lost fees, paid totals or holdings");
    request.revision=revision;request.kind=ui::BankRequestKind::sell_shares;request.count=4;oracle=g;sell_shares(oracle,1,0,4);
    check(app::apply_bank_request_source(g,exe,game,random,request,revision,notices,error)&&g.manager_records==oracle.manager_records&&g.stocks==oracle.stocks,
          "shared share sale differs from Core");
    request.revision=revision;request.kind=ui::BankRequestKind::stock_tip;request.count=0;oracle=g;auto expected=random;
    const auto tip=stock_tip(oracle,1,0,expected);
    check(app::apply_bank_request_source(g,exe,game,random,request,revision,notices,error)&&g.manager_records==oracle.manager_records&&
          notices.last_tip==tip&&random.seed()==expected.seed()&&random.calls()==expected.calls(),"shared stock tip lost its source charge or random stream");
    request.revision=revision;request.kind=ui::BankRequestKind::buy_fund;request.count=2;oracle=g;auto expected_game=game;
    buy_fund_units(oracle,1,0,2,expected_game);
    check(app::apply_bank_request_source(g,exe,game,random,request,revision,notices,error)&&g.manager_records==oracle.manager_records&&
          g.memory.u32(layout::FundBlock+8)==oracle.memory.u32(layout::FundBlock+8)&&
          g.memory.u16(layout::FundBlock+0x16)==oracle.memory.u16(layout::FundBlock+0x16)&&
          game.seed()==expected_game.seed()&&notices.last_tip==tip,"shared fund purchase lost paid totals, depreciation, prior tip or RNG");
    request.revision=revision;request.kind=ui::BankRequestKind::sell_fund;request.count=1;oracle=g;sell_fund_units(oracle,1,0,1);
    check(app::apply_bank_request_source(g,exe,game,random,request,revision,notices,error)&&g.manager_records==oracle.manager_records&&
          g.memory.u32(layout::FundBlock+8)==oracle.memory.u32(layout::FundBlock+8)&&g.memory.u16(layout::FundBlock+0x16)==1,
          "shared fund sale lost depreciation or holdings");
    std::cout<<"Shared bank requests: Core deposit, share/fund trades, paid totals/depreciation, charged stock tips, actor limits and RNG passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
