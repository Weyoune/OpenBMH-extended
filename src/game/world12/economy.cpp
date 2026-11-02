// SPDX-License-Identifier: GPL-3.0-only
#include "game/world12/economy.hpp"
#include "game/messages.hpp"
#include "game/week.hpp"
#include "game/bank_robbery.hpp"
#include "game/weekly.hpp"
#include <algorithm>
#include <stdexcept>
namespace bmh::world12 {
namespace {
constexpr size_t record_size=0xb0c;
constexpr size_t offer_offsets[]={0x170,0x171,0x174,0x175,0x176,0x177,0x178,0x179};
constexpr size_t monthly_offsets[]={0x338,0x33c,0x85c,0x860,0x864,0x86c,0x878,0x87c,0x880,0x884,0x888,0x88c,0x890,0x894,0x898};
constexpr size_t history_offsets[]={0x3d8,0x4a8,0x578,0x648};
uint32_t word(const Bytes& b,size_t p){return uint32_t(b.at(p))|uint32_t(b.at(p+1))<<8|uint32_t(b.at(p+2))<<16|uint32_t(b.at(p+3))<<24;}
void put(Bytes& b,size_t p,uint32_t v){for(size_t n=0;n<4;++n)b.at(p+n)=uint8_t(v>>(n*8));}
std::array<uint8_t,4> seats(const AiMatchProjection& p){
    std::array<uint8_t,4> out{};size_t at=0;
    for(uint8_t n=0;n<p.human_managers();++n)out[at++]=p.global_manager(n);
    for(uint8_t n=0;n<4;++n)if(std::find(out.begin(),out.begin()+ptrdiff_t(at),n)==out.begin()+ptrdiff_t(at))out[at++]=n;
    return out;
}
ManagerEconomy read_manager(const GameState& g,size_t local,const std::array<uint8_t,4>& map){
    ManagerEconomy m;const auto rec=local*record_size;
    m.bank_robbery_month=bank_robbery_guard_month(g,int(local));
    auto& monthly=m.monthly.emplace();monthly.history_count=g.manager_records.at(rec+0x286);
    for(size_t n=0;n<2;++n)monthly.expenses[n]=word(g.manager_records,rec+0x854+n*4);
    for(size_t n=0;n<15;++n)monthly.counters[n]=word(g.manager_records,rec+monthly_offsets[n]);
    m.deposit=word(g.manager_records,rec+0x318);m.interest_income=word(g.manager_records,rec+0x870);m.overdraft_interest=word(g.manager_records,rec+0x89c);m.depreciation=word(g.manager_records,rec+0x30c);
    m.submitted_balance_plan=word(g.manager_records,rec+0x320);
    std::copy_n(g.manager_records.begin()+ptrdiff_t(rec+0x54),8,m.planned_stand_capacity.begin());
    std::copy_n(g.manager_records.begin()+ptrdiff_t(rec+0x44),8,m.stand_flags.begin());
    m.camp_bill=word(g.manager_records,rec+0x850);m.camp_weeks=g.manager_records.at(rec+0x287);m.camp_flags=g.manager_records.at(rec+0x21f);m.weekly_count=g.manager_records.at(rec+0x285);
    std::copy_n(g.manager_records.begin()+ptrdiff_t(rec+0x1e),30,m.construction_weeks.begin());
    std::copy_n(g.manager_records.begin()+ptrdiff_t(rec+0x17a),52,m.facility_history.begin());
    for(size_t n=0;n<8;++n)m.offers[n]=g.manager_records.at(rec+offer_offsets[n]);
    for(size_t row=0;row<4;++row)for(size_t n=0;n<52;++n)m.financial_history[row][n]=word(g.manager_records,rec+history_offsets[row]+n*4);
    for(size_t lender=0;lender<5;++lender)for(size_t n=0;n<6;++n)m.loans[lender?size_t(map[lender-1])+1:0][n]=g.memory.u32(layout::Loans+uint32_t(local*0x78+lender*0x18+n*4));
    return m;
}
void write_manager(GameState& g,size_t local,const ManagerEconomy& m,const std::array<uint8_t,4>& map){
    const auto rec=local*record_size;
    if(m.depreciation)put(g.manager_records,rec+0x30c,*m.depreciation);
    set_bank_robbery_guard_month(g,int(local),m.bank_robbery_month);
    if(m.monthly){g.manager_records.at(rec+0x286)=m.monthly->history_count;
        for(size_t n=0;n<2;++n)put(g.manager_records,rec+0x854+n*4,m.monthly->expenses[n]);
        for(size_t n=0;n<15;++n)put(g.manager_records,rec+monthly_offsets[n],m.monthly->counters[n]);}
    put(g.manager_records,rec+0x318,m.deposit);put(g.manager_records,rec+0x870,m.interest_income);put(g.manager_records,rec+0x89c,m.overdraft_interest);
    put(g.manager_records,rec+0x320,m.submitted_balance_plan);
    std::copy(m.planned_stand_capacity.begin(),m.planned_stand_capacity.end(),g.manager_records.begin()+ptrdiff_t(rec+0x54));
    std::copy(m.stand_flags.begin(),m.stand_flags.end(),g.manager_records.begin()+ptrdiff_t(rec+0x44));
    put(g.manager_records,rec+0x850,m.camp_bill);g.manager_records.at(rec+0x287)=m.camp_weeks;g.manager_records.at(rec+0x21f)=m.camp_flags;g.manager_records.at(rec+0x285)=m.weekly_count;
    std::copy(m.construction_weeks.begin(),m.construction_weeks.end(),g.manager_records.begin()+ptrdiff_t(rec+0x1e));
    std::copy(m.facility_history.begin(),m.facility_history.end(),g.manager_records.begin()+ptrdiff_t(rec+0x17a));
    for(size_t n=0;n<8;++n)g.manager_records.at(rec+offer_offsets[n])=m.offers[n];
    for(size_t row=0;row<4;++row)for(size_t n=0;n<52;++n)put(g.manager_records,rec+history_offsets[row]+n*4,m.financial_history[row][n]);
    g.memory.add_block(layout::Loans,4*0x78);
    for(size_t lender=0;lender<5;++lender)for(size_t n=0;n<6;++n)g.memory.set32(layout::Loans+uint32_t(local*0x78+lender*0x18+n*4),m.loans[lender?size_t(map[lender-1])+1:0][n]);
}
void install_mailbox(GameState& g,const CareerEconomy& e,const std::array<uint8_t,4>& map){
    g.mailbox.assign(kMailboxEntries*kMailboxEntry,0);
    const auto local_mask=[&](uint32_t raw){uint32_t value=raw&0x80000000u;for(uint32_t n=0;n<4;++n)if(raw&(1u<<map[n]))value|=1u<<n;return value;};
    for(size_t row=0;row<e.messages.size();++row){const auto& m=e.messages[row];const auto at=row*kMailboxEntry;
        put(g.mailbox,at,local_mask(m.recipients));put(g.mailbox,at+4,local_mask(m.read_by));g.mailbox[at+8]=uint8_t(m.date>>8);g.mailbox[at+9]=uint8_t(m.date);
        std::copy(m.text.begin(),m.text.end(),g.mailbox.begin()+ptrdiff_t(at+10));
    }
    g.memory.add_block(layout::MailboxRecipientMask,4);g.memory.set32(layout::MailboxRecipientMask,local_mask(e.recipient_mask));
}
void capture_mailbox(CareerEconomy& e,const GameState& g,const std::array<uint8_t,4>& map){
    const auto global_mask=[&](uint32_t raw){uint32_t value=raw&0x80000000u;for(uint32_t n=0;n<4;++n)if(raw&(1u<<n))value|=1u<<map[n];return value;};
    e.messages.clear();size_t retained=0;
    for(size_t row=0;row<kMailboxEntries;++row){const auto at=row*kMailboxEntry;if(std::any_of(g.mailbox.begin()+ptrdiff_t(at),g.mailbox.begin()+ptrdiff_t(at+kMailboxEntry),[](auto b){return b!=0;}))retained=row+1;EconomyMessage m;
        m.recipients=global_mask(word(g.mailbox,at));m.read_by=global_mask(word(g.mailbox,at+4));m.date=uint16_t(uint16_t(g.mailbox[at+8])<<8|g.mailbox[at+9]);
        std::copy_n(g.mailbox.begin()+ptrdiff_t(at+10),kMailboxText,m.text.begin());e.messages.push_back(m);
    }
    e.messages.resize(retained);
    e.recipient_mask=global_mask(g.memory.u32(layout::MailboxRecipientMask));
}
}
namespace {
PoolsCoupon read_coupon(const GameState& g,size_t local) {
    const auto at=layout::Coupon+uint32_t(local*20);PoolsCoupon c;const auto raw=g.memory.u32(at);c.stake=raw&0x7fffffffu;c.voided=(raw&0x80000000u)!=0;c.winnings=g.memory.u32(at+4);c.correct=g.memory.u8(at+19);for(uint32_t n=0;n<11;++n)c.tips[n]=g.memory.u8(at+8+n);return c;
}
void write_coupon(GameState& g,size_t local,const PoolsCoupon& c) {
    const auto at=layout::Coupon+uint32_t(local*20);g.memory.set32(at,c.stake|(c.voided?0x80000000u:0));g.memory.set32(at+4,c.winnings);g.memory.set8(at+19,c.correct);for(uint32_t n=0;n<11;++n)g.memory.set8(at+8+n,c.tips[n]);
}
}
void initialize_career_economy(CareerSnapshot& state,const GameState& reference){
    state.validate();if(state.world.economy||!state.world.settings||state.date||state.season||state.world.managers.size()!=state.world.settings->planned_managers||state.desktop)throw std::invalid_argument("world12: economy requires fresh pre-desktop initialization");
    CareerEconomy e;e.week=reference.memory.u8(layout::Savestate_Layout_ExecutableField);
    if(reference.memory.has(layout::LeaguePresentationOptions)&&reference.memory.has(layout::LeaguePresentationOptions+179)){e.presentation_options.emplace();for(uint32_t i=0;i<180;++i)(*e.presentation_options)[i]=reference.memory.u8(layout::LeaguePresentationOptions+i);}
    if(reference.memory.has(layout::AdGroupId))e.advertising_group=reference.memory.u8(layout::AdGroupId);
    if(reference.memory.has(layout::Coupon)&&reference.memory.has(layout::Coupon+99)) {e.pools.emplace();for(size_t n=0;n<4;++n)e.pools->managers[n]=read_coupon(reference,n);for(uint32_t n=0;n<11;++n)e.pools->previous_outcomes[n]=reference.memory.u8(layout::MatchdaySetup_EvaluatePoolsCoupon_RuntimeField+n);}
    const std::array<uint8_t,4> identity{0,1,2,3};for(size_t n=0;n<4;++n)e.managers[n]=read_manager(reference,n,identity);
    // Initial reference mail must be empty: do not import an unrelated career.
    if(reference.mailbox.size()!=kMailboxEntries*kMailboxEntry||std::any_of(reference.mailbox.begin(),reference.mailbox.end(),[](auto b){return b!=0;}))throw std::invalid_argument("world12: initial economy reference contains mailbox state");
    auto candidate=state;candidate.world.economy=e;candidate.validate();state=std::move(candidate);
}
void install_career_economy(AiMatchProjection& projection,const World& world){
    if(!world.economy||!world.markets)throw std::invalid_argument("world12: career economy/market is missing");
    const auto map=seats(projection);auto& g=projection.state;const auto& e=*world.economy;
    if(e.pools){g.memory.add_block(layout::Coupon,100);for(size_t n=0;n<4;++n)write_coupon(g,n,e.pools->managers[map[n]]);for(uint32_t n=0;n<11;++n)g.memory.set8(layout::MatchdaySetup_EvaluatePoolsCoupon_RuntimeField+n,e.pools->previous_outcomes[n]);}
    if(e.advertising_group){g.memory.add_block(layout::AdGroupId,1);g.memory.set8(layout::AdGroupId,*e.advertising_group);}
    g.memory.add_block(layout::Savestate_Layout_ExecutableField,1);g.memory.set8(layout::Savestate_Layout_ExecutableField,e.week);
    g.stocks.assign(world.markets->stocks.begin(),world.markets->stocks.end());g.memory.add_block(layout::FundBlock,world.markets->funds.size());
    for(size_t n=0;n<world.markets->funds.size();++n)g.memory.set8(layout::FundBlock+uint32_t(n),world.markets->funds[n]);
    for(size_t local=0;local<4;++local){const auto global=map[local];
        for(size_t share=0;share<12;++share)for(size_t b=0;b<2;++b)g.stocks[share*0x5c+0x50+local*2+b]=world.markets->stocks[share*0x5c+0x50+global*2+b];
        for(size_t share=0;share<12;++share)for(size_t b=0;b<4;++b)g.stocks[share*0x5c+4+local*4+b]=world.markets->stocks[share*0x5c+4+global*4+b];
        for(size_t fund=0;fund<6;++fund){const auto address=layout::Advertising_OriginalShareAdjustment_RuntimeField+uint32_t(fund*0x1c);const size_t offset=address-layout::FundBlock;
            const auto& bytes=world.markets->funds;const auto value=uint16_t(bytes.at(offset+global*2)|uint16_t(bytes.at(offset+global*2+1))<<8);g.memory.set16(address+uint32_t(local*2),value);}
        for(size_t fund=0;fund<6;++fund)for(size_t b=0;b<4;++b)
            g.memory.set8(layout::FundBlock+uint32_t(fund*0x1c+4+local*4+b),world.markets->funds[fund*0x1c+4+global*4+b]);
    }
    for(uint8_t n=0;n<projection.human_managers();++n)write_manager(g,n,e.managers[map[n]],map);
    install_mailbox(g,e,map);
}
void capture_career_economy(World& world,const AiMatchProjection& projection){
    if(!world.economy||!world.markets)throw std::invalid_argument("world12: career economy/market is missing");
    const auto map=seats(projection);const auto& g=projection.state;auto& e=*world.economy;
    if(e.pools){for(size_t n=0;n<projection.human_managers();++n)e.pools->managers[map[n]]=read_coupon(g,n);for(uint32_t n=0;n<11;++n)e.pools->previous_outcomes[n]=g.memory.u8(layout::MatchdaySetup_EvaluatePoolsCoupon_RuntimeField+n);}
    if(e.presentation_options)for(uint32_t i=0;i<180;++i)(*e.presentation_options)[i]=g.memory.u8(layout::LeaguePresentationOptions+i);
    e.week=g.memory.u8(layout::Savestate_Layout_ExecutableField);
    if(e.advertising_group)e.advertising_group=g.memory.u8(layout::AdGroupId);
    for(uint8_t n=0;n<projection.human_managers();++n){const auto had_monthly=e.managers[map[n]].monthly.has_value();const auto had_depreciation=e.managers[map[n]].depreciation.has_value();e.managers[map[n]]=read_manager(g,n,map);if(!had_monthly)e.managers[map[n]].monthly.reset();if(!had_depreciation)e.managers[map[n]].depreciation.reset();}
    capture_mailbox(e,g,map);
}
void capture_career_portfolios(World& world,const AiMatchProjection& projection) {
    if(!world.markets||projection.state.stocks.size()!=world.markets->stocks.size())
        throw FormatError("world12: portfolio workspace is incomplete");
    const auto& g=projection.state;const auto map=seats(projection);auto stocks=world.markets->stocks;auto funds=world.markets->funds;
    std::copy(g.stocks.begin(),g.stocks.end(),stocks.begin());
    for(size_t n=0;n<funds.size();++n)funds[n]=g.memory.u8(layout::FundBlock+uint32_t(n));
    for(size_t local=0;local<4;++local){const auto global=map[local];
        for(size_t share=0;share<12;++share) {
            for(size_t b=0;b<2;++b)stocks[share*0x5c+0x50+global*2+b]=g.stocks[share*0x5c+0x50+local*2+b];
            for(size_t b=0;b<4;++b)stocks[share*0x5c+4+global*4+b]=g.stocks[share*0x5c+4+local*4+b];
        }
        for(size_t fund=0;fund<6;++fund) {
            for(size_t b=0;b<2;++b)funds[fund*0x1c+0x14+global*2+b]=g.memory.u8(layout::FundBlock+uint32_t(fund*0x1c+0x14+local*2+b));
            for(size_t b=0;b<4;++b)funds[fund*0x1c+4+global*4+b]=g.memory.u8(layout::FundBlock+uint32_t(fund*0x1c+4+local*4+b));
        }
    }
    world.markets->stocks=stocks;world.markets->funds=funds;
}
Bytes career_mailbox_bytes(const CareerEconomy& economy){GameState g;const std::array<uint8_t,4> identity{0,1,2,3};install_mailbox(g,economy,identity);return g.mailbox;}
MailboxEditFailure edit_career_message(CareerEconomy& economy,int actor,int row,const std::array<uint8_t,0xa0>& expected,MailboxEditKind kind){
    GameState g;const std::array<uint8_t,4> identity{0,1,2,3};install_mailbox(g,economy,identity);
    const auto failure=edit_mailbox_entry(g.mailbox,actor,row,expected,kind);if(failure!=MailboxEditFailure::none)return failure;
    auto candidate=economy;capture_mailbox(candidate,g,identity);economy=std::move(candidate);return failure;
}
void append_economy_message(CareerEconomy& economy,const EconomyMessage& message){
    if(!message.recipients||(message.recipients&~0x8000000fu)||message.read_by)throw std::invalid_argument("world12: invalid newly posted economy message");
    GameState g;const std::array<uint8_t,4> identity{0,1,2,3};install_mailbox(g,economy,identity);g.date=message.date;
    post_message_mask(g,message.recipients,Bytes(message.text.begin(),message.text.end()));capture_mailbox(economy,g,identity);
}
namespace {
EconomyPhaseOutcome phase(CareerSnapshot& state,const GameState& reference,bool weekly,bool stocks,bool interest=true){
    state.validate();if(!state.world.economy||!state.world.settings||state.world.managers.empty())throw std::invalid_argument("world12: unavailable economy phase");
    const auto before=state;auto candidate=state;std::vector<ClubId> clubs;for(const auto& m:state.world.managers)clubs.push_back(m.club);
    const auto& s=*state.world.settings;auto p=AiMatchProjection::create_for_humans(candidate,clubs,reference,{s.mode,s.difficulty,s.foreign_rule,true});install_career_economy(p,candidate.world);
    GameRandom game(state.game_seed);WatcomRandom random(state.watcom_seed);
    if(stocks)stock_market(p.state,game);
    for(size_t m=0;m<p.human_managers();++m){if(weekly)weekly_manager_settlement(p.state,int(m),random);else if(interest){p.state.manager_records.at(m*record_size+0x21f)&=0x7f;manager_daily_interest(p.state,int(m),random);}}
    p.commit_match_state(candidate.world);capture_career_economy(candidate.world,p);
    if(stocks)capture_career_portfolios(candidate.world,p);
    candidate.game_seed=game.seed();candidate.watcom_seed=random.seed();candidate.validate();if(state!=before)throw std::invalid_argument("world12: economy changed during service phase");
    const EconomyPhaseOutcome result{game.calls(),random.calls(),p.human_managers()};state=std::move(candidate);return result;
}
}
EconomyPhaseOutcome update_daily_stock_market(CareerSnapshot& state,const GameState& reference){return phase(state,reference,false,true,false);}
EconomyPhaseOutcome daily_finance(CareerSnapshot& s,const GameState& r,DailyEconomyKind kind){
    if(kind!=DailyEconomyKind::after_event&&kind!=DailyEconomyKind::plain_day)throw std::invalid_argument("world12: invalid daily economy kind");
    return phase(s,r,false,kind==DailyEconomyKind::plain_day);
}
EconomyPhaseOutcome settle_weekly_managers(CareerSnapshot& s,const GameState& r){return phase(s,r,true,false);}
}
