// SPDX-License-Identifier: GPL-3.0-only
#include "game/world12/progression.hpp"
#include "game/world12/league_history.hpp"
#include <stdexcept>
#include <limits>
#include <algorithm>
#include <set>
#include <type_traits>
namespace bmh::world12 {
static_assert(std::is_nothrow_move_assignable_v<CareerSnapshot>);
std::vector<OtherAppointment> admitted_other_appointments(const CareerSnapshot& state,uint16_t day) {
    if(!state.calendar)throw FormatError("world12: country calendar is not installed");
    auto appointments=state.calendar->schedule.others_on(day);
    std::erase_if(appointments,[&](const OtherAppointment& appointment){
        if(appointment.domain==CalendarDomain::shared&&appointment.source_entry!=0x4006) {
            const auto flags=appointment.source_entry;
            if(flags&0x1e00)return !national_appointment(appointment,state.season);
        }
        if(appointment.domain!=CalendarDomain::germany||appointment.source_entry!=0x4002)return false;
        if(!state.world.economy||!state.world.economy->presentation_options)
            throw FormatError("world12: German indoor calendar admission needs saved presentation options");
        return (*state.world.economy->presentation_options)[16*9+4]!=0;
    });
    return appointments;
}
std::optional<DayTicket> prepare_day(const CareerSnapshot& state){
    if(!state.calendar)throw std::invalid_argument("world12: country calendar is not installed");const auto& cursor=*state.calendar;
    if(cursor.next_day==cursor.schedule.days)return std::nullopt;
    if(cursor.next_day>cursor.schedule.days)throw std::invalid_argument("world12: invalid calendar cursor");
    DayTicket ticket;ticket.day=cursor.next_day;ticket.revision=cursor.revision;ticket.others=admitted_other_appointments(state,ticket.day);
    // OpenBMH deviates from the original: shared English outdoor fixtures
    // precede Hall passes on a coincident date, with no repeated daily service.
    const bool hall=std::any_of(ticket.others.begin(),ticket.others.end(),german_indoor_appointment);
    bool ordinary_already=false;
    if(hall){
        if(!state.world.indoor)throw FormatError("world12: indoor campaign is unknown in this older snapshot");
        ticket.indoor_date=true;const auto next=cursor.schedule.others_on(uint16_t(ticket.day+1));ticket.indoor_total=std::any_of(next.begin(),next.end(),german_indoor_appointment)?3:2;
        if(state.world.indoor->date_pass){const auto& pass=*state.world.indoor->date_pass;if(pass.day!=ticket.day||pass.total!=ticket.indoor_total)throw FormatError("world12: indoor date pass belongs to another calendar cell");ticket.indoor_completed=pass.completed;ordinary_already=pass.ordinary_completed;}
        const bool ordinary=!cursor.schedule.leagues_on(ticket.day).empty()||std::any_of(ticket.others.begin(),ticket.others.end(),[](const auto& appointment){return !german_indoor_appointment(appointment);});
        ticket.indoor_ordinary_phase=ordinary&&!ordinary_already;
        if(ticket.indoor_ordinary_phase)std::erase_if(ticket.others,german_indoor_appointment);
        else {std::erase_if(ticket.others,[](const auto& appointment){return !german_indoor_appointment(appointment);});ticket.indoor=prepare_indoor_groups(*state.world.indoor);}
    }
    if(!hall||ticket.indoor_ordinary_phase)for(const auto&a:cursor.schedule.leagues_on(ticket.day)){if(state.league_rounds.at(a.league.value)+1!=a.round)throw std::invalid_argument("world12: league cursor disagrees with calendar");ticket.leagues.push_back({a,league_fixtures(state.world,a.league,a.round)});}
    std::set<DomesticCup> scheduled;
    for(const auto& appointment:ticket.others) if(const auto required=domestic_cups_on(appointment))
        for(auto cup:*required) {
            if(!scheduled.insert(cup).second) throw std::invalid_argument("world12: domestic cup scheduled twice on one day");
            const auto stage=std::find_if(state.cups.rbegin(),state.cups.rend(),[&](const auto& s){return s.cup==cup;});
            if(stage!=state.cups.rend() && !stage->complete()) {
                stage->validate(state.world);
                ticket.cups.push_back({cup,stage->source_entry,stage->next_leg(),stage->revision,stage->next_fixtures()});
            }
        }
    std::set<EuropeanCompetition> shared;
    for(const auto& appointment:ticket.others)if(const auto required=european_cups_on(appointment))for(auto cup:*required){
        if(!shared.insert(cup).second)throw FormatError("world12: European competition scheduled twice on one day");
        if(!state.world.european_campaigns)throw FormatError("world12: European campaign state is unknown in this snapshot");
        const auto& campaign=(*state.world.european_campaigns)[size_t(uint8_t(cup)-1)];
        if(campaign.complete)throw FormatError("world12: calendar schedules an already completed European programme");
        const auto groups=prepare_european_groups(campaign);ticket.european.insert(ticket.european.end(),groups.begin(),groups.end());
    }
    if(std::any_of(ticket.others.begin(),ticket.others.end(),regional_playoff_appointment))ticket.regional_playoff=prepare_regional_playoff_group(state);
    if(std::any_of(ticket.others.begin(),ticket.others.end(),[&](const auto& a){return national_appointment(a,state.season);})) {
        if(!state.world.national_campaigns)throw FormatError("world12: national campaign is unknown");
        const auto found=state.world.national_campaigns->seasons.find(state.season);
        if(found==state.world.national_campaigns->seasons.end()||found->second.complete)throw FormatError("world12: national calendar has no active current programme");
        ticket.national=prepare_national_groups(found->second);
        if(ticket.national.empty())throw FormatError("world12: national calendar has no pending group");
    }
    ticket.friendlies=prepare_friendly_groups(state);
    return ticket;
}
void commit_league_day(CareerSnapshot& state,const DayTicket& ticket,const std::vector<CompletedLeagueRound>& completed){
    commit_day(state,ticket,completed,{});
}
DayPresentation commit_day(CareerSnapshot& state,const DayTicket& ticket,const std::vector<CompletedLeagueRound>& completed,
    const std::vector<CupLegResult>& cup_results,const std::vector<EuropeanResults>& european_results,const LeImage* shared_rules,const std::optional<IndoorResults>& indoor_results,const GameState* indoor_reference,const std::optional<RegionalPlayoffResults>& regional_results,const std::optional<NationalResults>& national_results,const LeImage* national_rules,const std::vector<FriendlyResults>& friendly_results){
    const auto expected=prepare_day(state);if(!expected||*expected!=ticket)throw std::invalid_argument("world12: stale or changed day ticket");
    size_t required_count=0;std::vector<EuropeanCompetition> shared;
    for(const auto& appointment:ticket.others) {
        const auto required=domestic_cups_on(appointment);
        if(required)for(auto cup:*required) {
            const bool installed=std::any_of(state.cups.begin(),state.cups.end(),[&](const auto& stage){return stage.cup==cup;});
            // A reigning league/cup double deliberately owns no Super-Cup
            // stage. Keep its real calendar flag, without demanding a match.
            if(installed||domestic_cup_expected(state.world,cup))++required_count;
        }
        else if(const auto european=european_cups_on(appointment))shared.insert(shared.end(),european->begin(),european->end());
        else if(!german_indoor_appointment(appointment)&&!regional_playoff_appointment(appointment)&&!national_appointment(appointment,state.season)&&!friendly_appointment(appointment))throw std::invalid_argument("world12: shared/national transaction is required before advancing this day");
    }
    if(required_count!=ticket.cups.size() || cup_results.size()!=ticket.cups.size())
        throw std::invalid_argument("world12: complete installed domestic cup stages and results are required");
    if(european_results.size()!=shared.size()||(!shared.empty()&&!shared_rules))throw FormatError("world12: complete European results/source owner is required");
    for(size_t i=0;i<shared.size();++i)if(european_results[i].competition!=shared[i])throw FormatError("world12: European result order/competition does not match calendar");
    const bool needs_indoor=!ticket.indoor.empty();
    if(needs_indoor!=indoor_results.has_value()||(needs_indoor&&(!shared_rules||!indoor_reference)))throw FormatError("world12: complete indoor result/reference owner is required");
    if(ticket.regional_playoff.has_value()!=regional_results.has_value()||(regional_results&&!shared_rules))throw FormatError("world12: complete regional playoff results/source owner is required");
    if(friendly_results.size()!=ticket.friendlies.size())throw FormatError("world12: incomplete friendly day results");
    if(ticket.national.empty()==national_results.has_value()||(national_results&&!national_rules))throw FormatError("world12: complete national results and Source rules are required");
    if(national_results&&(national_results->season!=state.season||national_results->revision!=ticket.national.front().revision))throw FormatError("world12: stale national programme results");
    if(state.date==std::numeric_limits<uint32_t>::max()||state.calendar->revision==std::numeric_limits<uint64_t>::max())throw std::overflow_error("world12: date/revision exhausted");
    if(completed.size()!=ticket.leagues.size())throw std::invalid_argument("world12: incomplete day results");
    // Validate the entire result batch before writing any table or cursor.
    for(size_t i=0;i<completed.size();++i){const auto& got=completed[i];const auto& plan=ticket.leagues[i];if(got.league!=plan.appointment.league||got.round!=plan.appointment.round||got.results.size()!=plan.matches.size())throw std::invalid_argument("world12: score batch is for another competition/round");
        for(size_t m=0;m<got.results.size();++m){const auto&score=got.results[m];if(!score.played||score.home>15||score.away>15||state.results.result(state.world.catalogue,got.league,got.round,int(m)).played)throw std::invalid_argument("world12: missing, invalid or already committed result");}}
    auto staged=state;
    DayPresentation presentation;
    for(size_t i=0;i<cup_results.size();++i) {
        const auto& result=cup_results[i]; const auto& plan=ticket.cups[i];
        if(result.cup!=plan.cup || result.source_entry!=plan.source_entry || result.revision!=plan.revision)
            throw std::invalid_argument("world12: score batch is for another cup stage/leg");
        auto stage=std::find_if(staged.cups.rbegin(),staged.cups.rend(),[&](const auto& s){return s.cup==result.cup;});
        commit_cup_leg(*stage,result,staged.world);
        if(stage->complete() && stage->next_entry) {
            if(!result.next_stage || result.next_stage->cup!=stage->cup ||
                result.next_stage->source_entry!=stage->next_entry || result.next_stage->revision!=0)
                throw std::invalid_argument("world12: linked cup stage and its original draw are required");
            staged.cups.push_back(*result.next_stage);
            if(auto notice=cup_draw_notice(staged.world,staged.cups.back()))presentation.cup_draws.push_back(std::move(*notice));
        } else if(result.next_stage)throw std::invalid_argument("world12: unexpected linked cup stage");
        else if(!result.next_draw.empty()) {
            if(auto notice=cup_draw_notice(staged.world,*stage))presentation.cup_draws.push_back(std::move(*notice));
        }
    }
    WatcomRandom european_random(staged.watcom_seed);
    for(const auto& result:european_results)commit_european_results(staged.world,result,*shared_rules,european_random,staged.season);
    staged.watcom_seed=european_random.seed();presentation.watcom_draws=european_random.calls();
    if(indoor_results){const auto outcome=commit_indoor_results(staged,*indoor_results,*indoor_reference,*shared_rules);presentation.game_draws+=outcome.game_draws;presentation.watcom_draws+=outcome.watcom_draws;}
    if(regional_results){commit_regional_playoff_results(staged,*regional_results,*shared_rules,european_random);staged.watcom_seed=european_random.seed();presentation.watcom_draws=european_random.calls();}
    if(national_results){commit_national_results(staged.world,*national_results,*national_rules,european_random);staged.watcom_seed=european_random.seed();presentation.watcom_draws=european_random.calls();}
    commit_friendly_results(staged,friendly_results);
    for(const auto& round:completed){for(size_t m=0;m<round.results.size();++m)staged.results.result(staged.world.catalogue,round.league,round.round,int(m))=round.results[m];staged.league_rounds[round.league.value]=round.round;}
    record_completed_league_history(staged,state,completed);
    if(std::any_of(staged.calendar->schedule.others.begin(),staged.calendar->schedule.others.end(),regional_playoff_appointment))activate_regional_playoff(staged,state);
    bool next_date=true;
    if(ticket.indoor_date){
        if(!staged.world.indoor->date_pass)staged.world.indoor->date_pass=IndoorDatePass{ticket.day,0,ticket.indoor_total,false};
        auto& pass=*staged.world.indoor->date_pass;
        if(ticket.indoor_ordinary_phase)pass.ordinary_completed=true;else ++pass.completed;
        next_date=pass.completed==pass.total;if(next_date)staged.world.indoor->date_pass.reset();
    }
    ++staged.calendar->revision;if(next_date){++staged.calendar->next_day;++staged.date;}
    validate_cup_programmes(staged.world,staged.cups);
    state=std::move(staged);
    return presentation;
}
}
