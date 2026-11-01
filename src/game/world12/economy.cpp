// SPDX-License-Identifier: GPL-3.0-only
#include "game/world12/economy.hpp"
#include "game/messages.hpp"
#include "game/week.hpp"
#include "game/weekly.hpp"
#include <algorithm>
#include <stdexcept>
namespace bmh::world12 {
namespace {
constexpr size_t record_size=0xb0c;
constexpr size_t offer_offsets[]={0x170,0x171,0x174,0x175,0x176,0x177,0x178,0x179};
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
    m.deposit=word(g.manager_records,rec+0x318);m.interest_income=word(g.manager_records,rec+0x870);m.overdraft_interest=word(g.manager_records,rec+0x89c);
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
    put(g.manager_records,rec+0x318,m.deposit);put(g.manager_records,rec+0x870,m.interest_income);put(g.manager_records,rec+0x89c,m.overdraft_interest);
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
    e.messages.clear();
    for(size_t row=0;row<kMailboxEntries;++row){const auto at=row*kMailboxEntry;if(!word(g.mailbox,at))break;EconomyMessage m;
        m.recipients=global_mask(word(g.mailbox,at));m.read_by=global_mask(word(g.mailbox,at+4));m.date=uint16_t(uint16_t(g.mailbox[at+8])<<8|g.mailbox[at+9]);
        std::copy_n(g.mailbox.begin()+ptrdiff_t(at+10),kMailboxText,m.text.begin());e.messages.push_back(m);
    }
    e.recipient_mask=global_mask(g.memory.u32(layout::MailboxRecipientMask));
}
}
void initialize_career_economy(CareerSnapshot& state,const GameState& reference){
    state.validate();if(state.world.economy||!state.world.settings||state.date||state.season||state.world.managers.size()!=state.world.settings->planned_managers||state.desktop)throw std::invalid_argument("world12: economy requires fresh pre-desktop initialization");
    CareerEconomy e;e.week=reference.memory.u8(layout::Savestate_Layout_ExecutableField);
    const std::array<uint8_t,4> identity{0,1,2,3};for(size_t n=0;n<4;++n)e.managers[n]=read_manager(reference,n,identity);
    // Initial reference mail must be empty: do not import an unrelated career.
    if(reference.mailbox.size()!=kMailboxEntries*kMailboxEntry||std::any_of(reference.mailbox.begin(),reference.mailbox.end(),[](auto b){return b!=0;}))throw std::invalid_argument("world12: initial economy reference contains mailbox state");
    auto candidate=state;candidate.world.economy=e;candidate.validate();state=std::move(candidate);
}
void install_career_economy(AiMatchProjection& projection,const World& world){
    if(!world.economy||!world.markets)throw std::invalid_argument("world12: career economy/market is missing");
    const auto map=seats(projection);auto& g=projection.state;const auto& e=*world.economy;
    g.memory.add_block(layout::Savestate_Layout_ExecutableField,1);g.memory.set8(layout::Savestate_Layout_ExecutableField,e.week);
    g.stocks.assign(world.markets->stocks.begin(),world.markets->stocks.end());g.memory.add_block(layout::FundBlock,world.markets->funds.size());
    for(size_t n=0;n<world.markets->funds.size();++n)g.memory.set8(layout::FundBlock+uint32_t(n),world.markets->funds[n]);
    for(size_t local=0;local<4;++local){const auto global=map[local];
        for(size_t share=0;share<12;++share)for(size_t b=0;b<2;++b)g.stocks[share*0x5c+0x50+local*2+b]=world.markets->stocks[share*0x5c+0x50+global*2+b];
        for(size_t fund=0;fund<6;++fund){const auto address=layout::Advertising_OriginalShareAdjustment_RuntimeField+uint32_t(fund*0x1c);const size_t offset=address-layout::FundBlock;
            const auto& bytes=world.markets->funds;const auto value=uint16_t(bytes.at(offset+global*2)|uint16_t(bytes.at(offset+global*2+1))<<8);g.memory.set16(address+uint32_t(local*2),value);}
    }
    for(uint8_t n=0;n<projection.human_managers();++n)write_manager(g,n,e.managers[map[n]],map);
    install_mailbox(g,e,map);
}
void capture_career_economy(World& world,const AiMatchProjection& projection){
    if(!world.economy||!world.markets)throw std::invalid_argument("world12: career economy/market is missing");
    const auto map=seats(projection);const auto& g=projection.state;auto& e=*world.economy;
    e.week=g.memory.u8(layout::Savestate_Layout_ExecutableField);
    for(uint8_t n=0;n<projection.human_managers();++n)e.managers[map[n]]=read_manager(g,n,map);
    capture_mailbox(e,g,map);
}
void append_economy_message(CareerEconomy& economy,const EconomyMessage& message){
    if(!message.recipients||(message.recipients&~0x8000000fu)||message.read_by)throw std::invalid_argument("world12: invalid newly posted economy message");
    GameState g;const std::array<uint8_t,4> identity{0,1,2,3};install_mailbox(g,economy,identity);g.date=message.date;
    post_message_mask(g,message.recipients,Bytes(message.text.begin(),message.text.end()));capture_mailbox(economy,g,identity);
}
namespace {
EconomyPhaseOutcome phase(CareerSnapshot& state,const GameState& reference,bool weekly,bool stocks){
    state.validate();if(!state.world.economy||!state.world.settings||state.world.managers.empty())throw std::invalid_argument("world12: unavailable economy phase");
    const auto before=state;auto candidate=state;std::vector<ClubId> clubs;for(const auto& m:state.world.managers)clubs.push_back(m.club);
    const auto& s=*state.world.settings;auto p=AiMatchProjection::create_for_humans(candidate,clubs,reference,{s.mode,s.difficulty,s.foreign_rule,true});install_career_economy(p,candidate.world);
    GameRandom game(state.game_seed);WatcomRandom random(state.watcom_seed);
    if(stocks)stock_market(p.state,game);
    for(size_t m=0;m<p.human_managers();++m){if(weekly)weekly_manager_settlement(p.state,int(m),random);else{p.state.manager_records.at(m*record_size+0x21f)&=0x7f;manager_daily_interest(p.state,int(m),random);}}
    p.commit_match_state(candidate.world);capture_career_economy(candidate.world,p);
    if(stocks){auto& stocks=candidate.world.markets->stocks;std::copy(p.state.stocks.begin(),p.state.stocks.end(),stocks.begin());const auto map=seats(p);for(size_t n=0;n<4;++n)for(size_t share=0;share<12;++share)for(size_t b=0;b<2;++b)stocks[share*0x5c+0x50+map[n]*2+b]=p.state.stocks[share*0x5c+0x50+n*2+b];}
    candidate.game_seed=game.seed();candidate.watcom_seed=random.seed();candidate.validate();if(state!=before)throw std::invalid_argument("world12: economy changed during service phase");
    const EconomyPhaseOutcome result{game.calls(),random.calls(),p.human_managers()};state=std::move(candidate);return result;
}
}
EconomyPhaseOutcome daily_finance(CareerSnapshot& s,const GameState& r,DailyEconomyKind kind){
    if(kind!=DailyEconomyKind::after_event&&kind!=DailyEconomyKind::plain_day)throw std::invalid_argument("world12: invalid daily economy kind");
    return phase(s,r,false,kind==DailyEconomyKind::plain_day);
}
EconomyPhaseOutcome settle_weekly_managers(CareerSnapshot& s,const GameState& r){return phase(s,r,true,false);}
}
