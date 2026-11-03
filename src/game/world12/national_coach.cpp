// SPDX-License-Identifier: GPL-3.0-only
#include "game/world12/national_coach.hpp"
#include "game/world12/national.hpp"
#include "game/world12/save.hpp"
#include "game/national_selection.hpp"
#include "game/world12/manager_finance.hpp"
#include "game/career.hpp"
#include "game/bank_robbery.hpp"
#include <algorithm>
#include <set>
namespace bmh::world12 {
namespace {
constexpr size_t record_size=0xb0c;
constexpr size_t monthly_offsets[]={0x338,0x33c,0x85c,0x860,0x864,0x86c,0x878,0x87c,0x880,0x884,0x888,0x88c,0x890,0x894,0x898};
uint32_t word(const Bytes& b,size_t p){uint32_t v=0;for(size_t n=0;n<4;++n)v|=uint32_t(b.at(p+n))<<(n*8);return v;}
void put(Bytes& b,size_t p,uint32_t v){for(size_t n=0;n<4;++n)b.at(p+n)=uint8_t(v>>(n*8));}
ClubId nation(const World& w,Country c){ClubId found;for(const auto& [id,club]:w.clubs)if(club.kind==ClubKind::national_team&&club.country==c){if(found.valid())throw FormatError("world12: ambiguous coach nation");found=id;}if(!found.valid())throw FormatError("world12: missing coach nation");return found;}
}
std::vector<NationalDesktopActor> national_desktop_actors(const CareerSnapshot& snapshot){
    snapshot.validate();const auto& w=snapshot.world;std::vector<NationalDesktopActor> result;if(!w.national_coaches)return result;
    const bool active=std::any_of(w.national_coaches->begin(),w.national_coaches->end(),[](const auto& record){return record.active;});if(!active)return result;if(!snapshot.desktop||snapshot.desktop->manager_order.empty())throw FormatError("world12: active national desktop lacks saved manager order");
    for(auto seat:snapshot.desktop->manager_order)for(const auto& record:*w.national_coaches)if(record.active&&record.coach==seat){const auto squad=std::find_if(w.national_squads.begin(),w.national_squads.end(),[&](const auto& value){return value.team==record.team;});if(squad==w.national_squads.end()||squad->coach!=seat)throw FormatError("world12: national desktop has no actual nominated coach squad");result.push_back({seat,record.team,w.clubs.at(record.team).country});}
    const auto expected=std::count_if(w.national_coaches->begin(),w.national_coaches->end(),[](const auto& record){return record.active;});if(result.size()!=size_t(expected))throw FormatError("world12: saved order omits an active national coach");return result;
}
bool national_roles_active(const World& w){return w.national_coaches&&std::any_of(w.national_coaches->begin(),w.national_coaches->end(),[](const auto& record){return record.active;});}
void park_national_coaches(World& w){if(!w.catalogue.scope.club_play())throw FormatError("world12: a national-only career has no parked club role");if(!w.national_coaches)return;auto candidate=w;for(auto& record:*candidate.national_coaches){record.coach.reset();record.active=false;}for(auto& squad:candidate.national_squads)squad.coach.reset();candidate.validate();w=std::move(candidate);}
NationalCapabilities national_capabilities(const World& w){
    w.validate();NationalCapabilities result;
    if(!w.catalogue.scope.international){result.unavailable_reason="International scope is disabled";return result;}
    const auto full=Catalogue::dual_classic();
    for(const auto& league:full.leagues){if(w.membership[league.id.value].clubs.size()==league.clubs)continue;size_t origins=0;if(w.national_origins)for(const auto& origin:*w.national_origins)origins+=origin.club.league==league.id;if(origins!=league.clubs){result.unavailable_reason="Complete imported national player origins are unavailable";return result;}}
    for(auto country:{Country::germany,Country::england}){
        try{const auto id=nation(w,country);if(w.players_initialized)(void)national_nomination(w,country);else {std::array<size_t,4> count{};const auto scan=[&](const ClubRecord& club){for(const auto& row:club.roster)if(!row.empty()){const auto& p=w.players.at(row.player);if(p.attributes.nationality()+((p.bonus&128)?32:0)==int(country))++count[size_t(p.attributes.position())];}};for(const auto& [id,club]:w.clubs)if(club.league.valid())scan(club);if(w.national_origins)for(const auto& origin:*w.national_origins)scan(origin.club);for(size_t position=0;position<4;++position)if(count[position]<NationalSquadSelection::quotas[position])throw FormatError("world12: imported national position pool is incomplete");}result.national_only_choices.push_back({country,id});}
        catch(const FormatError&){/* A real positional pool is required; no generated replacement. */}
    }
    if(result.national_only_choices.empty())result.unavailable_reason="The real imported rosters cannot supply a complete national squad";
    return result;
}
std::vector<std::pair<ClubId,uint8_t>> select_national_coaches(const World& w,const std::vector<uint8_t>& order){
    w.validate();if(!w.catalogue.scope.international||order.size()!=w.managers.size())throw FormatError("world12: national coach order is unavailable");
    std::set<uint8_t> seats;std::set<Country> selected;std::vector<std::pair<ClubId,uint8_t>> result;
    for(auto seat:order){const auto found=std::find_if(w.managers.begin(),w.managers.end(),[&](const auto& m){return m.seat==seat;});if(found==w.managers.end()||!seats.insert(seat).second)throw FormatError("world12: invalid saved national coach order");const auto country=w.manager_country(seat);if(country!=Country::germany&&country!=Country::england)throw FormatError("world12: unsupported human coach country");
        if(w.managers.size()>1){if(!found->career||!found->career->annual_records)throw FormatError("world12: national coach eligibility is unknown");if(!found->career->annual_records->national_eligibility)continue;}
        if(selected.insert(country).second)result.emplace_back(nation(w,country),seat);
    }
    return result;
}
NationalCoachAccount national_coach_account_from_legacy(const GameState& g,int local){
    if(local<0||local>3||g.manager_records.size()<size_t(local+1)*record_size)throw FormatError("world12: missing national financial record");const auto at=size_t(local)*record_size;NationalCoachAccount a;a.finance=manager_initial_finance_from_legacy(g,local);a.monthly.history_count=g.manager_records.at(at+0x286);
    for(size_t i=0;i<2;++i)a.monthly.expenses[i]=word(g.manager_records,at+0x854+i*4);for(size_t i=0;i<15;++i)a.monthly.counters[i]=word(g.manager_records,at+monthly_offsets[i]);a.investment_balance=word(g.manager_records,at+0x324);a.depreciation=word(g.manager_records,at+0x30c);a.deposit=word(g.manager_records,at+0x318);a.interest_income=word(g.manager_records,at+0x870);a.overdraft_interest=word(g.manager_records,at+0x89c);a.bank_robbery_month=bank_robbery_guard_month(g,local);std::copy_n(g.manager_records.begin()+ptrdiff_t(at+0xc5),13,a.training_preferences.begin());a.training_quality=uint16_t(g.manager_records.at(at+0x8ac)|uint16_t(g.manager_records.at(at+0x8ad))<<8);return a;
}
void install_national_coach_account(GameState& g,int local,const NationalCoachAccount& a){
    if(local<0||local>3||g.manager_records.size()<size_t(local+1)*record_size)throw FormatError("world12: missing national financial workspace");const auto at=size_t(local)*record_size;install_manager_finance(g,local,a.finance);g.manager_records[at+0x286]=a.monthly.history_count;for(size_t i=0;i<2;++i)put(g.manager_records,at+0x854+i*4,a.monthly.expenses[i]);for(size_t i=0;i<15;++i)put(g.manager_records,at+monthly_offsets[i],a.monthly.counters[i]);if(a.investment_balance)put(g.manager_records,at+0x324,*a.investment_balance);if(a.depreciation)put(g.manager_records,at+0x30c,*a.depreciation);put(g.manager_records,at+0x318,a.deposit);put(g.manager_records,at+0x870,a.interest_income);put(g.manager_records,at+0x89c,a.overdraft_interest);set_bank_robbery_guard_month(g,local,a.bank_robbery_month);std::copy(a.training_preferences.begin(),a.training_preferences.end(),g.manager_records.begin()+ptrdiff_t(at+0xc5));g.manager_records[at+0x8ac]=uint8_t(a.training_quality);g.manager_records[at+0x8ad]=uint8_t(a.training_quality>>8);
}
void initialize_national_coaches(World& w,const GameState& reference){
    w.validate();if(w.national_coaches||!w.settings||!w.players_initialized||!w.national_squads.empty()||w.national_campaigns||reference.manager_records.size()<5*record_size)throw FormatError("world12: national accounts require a fresh Source spare record");
    auto local=reference;initialize_national_manager_record(local);std::copy_n(local.manager_records.begin()+4*record_size,record_size,local.manager_records.begin());set_bank_robbery_guard_month(local,0,std::nullopt);const auto account=national_coach_account_from_legacy(local,0);auto candidate=w;candidate.national_coaches.emplace();for(auto country:{Country::germany,Country::england}){NationalCoachRecord record{nation(w,country),std::nullopt,false,account};const auto at=layout::Desktops+4*0x360;if(reference.memory.has(at+0x360-1)){auto& rows=record.desktop_layout.emplace();for(size_t r=0;r<54;++r)for(size_t b=0;b<16;++b)rows[r][b]=reference.memory.u8(at+uint32_t(r*16+b));}candidate.national_coaches->push_back(std::move(record));}candidate.validate();w=std::move(candidate);
}
void activate_national_coaches(World& w,const std::vector<uint8_t>& order){
    if(!w.national_coaches)throw FormatError("world12: national account history is unknown");const auto selection=select_national_coaches(w,order);auto candidate=w;for(auto& record:*candidate.national_coaches){record.coach.reset();record.active=false;for(const auto& [team,seat]:selection)if(team==record.team){record.coach=seat;record.active=true;}for(auto& squad:candidate.national_squads)if(squad.team==record.team)squad.coach=record.coach;}candidate.validate();w=std::move(candidate);
}
void validate_national_coaches(const World& w){
    if(!w.national_coaches)return;if(!w.catalogue.scope.international||!w.settings||!w.players_initialized||w.national_coaches->size()!=2)throw std::invalid_argument("world12: invalid national account context");std::set<ClubId> teams;std::set<uint8_t> coaches;
    for(const auto& record:*w.national_coaches){const auto found=w.clubs.find(record.team);
        if(w.catalogue.scope.club_play()&&(record.view_preferences||record.startup))throw std::invalid_argument("world12: national-only preferences/startup cannot replace parked club state");
        if(record.startup){const auto& receipt=*record.startup;if(!w.national_origins||!w.players_initialized||(receipt.primary!=Country::germany&&receipt.primary!=Country::england)||found==w.clubs.end()||found->second.country!=receipt.primary||receipt.initialized_players!=w.players.size()||receipt.origin_clubs!=w.national_origins->size()||receipt.origin_clubs!=Catalogue::dual_classic().league_clubs()||std::any_of(w.national_origins->begin(),w.national_origins->end(),[](const auto& origin){return !origin.source_initialized;}))throw std::invalid_argument("world12: invalid actual national startup receipt");for(auto bidder:receipt.entry40_bidders)if(bidder.valid()&&std::none_of(w.national_origins->begin(),w.national_origins->end(),[&](const auto& origin){return origin.club.id==bidder;}))throw std::invalid_argument("world12: national startup bidder has no actual origin");}
        if(found==w.clubs.end()||found->second.kind!=ClubKind::national_team||(found->second.country!=Country::germany&&found->second.country!=Country::england)||!teams.insert(record.team).second||record.active!=record.coach.has_value()||record.account.monthly.history_count>12||record.account.bank_robbery_month==0xffffffffu||!record.account.finance.baseline||(record.account.investment_balance&&*record.account.investment_balance!=record.account.finance.baseline->starting_balance)||std::any_of(record.account.training_preferences.begin(),record.account.training_preferences.end(),[](auto n){return n>3;}))throw std::invalid_argument("world12: invalid national coach account");if(record.coach){if(!coaches.insert(*record.coach).second||w.manager_country(*record.coach)!=found->second.country)throw std::invalid_argument("world12: repeated or foreign national coach");}for(const auto& squad:w.national_squads)if(squad.team==record.team&&(squad.coach!=record.coach||squad.match_state.staff_ratings[1]!=record.account.training_quality))throw std::invalid_argument("world12: squad and national account coach/quality differ");}
}
}
