// SPDX-License-Identifier: GPL-3.0-only
#include "app/advertising_request_source.hpp"
#include <iostream>
#include <stdexcept>
namespace {void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}}
int main()try {
    using namespace bmh;GameState g;g.level=3;g.manager_teams={0};g.manager_records.resize(0xb0c);g.memory.add_block(layout::ActiveManager,1);
    const auto put=[&](size_t at,uint32_t value){for(size_t n=0;n<4;++n)g.manager_records[at+n]=uint8_t(value>>(8*n));};put(0x314,1000000);
    WatcomRandom random(77);uint64_t revision=4,last=0;std::string error;AdvertisingSpendingResult result;
    ui::AdvertisingRequest request;request.request_id=1;request.manager_slot=0;request.runtime_club=0;request.revision=revision;request.kind=ui::AdvertisingActionKind::set_spending;request.spending={0,17,5000,0,0};
    auto expected=g;auto rng=random;const auto budget=set_advertising_spending(expected,request.spending,rng);
    check(app::apply_advertising_request_source(g,random,request,revision,last,error,&result)&&result.status==budget.status&&g.manager_records==expected.manager_records&&random.calls()==rng.calls()&&revision==5,"shared monthly advertising budget diverged from Core");
    request.request_id=2;request.revision=revision;request.spending.expected_amount=1;const auto before=g.manager_records;
    check(!app::apply_advertising_request_source(g,random,request,revision,last,error,&result)&&g.manager_records==before&&revision==5,"stale budget changed state or revision");
    request.spending={0,16,1000,0,0};expected=g;rng=random;const auto goods=set_advertising_spending(expected,request.spending,rng);
    check(app::apply_advertising_request_source(g,random,request,revision,last,error,&result)&&result.status==goods.status&&g.manager_records==expected.manager_records&&random.seed()==rng.seed()&&random.calls()==rng.calls(),"shared merchandise cost/delivery lost Core effects or RNG");
    request.request_id=3;request.revision=revision;request.spending.expected_amount=1000;request.spending.expected_delivery_weeks=g.manager_records[0x287];const auto delivered=g.manager_records;const auto seed=random.seed();
    check(!app::apply_advertising_request_source(g,random,request,revision,last,error,&result)&&g.manager_records==delivered&&random.seed()==seed,"delivery-pending purchase charged twice");
    put(0x314,0);put(0x850,777);g.manager_records[0x287]=0;request.spending.expected_amount=777;request.spending.expected_delivery_weeks=0;
    expected=g;rng=random;const auto poor=set_advertising_spending(expected,request.spending,rng);
    check(app::apply_advertising_request_source(g,random,request,revision,last,error,&result)&&poor.status==AdvertisingSpendingStatus::insufficient_funds&&result.status==poor.status&&g.manager_records==expected.manager_records&&random.seed()==seed&&random.calls()==rng.calls(),"insufficient-funds response lost the old-order clear or drew a delivery delay");
    request={};request.kind=ui::AdvertisingActionKind::check_renewal;request.request_id=4;request.manager_slot=0;request.runtime_club=0;request.revision=revision;request.renewal_display_slot=request.renewal_contract_slot=14;
    g.manager_records[0xa34+14*12+6]=12;std::copy_n(g.manager_records.begin()+0xa34+14*12,12,request.renewal_expected_contract.begin());
    check(app::validate_advertising_renewal_request_source(g,request,revision,last,error),"matching expired sponsor contract rejected");
    request.renewal_expected_contract[7]^=1;check(!app::validate_advertising_renewal_request_source(g,request,revision,last,error)&&random.seed()==seed,"modified sponsor owner accepted or consumed RNG");
    std::cout<<"Shared advertising budgets, merchandise fees/delivery, stale guards and renewal binding passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
