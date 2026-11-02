// SPDX-License-Identifier: GPL-3.0-only
#include "game/world12/codec.hpp"
#include "cli/json.hpp"
#include <stdexcept>
#include <algorithm>
#include <type_traits>
#include <limits>
#include <charconv>
namespace bmh::world12 {
namespace {
using J=cli::Json;using A=J::Array;using O=J::Object;
const J& field(const J& j,const char*name){if(!j.object())throw FormatError("world12: object expected");auto it=j.object()->find(name);if(it==j.object()->end())throw FormatError(std::string("world12: missing ")+name);return it->second;}
const A& array(const J& j){if(!j.array())throw FormatError("world12: array expected");return *j.array();}
uint32_t number(const J& j,uint32_t maximum=4294967294u){if(!j.integer()||*j.integer()<0||uint64_t(*j.integer())>maximum)throw FormatError("world12: integer/reference out of range");return uint32_t(*j.integer());}
std::string text(const J& j){if(!j.string()||j.string()->size()>4096)throw FormatError("world12: invalid text");return *j.string();}
J id(uint32_t value){return J(int64_t(value));}
int32_t signed_number(const J& j){if(!j.integer()||*j.integer()<-2147483648LL||*j.integer()>2147483647LL)throw FormatError("world12: signed 32-bit value out of range");return int32_t(*j.integer());}
bool boolean(const J& j){if(!j.boolean())throw FormatError("world12: boolean game scope expected");return *j.boolean();}
uint64_t statistics_number(const J& input) {
    if(!input.string() || input.string()->empty() || input.string()->size()>20)
        throw FormatError("world12: statistics require exact unsigned decimal strings");
    uint64_t result=0;const auto& value=*input.string();
    const auto parsed=std::from_chars(value.data(),value.data()+value.size(),result);
    if(parsed.ec!=std::errc{} || parsed.ptr!=value.data()+value.size())
        throw FormatError("world12: statistics value out of range");
    return result;
}
J encode_statistics(const GlobalPlayerStatistics& records) {
    A rows;
    for(const auto& [identity,s]:records) {
        A counters;for(auto n:s.appearances)counters.emplace_back(std::to_string(n));
        for(auto n:{s.total,s.goals,s.assists})counters.emplace_back(std::to_string(n));
        rows.emplace_back(O{{"club",id(identity.first.value)},{"player",id(identity.second.value)},{"counters",std::move(counters)}});
    }
    return rows;
}
GlobalPlayerStatistics decode_statistics(const J& input) {
    GlobalPlayerStatistics records;
    for(const auto& row:array(input)) {
        const auto identity=std::pair{ClubId{number(field(row,"club"))},PlayerId{number(field(row,"player"))}};
        const auto& values=array(field(row,"counters"));
        if(values.size()!=6)throw FormatError("world12: statistics dimensions must be six");
        PlayerClubStatistics s;for(size_t n=0;n<3;++n)s.appearances[n]=statistics_number(values[n]);
        s.total=statistics_number(values[3]);s.goals=statistics_number(values[4]);s.assists=statistics_number(values[5]);
        if(!records.emplace(identity,s).second)throw FormatError("world12: duplicate global statistics identity");
    }
    return records;
}
J encode_tactical_backing(const std::optional<ManagerTacticalBacking>& value) {
    if(!value)return nullptr;
    A rows,marks,marked_by,formations,tail,enabled,automation;
    for(const auto& row:value->scalar_rows){A bytes;for(auto b:row)bytes.emplace_back(int(b));rows.emplace_back(std::move(bytes));}
    for(auto p:value->marks)marks.push_back(p.valid()?id(p.value):J(nullptr));
    for(auto p:value->marked_by)marked_by.push_back(p.valid()?id(p.value):J(nullptr));
    for(auto b:value->formations)formations.emplace_back(int(b));
    for(auto b:value->formation_tail)tail.emplace_back(int(b));
    for(auto b:value->enabled_formations)enabled.emplace_back(int(b));
    for(auto b:value->automation)automation.emplace_back(int(b));
    return O{{"scalar_rows",std::move(rows)},{"marks",std::move(marks)},{"marked_by",std::move(marked_by)},
        {"formations",std::move(formations)},{"formation_tail",std::move(tail)},{"enabled_formations",std::move(enabled)},{"automation",std::move(automation)}};
}
std::optional<ManagerTacticalBacking> decode_tactical_backing(const J& value) {
    if(std::holds_alternative<std::nullptr_t>(value.value))return {};
    ManagerTacticalBacking result;
    const auto bytes=[&](const J& input,auto& output){const auto& a=array(input);if(a.size()!=output.size())throw FormatError("world12: invalid tactical byte dimensions");for(size_t i=0;i<a.size();++i)output[i]=uint8_t(number(a[i],255));};
    const auto refs=[&](const J& input,auto& output){const auto& a=array(input);if(a.size()!=output.size())throw FormatError("world12: invalid tactical reference dimensions");for(size_t i=0;i<a.size();++i){if(a[i].integer())output[i]={number(a[i])};else if(!std::holds_alternative<std::nullptr_t>(a[i].value))throw FormatError("world12: invalid tactical player reference");}};
    const auto& rows=array(field(value,"scalar_rows"));if(rows.size()!=24)throw FormatError("world12: invalid full tactical row count");
    for(size_t row=0;row<24;++row)bytes(rows[row],result.scalar_rows[row]);
    refs(field(value,"marks"),result.marks);refs(field(value,"marked_by"),result.marked_by);
    bytes(field(value,"formations"),result.formations);bytes(field(value,"formation_tail"),result.formation_tail);bytes(field(value,"enabled_formations"),result.enabled_formations);bytes(field(value,"automation"),result.automation);
    return result;
}
J encode_tactics(const ManagerTactics& tactics) {
    A rows;
    for(const auto& row:tactics.rows) {
        A orders;for(auto value:row.orders)orders.emplace_back(int(value));
        rows.emplace_back(O{{"orders",std::move(orders)},{"partner",int(row.partner)},
            {"depth",int(row.depth)},{"lane",int(row.lane)}});
    }
    return O{{"rows",std::move(rows)},{"automatic_weights",int(tactics.automatic_weights)},
        {"automatic_formation",int(tactics.automatic_formation)},
        {"backing",encode_tactical_backing(tactics.backing)}};
}
ManagerTactics decode_tactics(const J& input,uint32_t version) {
    ManagerTactics tactics;const auto& rows=array(field(input,"rows"));
    if(rows.size()!=24)throw FormatError("world12: tactics require 24 rows");
    for(size_t i=0;i<24;++i) {
        const auto& row=rows[i];const auto& orders=array(field(row,"orders"));
        if(orders.size()!=4)throw FormatError("world12: tactics require four player orders");
        for(size_t j=0;j<4;++j)tactics.rows[i].orders[j]=uint8_t(number(orders[j],255));
        tactics.rows[i].partner=uint8_t(number(field(row,"partner"),255));
        tactics.rows[i].depth=uint8_t(number(field(row,"depth"),255));
        tactics.rows[i].lane=uint8_t(number(field(row,"lane"),255));
    }
    tactics.automatic_weights=uint8_t(number(field(input,"automatic_weights"),255));
    tactics.automatic_formation=uint8_t(number(field(input,"automatic_formation"),8));
    if(version>=7)tactics.backing=decode_tactical_backing(field(input,"backing"));
    return tactics;
}
J encode_attendance(const std::optional<ManagerAttendanceState>& input) {
    if(!input)return nullptr;
    const auto values=[](const auto& row){A out;for(auto value:row)out.push_back(id(value));return J(std::move(out));};
    const auto refs=[](const auto& row){A out;for(auto value:row)out.push_back(value.valid()?id(value.value):J(nullptr));return J(std::move(out));};
    const auto& a=*input;
    return O{{"league_home_count",int(a.league_home_count)},{"ticket_prices",values(a.ticket_prices)},
        {"spectators",values(a.spectators)},{"catering",values(a.catering)},{"match_counts",values(a.match_counts)},
        {"spectator_totals",values(a.spectator_totals)},{"cup_income",values(a.cup_income)},
        {"record_spectators",values(a.record_spectators)},{"record_spectator_dates",values(a.record_spectator_dates)},
        {"record_income",values(a.record_income)},{"record_income_dates",values(a.record_income_dates)},
        {"spectator_opponents",refs(a.spectator_opponents)},{"income_opponents",refs(a.income_opponents)}};
}
std::optional<ManagerAttendanceState> decode_attendance(const J& input) {
    if(std::holds_alternative<std::nullptr_t>(input.value))return {};
    ManagerAttendanceState a;
    const auto values=[&](const char* name,auto& output,uint32_t maximum=4294967295u){const auto& row=array(field(input,name));if(row.size()!=output.size())throw FormatError("world12: invalid attendance dimensions");for(size_t i=0;i<row.size();++i)output[i]=number(row[i],maximum);};
    const auto refs=[&](const char* name,auto& output){const auto& row=array(field(input,name));if(row.size()!=output.size())throw FormatError("world12: invalid attendance reference dimensions");for(size_t i=0;i<row.size();++i){if(row[i].integer())output[i]={number(row[i])};else if(!std::holds_alternative<std::nullptr_t>(row[i].value))throw FormatError("world12: invalid attendance opponent");}};
    a.league_home_count=uint8_t(number(field(input,"league_home_count"),19));values("ticket_prices",a.ticket_prices,255);
    values("spectators",a.spectators);values("catering",a.catering);values("match_counts",a.match_counts,255);
    values("spectator_totals",a.spectator_totals);values("cup_income",a.cup_income);
    values("record_spectators",a.record_spectators);values("record_spectator_dates",a.record_spectator_dates);
    values("record_income",a.record_income);values("record_income_dates",a.record_income_dates);
    refs("spectator_opponents",a.spectator_opponents);refs("income_opponents",a.income_opponents);return a;
}
J encode_manager_match(const std::optional<ManagerMatchState>& input) {
    if(!input)return nullptr;const auto& m=*input;
    const auto bytes=[](const auto& values){A a;for(auto value:values)a.emplace_back(int(value));return J(std::move(a));};
    A fixtures,takers,history;
    for(auto value:m.recent_fixtures)fixtures.push_back(value.valid()?id(value.value):J(nullptr));
    for(auto value:m.takers)takers.push_back(value.valid()?id(value.value):J(nullptr));
    for(const auto& [club,scores]:m.head_to_head)history.emplace_back(O{{"club",id(club.value)},{"scores",bytes(scores)}});
    return O{{"attendance",encode_attendance(m.attendance)},{"preferences",bytes(m.preferences)},{"tax_policy",int(m.tax_policy)},{"sponsor_win_bonus",int(m.sponsor_win_bonus)},
        {"crowd_pressure",int(m.crowd_pressure)},{"goal_notices",int(m.goal_notices)},{"point_deduction",int(m.point_deduction)},
        {"best_streaks",bytes(m.best_streaks)},{"timing_stats",bytes(m.timing_stats)},{"staff_ratings",bytes(m.staff_ratings)},
        {"rating_history",bytes(m.rating_history)},{"recent_scores",bytes(m.recent_scores)},{"recent_fixtures",std::move(fixtures)},
        {"head_to_head",std::move(history)},{"designated_takers",m.designated_takers},{"takers",std::move(takers)}};
}
std::optional<ManagerMatchState> decode_manager_match(const J& input,uint32_t version) {
    if(std::holds_alternative<std::nullptr_t>(input.value))return {};
    ManagerMatchState result;
    if(version>=25)result.attendance=decode_attendance(field(input,"attendance"));
    else if(const auto found=input.object()->find("attendance");found!=input.object()->end()&&!std::holds_alternative<std::nullptr_t>(found->second.value))
        throw FormatError("world12: older schema cannot restore attendance records");
    const auto numbers=[&](const char* key,auto& values,uint32_t maximum){const auto& a=array(field(input,key));if(a.size()!=values.size())throw FormatError("world12: invalid manager match dimensions");for(size_t i=0;i<a.size();++i)values[i]=number(a[i],maximum);};
    const auto refs=[&](const char* key,auto& values){const auto& a=array(field(input,key));if(a.size()!=values.size())throw FormatError("world12: invalid manager reference dimensions");for(size_t i=0;i<a.size();++i){if(a[i].integer())values[i]={number(a[i])};else if(!std::holds_alternative<std::nullptr_t>(a[i].value))throw FormatError("world12: invalid manager optional reference");}};
    numbers("preferences",result.preferences,255);numbers("best_streaks",result.best_streaks,255);numbers("timing_stats",result.timing_stats,255);
    numbers("staff_ratings",result.staff_ratings,65535);numbers("rating_history",result.rating_history,255);numbers("recent_scores",result.recent_scores,255);
    result.tax_policy=uint8_t(number(field(input,"tax_policy"),255));result.sponsor_win_bonus=uint8_t(number(field(input,"sponsor_win_bonus"),255));
    result.crowd_pressure=uint8_t(number(field(input,"crowd_pressure"),255));result.goal_notices=uint8_t(number(field(input,"goal_notices"),255));result.point_deduction=uint8_t(number(field(input,"point_deduction"),255));
    refs("recent_fixtures",result.recent_fixtures);refs("takers",result.takers);result.designated_takers=boolean(field(input,"designated_takers"));
    for(const auto& entry:array(field(input,"head_to_head"))){ClubId club{number(field(entry,"club"))};const auto& scores=array(field(entry,"scores"));if(scores.size()!=10)throw FormatError("world12: invalid opponent result count");std::array<uint8_t,10> row;for(size_t i=0;i<10;++i)row[i]=uint8_t(number(scores[i],255));if(!result.head_to_head.emplace(club,row).second)throw FormatError("world12: repeated manager opponent history");}
    return result;
}

J encode_markets(const std::optional<CareerMarkets>& input) {
    if(!input)return nullptr;
    A stocks,funds;for(auto b:input->stocks)stocks.emplace_back(int(b));for(auto b:input->funds)funds.emplace_back(int(b));
    return O{{"stocks",std::move(stocks)},{"funds",std::move(funds)},{"first_default_name",int(input->first_default_name)}};
}
std::optional<CareerMarkets> decode_markets(const J& input) {
    if(std::holds_alternative<std::nullptr_t>(input.value))return {};
    CareerMarkets result;
    const auto bytes=[&](const char* name,auto& output){const auto& values=array(field(input,name));if(values.size()!=output.size())throw FormatError("world12: invalid market dimensions");for(size_t i=0;i<values.size();++i)output[i]=uint8_t(number(values[i],255));};
    bytes("stocks",result.stocks);bytes("funds",result.funds);result.first_default_name=uint8_t(number(field(input,"first_default_name"),1));return result;
}

J encode_free_players(const std::optional<CareerPlayerPools>& pools) {
    if(!pools)return nullptr;
    const auto refs=[](const auto& input){A output;for(auto player:input)output.push_back(player.valid()?id(player.value):J(nullptr));return output;};
    return O{{"domestic",refs(pools->domestic)},{"foreign",refs(pools->foreign)}};
}
std::optional<CareerPlayerPools> decode_free_players(const J& input) {
    if(std::holds_alternative<std::nullptr_t>(input.value))return {};
    CareerPlayerPools result;
    const auto refs=[&](const char* name,auto& output){const auto& values=array(field(input,name));if(values.size()!=output.size())throw FormatError("world12: invalid free-player dimensions");for(size_t i=0;i<values.size();++i){if(values[i].integer())output[i]={number(values[i])};else if(!std::holds_alternative<std::nullptr_t>(values[i].value))throw FormatError("world12: invalid free-player identity");}};
    refs("domestic",result.domestic);refs("foreign",result.foreign);return result;
}

J encode_pools(const std::optional<CareerPools>& input) {
    if(!input)return J(nullptr);A managers,previous;
    for(const auto& c:input->managers){A tips;for(auto v:c.tips)tips.emplace_back(int(v));managers.emplace_back(O{{"stake",id(c.stake)},{"winnings",id(c.winnings)},{"tips",std::move(tips)},{"correct",int(c.correct)},{"voided",c.voided}});}
    for(auto v:input->previous_outcomes)previous.emplace_back(int(v));return O{{"appointment",input->appointment?J(O{{"season",id(input->appointment->first)},{"day",id(input->appointment->second)}}):J(nullptr)},{"managers",std::move(managers)},{"previous",std::move(previous)}};
}
std::optional<CareerPools> decode_pools(const J& input) {
    if(std::holds_alternative<std::nullptr_t>(input.value))return {};CareerPools pools;
    const auto& appointment=field(input,"appointment");if(appointment.object())pools.appointment=std::make_pair(number(field(appointment,"season")),number(field(appointment,"day")));else if(!std::holds_alternative<std::nullptr_t>(appointment.value))throw FormatError("world12: invalid pools appointment");
    const auto& managers=array(field(input,"managers"));const auto& previous=array(field(input,"previous"));
    if(managers.size()!=4||previous.size()!=11)throw FormatError("world12: invalid pools dimensions");
    for(size_t i=0;i<4;++i){const auto& j=managers[i];auto& c=pools.managers[i];c.stake=number(field(j,"stake"),400000);if(c.stake&&c.stake<10000)throw FormatError("world12: invalid pools stake");c.winnings=number(field(j,"winnings"),4294967295u);c.correct=uint8_t(number(field(j,"correct"),11));c.voided=boolean(field(j,"voided"));const auto& tips=array(field(j,"tips"));if(tips.size()!=11)throw FormatError("world12: invalid pools tips");for(size_t n=0;n<11;++n)c.tips[n]=uint8_t(number(tips[n],2));}
    for(size_t n=0;n<11;++n)pools.previous_outcomes[n]=uint8_t(number(previous[n],2));return pools;
}
J encode_economy(const std::optional<CareerEconomy>& input) {
    if(!input)return nullptr;
    const auto numbers=[](const auto& values){A out;for(auto v:values)out.push_back(id(v));return out;};
    A managers,messages;
    for(const auto& m:input->managers){A loans,history;for(const auto& row:m.loans)loans.emplace_back(numbers(row));for(const auto& row:m.financial_history)history.emplace_back(numbers(row));
        J monthly=nullptr;if(m.monthly)monthly=O{{"history_count",int(m.monthly->history_count)},{"expenses",numbers(m.monthly->expenses)},{"counters",numbers(m.monthly->counters)}};
        managers.emplace_back(O{{"depreciation",m.depreciation?id(*m.depreciation):J(nullptr)},{"bank_robbery_month",m.bank_robbery_month?id(*m.bank_robbery_month):J(nullptr)},{"monthly",std::move(monthly)},{"deposit",id(m.deposit)},{"interest_income",id(m.interest_income)},{"overdraft_interest",id(m.overdraft_interest)},
            {"loans",std::move(loans)},{"stand_flags",numbers(m.stand_flags)},{"planned_stand_capacity",numbers(m.planned_stand_capacity)},{"submitted_balance_plan",id(m.submitted_balance_plan)},{"construction_weeks",numbers(m.construction_weeks)},{"offers",numbers(m.offers)},
            {"camp_weeks",int(m.camp_weeks)},{"camp_flags",int(m.camp_flags)},{"camp_bill",id(m.camp_bill)},{"weekly_count",int(m.weekly_count)},
            {"facility_history",numbers(m.facility_history)},{"financial_history",std::move(history)}});
    }
    for(const auto& m:input->messages)messages.emplace_back(O{{"recipients",id(m.recipients)},{"read_by",id(m.read_by)},{"date",int(m.date)},{"text",numbers(m.text)}});
    return O{{"presentation_options",input->presentation_options?J(numbers(*input->presentation_options)):J(nullptr)},{"pools",encode_pools(input->pools)},{"advertising_group",input->advertising_group?J(int(*input->advertising_group)):J(nullptr)},{"week",int(input->week)},{"managers",std::move(managers)},{"messages",std::move(messages)},{"recipient_mask",id(input->recipient_mask)}};
}
std::optional<CareerEconomy> decode_economy(const J& input,uint32_t version) {
    if(std::holds_alternative<std::nullptr_t>(input.value))return {};
    CareerEconomy result;result.week=uint8_t(number(field(input,"week"),11));result.recipient_mask=number(field(input,"recipient_mask"),4294967295u);
    if(version>=30)result.pools=decode_pools(field(input,"pools"));
    else if(const auto found=input.object()->find("pools");found!=input.object()->end()&&!std::holds_alternative<std::nullptr_t>(found->second.value))throw FormatError("world12: older schema cannot restore pools coupons");
    if(version>=28){const auto& value=field(input,"advertising_group");if(value.integer())result.advertising_group=uint8_t(number(value,255));else if(!std::holds_alternative<std::nullptr_t>(value.value))throw FormatError("world12: invalid advertising group");}
    else if(const auto found=input.object()->find("advertising_group");found!=input.object()->end()&&!std::holds_alternative<std::nullptr_t>(found->second.value))throw FormatError("world12: older schema cannot restore advertising group");
    const auto numbers=[](const J& j,auto& output){const auto& a=array(j);if(a.size()!=output.size())throw FormatError("world12: invalid economy row dimensions");
        using T=typename std::remove_reference_t<decltype(output)>::value_type;for(size_t i=0;i<a.size();++i)output[i]=T(number(a[i],std::numeric_limits<T>::max()));};
    if(version>=31){const auto& value=field(input,"presentation_options");if(value.array())numbers(value,result.presentation_options.emplace());else if(!std::holds_alternative<std::nullptr_t>(value.value))throw FormatError("world12: invalid presentation options");}
    else if(const auto found=input.object()->find("presentation_options");found!=input.object()->end()&&!std::holds_alternative<std::nullptr_t>(found->second.value))throw FormatError("world12: older schema cannot restore presentation options");
    const auto& managers=array(field(input,"managers"));if(managers.size()!=4)throw FormatError("world12: invalid economy manager count");
    for(size_t i=0;i<4;++i){const auto& j=managers[i];auto& m=result.managers[i];
        if(!j.object())throw FormatError("world12: invalid economy manager record");
        if(version>=26){const auto& value=field(j,"depreciation");if(value.integer())m.depreciation=number(value,4294967295u);else if(!std::holds_alternative<std::nullptr_t>(value.value))throw FormatError("world12: invalid depreciation account");}
        else if(const auto found=j.object()->find("depreciation");found!=j.object()->end()&&!std::holds_alternative<std::nullptr_t>(found->second.value))throw FormatError("world12: older schema cannot restore depreciation");
        if(version>=22){const auto& month=field(j,"bank_robbery_month");if(month.integer())m.bank_robbery_month=number(month);else if(!std::holds_alternative<std::nullptr_t>(month.value))throw FormatError("world12: invalid bank robbery month guard");}
        if(version>=20){const auto& monthly=field(j,"monthly");if(monthly.object()){auto& value=m.monthly.emplace();value.history_count=uint8_t(number(field(monthly,"history_count"),12));numbers(field(monthly,"expenses"),value.expenses);numbers(field(monthly,"counters"),value.counters);}else if(!std::holds_alternative<std::nullptr_t>(monthly.value))throw FormatError("world12: invalid monthly economy");}
        m.deposit=number(field(j,"deposit"),4294967295u);m.interest_income=number(field(j,"interest_income"),4294967295u);m.overdraft_interest=number(field(j,"overdraft_interest"),4294967295u);
        const auto& loans=array(field(j,"loans"));const auto& history=array(field(j,"financial_history"));if(loans.size()!=5||history.size()!=4)throw FormatError("world12: invalid loan/history row count");
        for(size_t row=0;row<5;++row)numbers(loans[row],m.loans[row]);for(size_t row=0;row<4;++row)numbers(history[row],m.financial_history[row]);
        numbers(field(j,"construction_weeks"),m.construction_weeks);if(version>=17){numbers(field(j,"stand_flags"),m.stand_flags);numbers(field(j,"planned_stand_capacity"),m.planned_stand_capacity);m.submitted_balance_plan=number(field(j,"submitted_balance_plan"),4294967295u);}else if(std::any_of(m.construction_weeks.begin()+22,m.construction_weeks.end(),[](auto weeks){return weeks!=0;}))throw FormatError("world12: schema16 cannot restore unfinished stand targets");numbers(field(j,"offers"),m.offers);numbers(field(j,"facility_history"),m.facility_history);
        m.camp_weeks=uint8_t(number(field(j,"camp_weeks"),255));m.camp_flags=uint8_t(number(field(j,"camp_flags"),255));m.camp_bill=number(field(j,"camp_bill"),4294967295u);m.weekly_count=uint8_t(number(field(j,"weekly_count"),52));
    }
    const auto& messages=array(field(input,"messages"));if(messages.size()>40)throw FormatError("world12: too many economy messages");
    for(const auto& j:messages){EconomyMessage m;m.recipients=number(field(j,"recipients"),4294967295u);if(version<21&&!m.recipients)throw FormatError("world12: sparse mailbox requires schema21");m.read_by=number(field(j,"read_by"),4294967295u);m.date=uint16_t(number(field(j,"date"),65535));numbers(field(j,"text"),m.text);result.messages.push_back(m);}
    return result;
}

J encode_settings(const std::optional<CareerSettings>& input) {
    if(!input)return nullptr;
    return O{{"mode",int(input->mode)},{"difficulty",int(input->difficulty)},{"duration",int(input->duration)},{"planned_managers",int(input->planned_managers)},{"foreign_rule",input->foreign_rule}};
}
std::optional<CareerSettings> decode_settings(const J& input) {
    if(std::holds_alternative<std::nullptr_t>(input.value))return {};
    return CareerSettings{uint8_t(number(field(input,"mode"),3)),uint8_t(number(field(input,"difficulty"),9)),uint8_t(number(field(input,"duration"),5)),uint8_t(number(field(input,"planned_managers"),4)),boolean(field(input,"foreign_rule"))};
}
J encode_season_services(const std::optional<ManagerSeasonServices>& input) {
    if(!input)return nullptr;
    const auto numbers=[](const auto& values){A out;for(auto v:values)out.emplace_back(int(v));return out;};
    A offers,contracts;for(const auto& row:input->advertising_offers)offers.emplace_back(numbers(row));for(const auto& row:input->advertising_contracts)contracts.emplace_back(numbers(row));
    return O{{"tickets_sold",int(input->tickets_sold)},{"ticket_income",id(input->ticket_income)},{"total_ticket_income",id(input->total_ticket_income)},{"offers",std::move(offers)},{"contracts",std::move(contracts)},
        {"history_count",int(input->strength_history_count)},{"strength_history",numbers(input->strength_history)},{"position_history",numbers(input->position_history)}};
}
std::optional<ManagerSeasonServices> decode_season_services(const J& input) {
    if(std::holds_alternative<std::nullptr_t>(input.value))return {};
    ManagerSeasonServices value;value.tickets_sold=uint16_t(number(field(input,"tickets_sold"),65535));value.ticket_income=number(field(input,"ticket_income"),4294967295u);value.total_ticket_income=number(field(input,"total_ticket_income"),4294967295u);
    const auto numbers=[](const J& j,auto& output){const auto& values=array(j);if(values.size()!=output.size())throw FormatError("world12: invalid seasonal record dimensions");for(size_t i=0;i<values.size();++i)output[i]=uint8_t(number(values[i],255));};
    const auto rows=[&](const char* name,auto& output){const auto& values=array(field(input,name));if(values.size()!=output.size())throw FormatError("world12: invalid advertising row count");for(size_t i=0;i<values.size();++i)numbers(values[i],output[i]);};
    rows("offers",value.advertising_offers);rows("contracts",value.advertising_contracts);value.strength_history_count=uint8_t(number(field(input,"history_count"),38));numbers(field(input,"strength_history"),value.strength_history);numbers(field(input,"position_history"),value.position_history);return value;
}
J encode_manager_career(const std::optional<ManagerCareerState>& input) {
    if(!input)return nullptr;
    const auto bytes=[](const auto& values){A out;for(auto value:values)out.emplace_back(int(value));return out;};
    const auto& m=*input;A youth,budgets,sponsors,attempts;
    for(const auto& c:m.environment.youth)youth.emplace_back(O{{"strength",int(c.strength)},{"position",int(c.position)},{"league",int(c.league)}});
    for(auto value:m.environment.monthly)budgets.emplace_back(J(int64_t(value)));
    for(const auto& c:m.sponsors)sponsors.emplace_back(O{{"amount",id(c.amount)},{"months",int(c.months)}});
    for(auto player:m.poach_attempts)attempts.push_back(player.valid()?id(player.value):J(nullptr));
    return O{{"cup_titles",m.cup_titles?J(bytes(*m.cup_titles)):J(nullptr)},{"strengths_display_word",m.strengths_display_word?J(int(*m.strengths_display_word)):J(nullptr)},{"pairings_display_byte",m.pairings_display_byte?J(int(*m.pairings_display_byte)):J(nullptr)},{"table_display_word",m.table_display_word?J(int(*m.table_display_word)):J(nullptr)},{"other_games_view",m.other_games_view?J(int(*m.other_games_view)):J(nullptr)},{"stadium_curve_flags",m.stadium_curve_flags?J(int(*m.stadium_curve_flags)):J(nullptr)},{"camp_focus",m.camp_focus?J(bytes(*m.camp_focus)):J(nullptr)},{"stadium",O{{"facility_values",bytes(m.stadium.facility_values)},{"stand_condition",bytes(m.stadium.stand_condition)},{"facility_levels",bytes(m.stadium.facility_levels)},{"stand_capacity",bytes(m.stadium.stand_capacity)},{"capacity_sum",int(m.stadium.capacity_sum)}}},
        {"environment",O{{"youth",std::move(youth)},{"monthly",std::move(budgets)}}},{"sponsors",std::move(sponsors)},{"board_patience",int(m.board_patience)},{"training_plan",int(m.training_plan)},{"transfer_filters",bytes(m.transfer_filters)},{"poach_attempts",std::move(attempts)},{"season_services",encode_season_services(m.season_services)},{"season_records",m.season_records?J(bytes(*m.season_records)):J(nullptr)}};
}
std::optional<ManagerCareerState> decode_manager_career(const J& input,uint32_t version) {
    if(std::holds_alternative<std::nullptr_t>(input.value))return {};
    ManagerCareerState m;const auto& stadium=field(input,"stadium");
    if(version>=35){const auto& titles=field(input,"cup_titles");if(titles.array()){if(titles.array()->size()!=2)throw FormatError("world12: invalid cup title count dimensions");m.cup_titles.emplace();for(size_t i=0;i<2;++i)(*m.cup_titles)[i]=uint16_t(number(titles.array()->at(i),65535));}else if(!std::holds_alternative<std::nullptr_t>(titles.value))throw FormatError("world12: invalid cup title counts");}
    else if(const auto titles=input.object()->find("cup_titles");titles!=input.object()->end()&&!std::holds_alternative<std::nullptr_t>(titles->second.value))throw FormatError("world12: older schema cannot restore cup title counts");
    const auto bytes=[](const J& object,const char* name,auto& output){const auto& values=array(field(object,name));if(values.size()!=output.size())throw FormatError("world12: invalid manager career dimensions");for(size_t i=0;i<values.size();++i)output[i]=uint8_t(number(values[i],255));};
    const auto optional_preference=[&](const char* key,uint32_t maximum)->std::optional<uint32_t>{
        const auto found=input.object()->find(key);
        if(version<34){if(found!=input.object()->end()&&!std::holds_alternative<std::nullptr_t>(found->second.value))throw FormatError("world12: older schema cannot restore family preferences");return {};}
        const auto& value=field(input,key);if(value.integer())return uint32_t(number(value,maximum));
        if(!std::holds_alternative<std::nullptr_t>(value.value))throw FormatError("world12: invalid family preferences");return {};
    };
    if(auto value=optional_preference("strengths_display_word",65535))m.strengths_display_word=uint16_t(*value);
    if(auto value=optional_preference("pairings_display_byte",255))m.pairings_display_byte=uint8_t(*value);
    if(version>=33){const auto& value=field(input,"table_display_word");if(value.integer())m.table_display_word=uint16_t(number(value,65535));else if(!std::holds_alternative<std::nullptr_t>(value.value))throw FormatError("world12: invalid table display word");}
    else if(const auto found=input.object()->find("table_display_word");found!=input.object()->end()&&!std::holds_alternative<std::nullptr_t>(found->second.value))throw FormatError("world12: older schema cannot restore table display word");
    if(version>=32){const auto& value=field(input,"other_games_view");if(value.integer())m.other_games_view=uint8_t(number(value,255));else if(!std::holds_alternative<std::nullptr_t>(value.value))throw FormatError("world12: invalid other-games view");}
    else if(const auto found=input.object()->find("other_games_view");found!=input.object()->end()&&!std::holds_alternative<std::nullptr_t>(found->second.value))throw FormatError("world12: older schema cannot restore other-games view");
    if(version>=29){const auto& value=field(input,"stadium_curve_flags");if(value.integer())m.stadium_curve_flags=uint8_t(number(value,255));else if(!std::holds_alternative<std::nullptr_t>(value.value))throw FormatError("world12: invalid stadium curve flags");}
    else if(const auto found=input.object()->find("stadium_curve_flags");found!=input.object()->end()&&!std::holds_alternative<std::nullptr_t>(found->second.value))throw FormatError("world12: older schema cannot restore stadium curve flags");
    if(version>=27){const auto& value=field(input,"camp_focus");if(value.array()){if(value.array()->size()!=4)throw FormatError("world12: invalid camp focus count");m.camp_focus.emplace();for(size_t i=0;i<4;++i)(*m.camp_focus)[i]=uint8_t(number(value.array()->at(i),15));}else if(!std::holds_alternative<std::nullptr_t>(value.value))throw FormatError("world12: invalid camp focus");}
    else if(const auto found=input.object()->find("camp_focus");found!=input.object()->end()&&!std::holds_alternative<std::nullptr_t>(found->second.value))throw FormatError("world12: older schema cannot restore camp focus");
    bytes(stadium,"facility_values",m.stadium.facility_values);bytes(stadium,"stand_condition",m.stadium.stand_condition);bytes(stadium,"facility_levels",m.stadium.facility_levels);bytes(stadium,"stand_capacity",m.stadium.stand_capacity);
    m.stadium.capacity_sum=uint8_t(number(field(stadium,"capacity_sum"),255));
    const auto& environment=field(input,"environment");const auto& youth=array(field(environment,"youth"));const auto& budgets=array(field(environment,"monthly"));const auto& sponsors=array(field(input,"sponsors"));const auto& attempts=array(field(input,"poach_attempts"));
    if(youth.size()!=3||budgets.size()!=6||sponsors.size()!=3||attempts.size()!=5)throw FormatError("world12: invalid initial environment/sponsor/attempt count");
    for(size_t i=0;i<3;++i){const auto& c=youth[i];m.environment.youth[i]={uint8_t(number(field(c,"strength"),255)),uint8_t(number(field(c,"position"),11)),uint8_t(number(field(c,"league"),2))};
        m.sponsors[i]={number(field(sponsors[i],"amount"),4294967295u),uint8_t(number(field(sponsors[i],"months"),255))};}
    for(size_t i=0;i<6;++i)m.environment.monthly[i]=signed_number(budgets[i]);
    for(size_t i=0;i<5;++i){if(attempts[i].integer())m.poach_attempts[i]={number(attempts[i])};else if(!std::holds_alternative<std::nullptr_t>(attempts[i].value))throw FormatError("world12: invalid poaching attempt reference");}
    m.board_patience=uint8_t(number(field(input,"board_patience"),255));m.training_plan=uint8_t(number(field(input,"training_plan"),255));bytes(input,"transfer_filters",m.transfer_filters);if(version>=14)m.season_services=decode_season_services(field(input,"season_services"));if(version>=13){const auto& records=field(input,"season_records");if(records.array()){m.season_records.emplace();bytes(input,"season_records",*m.season_records);}else if(!std::holds_alternative<std::nullptr_t>(records.value))throw FormatError("world12: invalid season history");}return m;
}

J encode_european_campaigns(const std::optional<EuropeanCampaigns>& input) {
    if(!input)return J(nullptr);A campaigns;
    const auto ref=[](ClubId club){return club.valid()?id(club.value):J(nullptr);};
    const auto refs=[&](const std::vector<ClubId>& clubs){A out;for(auto club:clubs)out.push_back(ref(club));return J(std::move(out));};
    for(const auto& c:*input){A stages;for(const auto& stage:c.stages){const auto& r=stage.rules;A rules;for(auto value:{r.entry,r.flags,r.previous,r.next,r.parallel,r.teams,r.rounds,r.group_count,r.group_index,r.quota,r.seeds})rules.emplace_back(int(value));A scores;for(const auto& leg:stage.scores){A words;for(auto word:leg)words.emplace_back(int(word));scores.emplace_back(std::move(words));}stages.emplace_back(O{{"rules",std::move(rules)},{"played",int(stage.played)},{"teams",refs(stage.teams)},{"seeds",refs(stage.seeds)},{"scores",std::move(scores)},{"winner",ref(stage.winner)},{"runner_up",ref(stage.runner_up)}});}
        campaigns.emplace_back(O{{"competition",int(c.competition)},{"revision",std::to_string(c.revision)},{"entrants",refs(c.entrants)},{"stages",std::move(stages)},{"active_entry",int(c.active_entry)},{"complete",c.complete},{"winner",ref(c.winner)},{"runner_up",ref(c.runner_up)}});}
    return J(std::move(campaigns));
}
std::optional<EuropeanCampaigns> decode_european_campaigns(const J& input) {
    if(std::holds_alternative<std::nullptr_t>(input.value))return {};const auto& campaigns=array(input);if(campaigns.size()!=3)throw FormatError("world12: European campaigns require three programmes");EuropeanCampaigns result;
    const auto ref=[](const J& j){if(j.integer())return ClubId{number(j)};if(!std::holds_alternative<std::nullptr_t>(j.value))throw FormatError("world12: invalid European club reference");return ClubId{};};
    const auto refs=[&](const J& input){std::vector<ClubId> out;const auto& items=array(input);if(items.size()>128)throw FormatError("world12: European field exceeds capacity");for(const auto& item:items)out.push_back(ref(item));return out;};
    for(size_t k=0;k<3;++k){auto& c=result[k];const auto& j=campaigns[k];c.competition=EuropeanCompetition(number(field(j,"competition"),3));c.revision=statistics_number(field(j,"revision"));c.entrants=refs(field(j,"entrants"));c.active_entry=uint8_t(number(field(j,"active_entry"),35));c.complete=boolean(field(j,"complete"));c.winner=ref(field(j,"winner"));c.runner_up=ref(field(j,"runner_up"));const auto& stages=array(field(j,"stages"));if(stages.size()>8)throw FormatError("world12: European stage capacity exceeded");
        for(const auto& item:stages){EuropeanStage stage;const auto& rules=array(field(item,"rules"));if(rules.size()!=11)throw FormatError("world12: invalid European stage descriptor");std::array<uint8_t,11> r{};for(size_t i=0;i<11;++i)r[i]=uint8_t(number(rules[i],255));stage.rules={r[0],r[1],r[2],r[3],r[4],r[5],r[6],r[7],r[8],r[9],r[10]};stage.played=uint8_t(number(field(item,"played"),12));stage.teams=refs(field(item,"teams"));stage.seeds=refs(field(item,"seeds"));stage.winner=ref(field(item,"winner"));stage.runner_up=ref(field(item,"runner_up"));const auto& scores=array(field(item,"scores"));if(scores.size()!=2)throw FormatError("world12: invalid European score leg count");for(size_t leg=0;leg<2;++leg){const auto& words=array(scores[leg]);if(words.size()>64)throw FormatError("world12: European score capacity exceeded");for(const auto& word:words)stage.scores[leg].push_back(uint16_t(number(word,65535)));}c.stages.push_back(std::move(stage));}}
    return result;
}
J encode_initial_competitions(const std::optional<InitialCompetitionFields>& input) {
    if(!input)return nullptr;const auto refs=[](const auto& values){A out;for(auto value:values)out.push_back(id(value.value));return out;};
    A champions,domestic;for(const auto& [country,club]:input->champions)champions.emplace_back(O{{"country",int(country)},{"club",id(club.value)}});for(const auto& [country,club]:input->domestic_holders)domestic.emplace_back(O{{"country",int(country)},{"club",id(club.value)}});
    A fields,qualified;for(size_t k=0;k<3;++k){fields.emplace_back(refs(input->fields[k]));qualified.emplace_back(refs(input->qualified[k]));}
    return O{{"fields",std::move(fields)},{"qualified",std::move(qualified)},{"holders",refs(input->holders)},{"national_field",refs(input->national_field)},{"champions",std::move(champions)},{"domestic_holders",std::move(domestic)},{"manager_services_complete",input->manager_services_complete}};
}
std::optional<InitialCompetitionFields> decode_initial_competitions(const J& input,uint32_t version) {
    if(std::holds_alternative<std::nullptr_t>(input.value))return {};
    InitialCompetitionFields result;const auto& fields=array(field(input,"fields"));const auto& qualified=array(field(input,"qualified"));const auto& holders=array(field(input,"holders"));
    if(fields.size()!=3||qualified.size()!=3||holders.size()!=3)throw FormatError("world12: invalid initial competition dimensions");
    for(size_t k=0;k<3;++k){for(const auto& value:array(fields[k]))result.fields[k].push_back({number(value)});for(const auto& value:array(qualified[k]))result.qualified[k].push_back({number(value)});result.holders[k]={number(holders[k])};}
    for(const auto& value:array(field(input,"national_field")))result.national_field.push_back({number(value)});
    if(version>=14){result.manager_services_complete=boolean(field(input,"manager_services_complete"));const auto refs=[&](const char* name,auto& output){for(const auto& j:array(field(input,name))){const auto country=Country(number(field(j,"country"),61));if(std::any_of(output.begin(),output.end(),[&](const auto& row){return row.first==country;}))throw FormatError("world12: repeated domestic title country");output.push_back({country,ClubId{number(field(j,"club"))}});}std::sort(output.begin(),output.end(),[](const auto& a,const auto& b){return a.first<b.first;});};refs("champions",result.champions);refs("domestic_holders",result.domestic_holders);}return result;
}

J encode_market(const std::optional<TransferMarket>& market) {
    if(!market)return nullptr;A rows;
    for(const auto& row:market->rows){if(!row){rows.emplace_back(nullptr);continue;}A bids;for(const auto& bid:row->bids){A bonuses;for(auto b:bid.bonuses)bonuses.emplace_back(int(b));bids.emplace_back(O{{"club",bid.bidder.valid()?id(bid.bidder.value):J(nullptr)},{"fee",id(bid.fee)},{"salary",id(bid.salary)},{"months",int(bid.months)},{"bonuses",std::move(bonuses)}});}
        rows.emplace_back(O{{"player",id(row->player.value)},{"seller",row->seller.valid()?id(row->seller.value):J(nullptr)},{"human_seller",row->human_seller},{"loan",row->loan},{"fee",id(row->fee)},{"salary",id(row->salary)},{"asking_percent",int(row->asking_percent)},{"demand_percent",int(row->demand_percent)},{"weeks",int(row->weeks)},{"bids",std::move(bids)}});}
    A history;for(auto club:market->last_entry40_bidders)history.push_back(club.valid()?id(club.value):J(nullptr));
    return O{{"last_entry40_bidders",std::move(history)},{"entry40_history_known",market->entry40_history_known},{"rows",std::move(rows)},{"toggle",int(market->list_toggle)},{"checkpoint_game_seed",J(int64_t(market->checkpoint_game_seed))}};
}
std::optional<TransferMarket> decode_market(const J& input,uint32_t version) {
    if(std::holds_alternative<std::nullptr_t>(input.value))return {};TransferMarket market;market.list_toggle=uint8_t(number(field(input,"toggle"),1));market.checkpoint_game_seed=signed_number(field(input,"checkpoint_game_seed"));const auto& rows=array(field(input,"rows"));if(rows.size()!=44)throw FormatError("world12: invalid market row count");
    const auto optional_id=[](const J& j){if(j.integer())return ClubId{number(j)};if(!std::holds_alternative<std::nullptr_t>(j.value))throw FormatError("world12: invalid optional market club");return ClubId{};};
    for(size_t i=0;i<rows.size();++i){const auto& j=rows[i];if(std::holds_alternative<std::nullptr_t>(j.value))continue;MarketListing row;row.player={number(field(j,"player"))};row.seller=optional_id(field(j,"seller"));row.human_seller=boolean(field(j,"human_seller"));row.loan=boolean(field(j,"loan"));row.fee=number(field(j,"fee"),4294967295u);row.salary=number(field(j,"salary"),4294967295u);row.asking_percent=uint8_t(number(field(j,"asking_percent"),255));row.demand_percent=uint8_t(number(field(j,"demand_percent"),255));row.weeks=uint8_t(number(field(j,"weeks"),255));const auto& bids=array(field(j,"bids"));if(bids.size()!=4)throw FormatError("world12: invalid market bid count");
        for(size_t b=0;b<4;++b){const auto& value=bids[b];auto& bid=row.bids[b];bid.bidder=optional_id(field(value,"club"));bid.fee=number(field(value,"fee"),4294967295u);bid.salary=number(field(value,"salary"),4294967295u);bid.months=uint8_t(number(field(value,"months"),255));const auto& bonus=array(field(value,"bonuses"));if(bonus.size()!=7)throw FormatError("world12: invalid market bonus count");for(size_t k=0;k<7;++k)bid.bonuses[k]=uint8_t(number(bonus[k],255));}
        market.rows[i]=row;
    }
    if(version>=18){const auto& history=array(field(input,"last_entry40_bidders"));if(history.size()!=4)throw FormatError("world12: invalid special bid history");for(size_t n=0;n<4;++n)market.last_entry40_bidders[n]=optional_id(history[n]);market.entry40_history_known=boolean(field(input,"entry40_history_known"));}
    else if(market.rows[40])for(size_t n=0;n<4;++n)market.last_entry40_bidders[n]=market.rows[40]->bids[n].bidder;
    else market.entry40_history_known=false;
    return market;
}

}
Bytes encode_world(const World& world){
    world.validate();A players,clubs,members,crests,loans,managers;
    for(const auto&[key,p]:world.players)players.emplace_back(O{{"id",id(key.value)},{"name",p.attributes.name},{"form_word",id(p.attributes.w35c)},{"skill_word",id(p.attributes.w368)},{"identity_word",id(p.attributes.w364)},{"bonus",int(p.bonus)},{"cards",int(p.cards)}});
    for(const auto&[key,c]:world.clubs){A counters,opponents,dates;for(auto value:c.season_stats.counters)counters.emplace_back(int(value));for(auto value:c.season_stats.opponents)opponents.push_back(value.valid()?id(value.value):J(nullptr));for(auto value:c.season_stats.dates)dates.emplace_back(int(value));A roster;for(const auto& s:c.roster)roster.emplace_back(O{{"player",s.empty()?J(nullptr):id(s.player.value)},{"flags",int(s.flags)}});clubs.emplace_back(O{{"id",id(key.value)},{"name",c.name},{"country",int(c.country)},{"kind",int(c.kind)},{"league",c.league.valid()?id(c.league.value):J(nullptr)},{"crest",id(c.crest.value)},{"kit",int(c.kit)},{"rating",int(c.rating)},{"short_span",int(c.short_span)},{"match_flags",int(c.match_flags)},{"fairplay_points",int(c.fairplay_points)},{"morale",int(c.morale)},{"lineup_indoor",c.lineup_indoor},{"season_stats",O{{"counters",std::move(counters)},{"opponents",std::move(opponents)},{"dates",std::move(dates)}}},{"roster",std::move(roster)}});}
    for(const auto&m:world.membership){A ids;for(auto c:m.clubs)ids.push_back(id(c.value));members.emplace_back(O{{"league",id(m.league.value)},{"clubs",std::move(ids)}});}
    for(const auto&[key,crest]:world.crests)crests.emplace_back(O{{"id",id(key.value)},{"resource",crest.path},{"cell",int(crest.cell)}});
    for(const auto&l:world.loans)loans.emplace_back(O{{"player",id(l.player.value)},{"owner",id(l.owner.value)},{"borrower",id(l.borrower.value)},{"months",int(l.months)},{"source_slot",l.source_slot?J(int(*l.source_slot)):J(nullptr)}});
    for(const auto&m:world.managers){J finance=nullptr;if(m.finance){const auto& f=*m.finance;finance=O{{"balance",J(int64_t(f.balance))},{"credit",id(f.credit)},{"income",id(f.income)},{"expenses",id(f.expenses)},{"bonus_paid",id(f.bonus_paid)},{"baseline",f.baseline?J(O{{"starting_balance",id(f.baseline->starting_balance)},{"account_flags",id(f.baseline->account_flags)}}):J(nullptr)}};}J tactics=m.tactics?encode_tactics(*m.tactics):J(nullptr);managers.emplace_back(O{{"seat",int(m.seat)},{"name",m.name},{"club",id(m.club.value)},{"start_league",id(m.start_league.value)},{"portrait",int(m.portrait)},{"finance",std::move(finance)},{"tactics",std::move(tactics)},{"match_state",encode_manager_match(m.match_state)},{"career",encode_manager_career(m.career)}});}
    A nationals;
    for(const auto& squad:world.national_squads) {
        A roster;for(const auto& row:squad.roster)roster.emplace_back(O{
            {"player",row.empty()?J(nullptr):id(row.player.value)},{"flags",int(row.flags)}});
        nationals.emplace_back(O{{"team",id(squad.team.value)},{"nominated_on",id(squad.nominated_on)},
            {"coach",squad.coach?J(int(*squad.coach)):J(nullptr)},{"roster",std::move(roster)},
            {"tactics",encode_tactics(squad.tactics)},{"match_state",encode_manager_match(squad.match_state)}});
    }
    auto raw=cli::dump_json(O{{"format","openbmh-world-12"},{"version",int(kWorldSchemaVersion)},{"profile",kProfile},{"scope",O{{"germany",world.catalogue.scope.germany},{"england",world.catalogue.scope.england},{"international",world.catalogue.scope.international}}},{"national_squads",std::move(nationals)},{"player_statistics",encode_statistics(world.player_statistics)},{"players_initialized",world.players_initialized},{"economy",encode_economy(world.economy)},{"markets",encode_markets(world.markets)},{"free_players",encode_free_players(world.free_players)},{"settings",encode_settings(world.settings)},{"european_campaigns",encode_european_campaigns(world.european_campaigns)},{"initial_competitions",encode_initial_competitions(world.initial_competitions)},{"transfer_market",encode_market(world.transfer_market)},{"development_balance",J(int64_t(world.development_balance))},{"trend_rotation",int(world.trend_rotation)},{"players",std::move(players)},{"clubs",std::move(clubs)},{"leagues",std::move(members)},{"crests",std::move(crests)},{"loans",std::move(loans)},{"managers",std::move(managers)}});return Bytes(raw.begin(),raw.end());
}
World decode_world(const Bytes& bytes){
    if(bytes.size()>32*1024*1024)throw FormatError("world12: database too large");auto root=cli::parse_json(std::string(bytes.begin(),bytes.end()));const auto version=number(field(root,"version"));if(text(field(root,"format"))!="openbmh-world-12"||(version<1||version>kWorldSchemaVersion)||text(field(root,"profile"))!=kProfile)throw FormatError("world12: unsupported database profile/version");
    World w;
    if(version>=35)w.european_campaigns=decode_european_campaigns(field(root,"european_campaigns"));
    else if(const auto found=root.object()->find("european_campaigns");found!=root.object()->end()&&!std::holds_alternative<std::nullptr_t>(found->second.value))throw FormatError("world12: older schema cannot restore European campaigns");
    if(version>=24) {
        const auto& squads=array(field(root,"national_squads"));
        if(squads.size()>2)throw FormatError("world12: national squad capacity exceeded");
        for(const auto& input:squads) {
            NationalSquad squad;squad.team={number(field(input,"team"))};
            squad.nominated_on=number(field(input,"nominated_on"),4294967295u);
            const auto& coach=field(input,"coach");
            if(coach.integer())squad.coach=uint8_t(number(coach,3));
            else if(!std::holds_alternative<std::nullptr_t>(coach.value))throw FormatError("world12: invalid national coach");
            const auto& roster=array(field(input,"roster"));
            if(roster.size()!=24)throw FormatError("world12: national roster requires 24 slots");
            for(size_t i=0;i<24;++i) {
                const auto& player=field(roster[i],"player");
                if(player.integer())squad.roster[i].player={number(player)};
                else if(!std::holds_alternative<std::nullptr_t>(player.value))throw FormatError("world12: invalid national player");
                squad.roster[i].flags=uint16_t(number(field(roster[i],"flags"),15));
            }
            squad.tactics=decode_tactics(field(input,"tactics"),version);
            const auto match=decode_manager_match(field(input,"match_state"),version);
            if(!match)throw FormatError("world12: missing national match state");
            squad.match_state=*match;w.national_squads.push_back(std::move(squad));
        }
    }else if(const auto found=root.object()->find("national_squads");found!=root.object()->end()&&!array(found->second).empty())
        throw FormatError("world12: older schema cannot restore national squads");
    if(version>=23)w.player_statistics=decode_statistics(field(root,"player_statistics"));
    else if(const auto found=root.object()->find("player_statistics");found!=root.object()->end()&&!array(found->second).empty())
        throw FormatError("world12: older schema cannot restore wide player statistics");
    if(version>=16)w.economy=decode_economy(field(root,"economy"),version);if(version>=15)w.transfer_market=decode_market(field(root,"transfer_market"),version);if(version>=13)w.initial_competitions=decode_initial_competitions(field(root,"initial_competitions"),version);if(version>=12)w.settings=decode_settings(field(root,"settings"));if(version>=11)w.free_players=decode_free_players(field(root,"free_players"));if(version>=10)w.markets=decode_markets(field(root,"markets"));if(version>=8)w.players_initialized=boolean(field(root,"players_initialized"));if(version>=4){w.development_balance=signed_number(field(root,"development_balance"));w.trend_rotation=uint8_t(number(field(root,"trend_rotation"),2));}if(version>=2){const auto& scope=field(root,"scope");w.catalogue=Catalogue::dual_classic({boolean(field(scope,"germany")),boolean(field(scope,"england")),boolean(field(scope,"international"))});}for(const auto& j:array(field(root,"players"))){PlayerRecord p;p.id={number(field(j,"id"))};p.attributes={text(field(j,"name")),number(field(j,"form_word"),4294967295u),number(field(j,"skill_word"),4294967295u),number(field(j,"identity_word"),4294967295u)};p.bonus=uint8_t(number(field(j,"bonus"),255));if(version>=3)p.cards=uint16_t(number(field(j,"cards"),65535));if(!w.players.emplace(p.id,p).second)throw FormatError("world12: duplicate player ID");}
    for(const auto&j:array(field(root,"clubs"))){ClubRecord c;c.id={number(field(j,"id"))};c.name=text(field(j,"name"));c.country=Country(number(field(j,"country"),61));c.kind=ClubKind(number(field(j,"kind"),1));const auto& l=field(j,"league");if(l.integer())c.league={number(l,uint32_t(kLeagueSlots-1))};else if(!std::holds_alternative<std::nullptr_t>(l.value))throw FormatError("world12: invalid optional league");c.crest={number(field(j,"crest"))};c.kit=uint8_t(number(field(j,"kit"),255));c.rating=uint8_t(number(field(j,"rating"),255));c.short_span=uint8_t(number(field(j,"short_span"),255));if(version>=3){c.match_flags=uint16_t(number(field(j,"match_flags"),65535));c.morale=uint8_t(number(field(j,"morale"),63));}if(version>=9)c.fairplay_points=uint16_t(number(field(j,"fairplay_points"),65535));if(version>=7)c.lineup_indoor=boolean(field(j,"lineup_indoor"));if(version>=4){const auto& stats=field(j,"season_stats");const auto& counters=array(field(stats,"counters"));const auto& opponents=array(field(stats,"opponents"));const auto& dates=array(field(stats,"dates"));if(counters.size()!=42||opponents.size()!=8||dates.size()!=8)throw FormatError("world12: invalid club statistics dimensions");for(size_t i=0;i<42;++i)c.season_stats.counters[i]=uint8_t(number(counters[i],255));for(size_t i=0;i<8;++i){if(opponents[i].integer())c.season_stats.opponents[i]={number(opponents[i])};else if(!std::holds_alternative<std::nullptr_t>(opponents[i].value))throw FormatError("world12: invalid statistics opponent");c.season_stats.dates[i]=uint16_t(number(dates[i],65535));}}
const auto& roster=array(field(j,"roster"));if(roster.size()!=24)throw FormatError("world12: roster must have 24 positions");for(size_t i=0;i<24;++i){const auto&p=field(roster[i],"player");if(p.integer())c.roster[i].player={number(p)};else if(!std::holds_alternative<std::nullptr_t>(p.value))throw FormatError("world12: invalid optional player");c.roster[i].flags=uint16_t(number(field(roster[i],"flags"),15));}if(!w.clubs.emplace(c.id,c).second)throw FormatError("world12: duplicate club ID");}
    const auto& leagues=array(field(root,"leagues"));if(leagues.size()!=kLeagueSlots)throw FormatError("world12: exactly thirteen leagues required");bool seen[kLeagueSlots]{};for(const auto&j:leagues){LeagueId id{number(field(j,"league"),uint32_t(kLeagueSlots-1))};if(seen[id.value])throw FormatError("world12: duplicate league");seen[id.value]=true;auto&m=w.membership[id.value];m.league=id;for(const auto& c:array(field(j,"clubs")))m.clubs.push_back({number(c)});}
    for(const auto&j:array(field(root,"crests")))if(!w.crests.emplace(CrestId{number(field(j,"id"))},CrestResource{text(field(j,"resource")),uint8_t(number(field(j,"cell"),7))}).second)throw FormatError("world12: duplicate crest ID");
    for(const auto&j:array(field(root,"loans"))){Loan loan{{number(field(j,"player"))},{number(field(j,"owner"))},{number(field(j,"borrower"))},uint8_t(number(field(j,"months"),255))};if(version>=19){const auto& slot=field(j,"source_slot");if(slot.integer())loan.source_slot=uint16_t(number(slot,199));else if(!std::holds_alternative<std::nullptr_t>(slot.value))throw FormatError("world12: invalid optional loan slot");}w.loans.push_back(loan);}
    for(const auto&j:array(field(root,"managers"))){Manager manager{uint8_t(number(field(j,"seat"),3)),text(field(j,"name")),{number(field(j,"club"))},{number(field(j,"start_league"),uint32_t(kLeagueSlots-1))},uint8_t(number(field(j,"portrait"),11))};if(version>=5){const auto& f=field(j,"finance");if(f.object())manager.finance=ManagerFinance{signed_number(field(f,"balance")),number(field(f,"credit"),4294967295u),number(field(f,"income"),4294967295u),number(field(f,"expenses"),4294967295u),number(field(f,"bonus_paid"),4294967295u)};else if(!std::holds_alternative<std::nullptr_t>(f.value))throw FormatError("world12: invalid optional manager finances");}if(version>=6){const auto& t=field(j,"tactics");if(t.object()){
manager.tactics=decode_tactics(t,version);
}else if(!std::holds_alternative<std::nullptr_t>(t.value))throw FormatError("world12: invalid optional manager tactics");}if(version>=8&&manager.finance){const auto& baseline=field(field(j,"finance"),"baseline");if(baseline.object())manager.finance->baseline=ManagerFinanceBaseline{number(field(baseline,"starting_balance"),4294967295u),number(field(baseline,"account_flags"),4294967295u)};else if(!std::holds_alternative<std::nullptr_t>(baseline.value))throw FormatError("world12: invalid initial account baseline");}if(version>=9)manager.match_state=decode_manager_match(field(j,"match_state"),version);if(version>=12)manager.career=decode_manager_career(field(j,"career"),version);if(version<17&&manager.match_state&&manager.match_state->designated_takers&&manager.career&&manager.career->season_services)std::fill(manager.career->season_services->advertising_contracts[16].begin()+8,manager.career->season_services->advertising_contracts[16].end(),uint8_t(0));w.managers.push_back(std::move(manager));}
    try{w.validate();}catch(const std::exception&e){throw FormatError(e.what());}return w;
}
}
