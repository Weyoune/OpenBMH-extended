// SPDX-License-Identifier: GPL-3.0-only
#include <string>
#include "app/world12_weather.hpp"
#include "cli/json.hpp"
#include "game/world12/prepared_event.hpp"
#include "game/world12/national_coach.hpp"
#include "game/world12/national.hpp"
#include "game/week.hpp"
#include <bit>
#include <limits>
#include <algorithm>
#include "game/world12/replay_rules.hpp"
#include <map>
namespace bmh::app::world12 {
using namespace bmh::world12;
RefereeCatalogue decode_referee_catalogue(const Bytes& bytes) {
    if(bytes.size()>65536)throw FormatError("world12: referee catalogue is too large");
    const auto root=cli::parse_json(std::string(bytes.begin(),bytes.end()));
    const auto get=[&](const char* name)->const cli::Json& {
        if(!root.object())throw FormatError("world12: invalid referee catalogue object");
        const auto found=root.object()->find(name);
        if(found==root.object()->end())throw FormatError("world12: incomplete referee catalogue");return found->second;
    };
    const auto& format=get("format");const auto& version=get("version");
    if(!format.string()||*format.string()!="openbmh-referees"||!version.integer()||*version.integer()!=1)
        throw FormatError("world12: unsupported referee catalogue");
    const auto& names=get("names");const auto& properties=get("properties");
    if(!names.array()||names.array()->size()!=50||!properties.array()||properties.array()->size()!=50)
        throw FormatError("world12: referee catalogue must contain fifty entries");
    RefereeCatalogue result;
    for(size_t i=0;i<50;++i) {
        const auto& name=names.array()->at(i);const auto& property=properties.array()->at(i);
        if(!name.string()||name.string()->empty()||name.string()->size()>255||name.string()->find('\0')!=std::string::npos||
           !property.integer()||*property.integer()<0||*property.integer()>255)
            throw FormatError("world12: invalid referee name/properties");
        result.names.push_back(*name.string());result.properties.push_back(uint8_t(*property.integer()));
    }
    return result;
}
WeatherPreparation prepare_fixture_weather(const World& world,int month,
    const std::vector<FixtureWeatherRequest>& requests,const std::vector<uint8_t>& catalogue,
    GameRandom& game,WatcomRandom& random) {
    world.validate();
    if(month<1||month>12||catalogue.size()!=50)throw FormatError("world12: invalid match weather/referee resources");
    std::map<Country,size_t> counts;
    // OpenBMH deviates from the original: referee availability is scoped to
    // each country's programme so the shared world can exceed 49 matches.
    for(const auto& request:requests) {
        const auto home=world.clubs.find(request.fixture.home),away=world.clubs.find(request.fixture.away);
        if(home==world.clubs.end()||away==world.clubs.end()||home==away)
            throw FormatError("world12: invalid weather fixture identities");
        const bool referee=home->second.kind==ClubKind::club&&away->second.kind==ClubKind::club&&!(request.flags&0x80);
        if(referee&&++counts[home->second.country]>49)
            throw FormatError("world12: country programme exceeds its referee pool");
    }
    auto staged_game=game;auto staged_random=random;
    const auto previous_game=game.calls(),previous_random=random.calls();
    std::map<Country,std::vector<uint8_t>> pools;WeatherPreparation result;
    for(const auto& request:requests) {
        const auto& home=world.clubs.at(request.fixture.home);const auto& away=world.clubs.at(request.fixture.away);
        auto [pool,inserted]=pools.try_emplace(home.country,catalogue);
        if(inserted)for(auto& property:pool->second)property&=0x7f;
        const bool referee=home.kind==ClubKind::club&&away.kind==ClubKind::club&&!(request.flags&0x80);
        const auto weather=draw_match_weather(int(home.country),month,referee,pool->second,staged_game,staged_random);
        result.matches.push_back({request,weather,home.country});
    }
    result.game_draws=staged_game.calls()-previous_game;result.watcom_draws=staged_random.calls()-previous_random;
    game=staged_game;random=staged_random;return result;
}
WeatherPreparation prepare_event_weather(CareerSnapshot& state,ManagerActionOwner& owner,
    ManagerActionOwner expected,const std::vector<uint8_t>& catalogue,const CupSources& sources,const GameState* indoor_reference) {
    state.validate();
    if(!owner.scope||owner.scope!=expected.scope||owner.revision!=expected.revision||
       owner.revision==std::numeric_limits<uint64_t>::max()||state.prepared_event||!state.progress||
       state.progress->phase!=ProgressPhase::scheduled_event||!state.desktop||state.desktop->manager_order.empty()||catalogue.size()!=50)
        throw FormatError("world12: missing or stale event-weather owner");
    const auto before=state;auto candidate=state;
    IndoorTransitionOutcome invitation;
    if(candidate.world.catalogue.scope.germany&&candidate.league_rounds[0]==17&&candidate.world.economy&&candidate.world.economy->presentation_options&&!(*candidate.world.economy->presentation_options)[16*9+4]&&(!candidate.world.indoor||candidate.world.indoor->tournaments.empty())){
        if(!indoor_reference)throw FormatError("world12: first indoor draw needs its Source reference owner");invitation=prepare_indoor_draw(candidate,*indoor_reference,sources.german);
    }
    const auto appointments=admitted_other_appointments(candidate,candidate.calendar->next_day);
    uint64_t nomination_draws=0;
    const bool national=std::any_of(appointments.begin(),appointments.end(),[&](const auto& a){return national_appointment(a,candidate.season);})||(candidate.friendly_plan&&std::any_of(candidate.friendly_plan->pending.begin(),candidate.friendly_plan->pending.end(),[&](const auto& b){return b.day==candidate.calendar->next_day&&b.national;}));
    if(national){if(!indoor_reference)throw FormatError("world12: national appointment needs its actual Source reference");if(!candidate.world.national_coaches)throw FormatError("world12: national coach account history is unknown");activate_national_coaches(candidate.world,candidate.desktop->manager_order);WatcomRandom nominees(candidate.watcom_seed);for(auto country:{Country::germany,Country::england}){const auto squad=std::find_if(candidate.world.national_squads.begin(),candidate.world.national_squads.end(),[&](const auto& s){return candidate.world.clubs.at(s.team).country==country;});if(squad==candidate.world.national_squads.end())nominate_national_squad(candidate.world,country,candidate.date,*indoor_reference,nominees);}candidate.watcom_seed=nominees.seed();nomination_draws=nominees.calls();}
    else if(candidate.world.catalogue.scope.club_play()&&national_roles_active(candidate.world))park_national_coaches(candidate.world);
    const auto ticket=prepare_day(candidate);
    if(!ticket||ticket->leagues.empty()&&ticket->cups.empty()&&ticket->european.empty()&&ticket->indoor.empty()&&!ticket->regional_playoff&&ticket->national.empty()&&ticket->friendlies.empty()){std::string entries;if(ticket)for(const auto& other:ticket->others)entries+=" "+std::to_string(int(other.domain))+":"+std::to_string(other.source_entry);
        throw FormatError("world12: event weather has no scheduled matches (calendar day "+std::to_string(candidate.calendar->next_day)+", season "+std::to_string(candidate.season)+", entries domain:source"+entries+")");}
    for(const auto& appointment:ticket->others)if(!domestic_cups_on(appointment)&&!european_cups_on(appointment)&&!german_indoor_appointment(appointment)&&!regional_playoff_appointment(appointment)&&!national_appointment(appointment,candidate.season)&&!friendly_appointment(appointment))throw FormatError("world12: weather for this shared/national appointment needs its competition owner");
    PreparedEvent event;
    event.day=ticket->day;event.calendar_revision=ticket->revision;
    std::copy(catalogue.begin(),catalogue.end(),event.referee_properties.begin());
    std::vector<FixtureWeatherRequest> requests;
    for(const auto& league:ticket->leagues) {
        PreparedGroup group;group.league=league.appointment.league;group.round=league.appointment.round;group.replay=!league.ordinary;
        if(group.replay){group.source_entry=replay_source_entry(sources.german);group.flags=sources.german.u8(cup_entry_address(group.source_entry)+0x14);}
        for(auto fixture:league.matches){requests.push_back({fixture,group.flags});group.matches.push_back({fixture});}
        event.groups.push_back(std::move(group));
    }
    for(const auto& cup:ticket->cups) {
        const auto stage=std::find_if(candidate.cups.rbegin(),candidate.cups.rend(),[&](const auto& s){return s.cup==cup.cup;});
        const auto& source=cup_country(cup.cup)==Country::germany?sources.german:sources.english;
        const auto verified=KnockoutStage::from_source(cup.cup,source,cup.source_entry,stage->rounds.front().fixtures,candidate.world,stage->seeded);
        if(verified.flags!=stage->flags||verified.planned_rounds!=stage->planned_rounds||verified.next_entry!=stage->next_entry)
            throw FormatError("world12: source cup rules changed before weather preparation");
        PreparedGroup group;group.cup=cup.cup;group.source_entry=cup.source_entry;group.leg=cup.leg;
        group.round=uint8_t(stage->rounds.size());group.revision=cup.revision;group.flags=prepared_cup_flags(*stage);
        for(auto fixture:cup.matches){requests.push_back({fixture,group.flags});group.matches.push_back({fixture});}
        event.groups.push_back(std::move(group));
    }
    for(const auto& plan:ticket->european){
        const auto rules=european_programme_rules(sources.german,plan.competition);const auto& campaign=(*candidate.world.european_campaigns)[size_t(uint8_t(plan.competition)-1)];
        if(rules.size()!=campaign.stages.size())throw FormatError("world12: European source programme changed before weather");
        for(size_t i=0;i<rules.size();++i)if(rules[i]!=campaign.stages[i].rules)throw FormatError("world12: European source stage changed before weather");
        PreparedGroup group;group.european=plan.competition;group.source_entry=plan.source_entry;group.round=plan.round;group.leg=plan.leg;group.flags=plan.flags;group.revision=plan.revision;
        for(auto fixture:plan.matches){requests.push_back({fixture,group.flags});group.matches.push_back({fixture});}event.groups.push_back(std::move(group));
    }
    for(const auto& plan:ticket->indoor){
        if(candidate.world.indoor->rules!=indoor_programme_rules(sources.german))throw FormatError("world12: indoor rules changed before weather preparation");PreparedGroup group;group.indoor=true;group.indoor_tournament=plan.tournament;group.source_entry=plan.source_entry;group.round=uint8_t(plan.played+1);group.leg=1;group.flags=plan.flags;group.revision=plan.revision;for(auto fixture:plan.matches){requests.push_back({fixture,group.flags});group.matches.push_back({fixture});}event.groups.push_back(std::move(group));
    }
    if(ticket->regional_playoff){const auto& plan=*ticket->regional_playoff;
        if(!candidate.world.regional_playoff||regional_playoff_rules(sources.german)!=candidate.world.regional_playoff->rules)throw FormatError("world12: regional rules changed before weather");
        PreparedGroup group;group.regional_playoff=true;group.source_entry=35;group.round=uint8_t(plan.played+1);group.leg=1;group.flags=plan.flags;group.revision=plan.revision;
        for(auto fixture:plan.matches){requests.push_back({fixture,group.flags});group.matches.push_back({fixture});}event.groups.push_back(std::move(group));
    }
    for(const auto& plan:ticket->national){
        const auto primary=candidate.world.national_campaigns->primary_country;const auto& source=primary==Country::england?sources.english:sources.german;
        const auto rules=national_programme_rules(source,candidate.season);const auto& campaign=candidate.world.national_campaigns->seasons.at(candidate.season);
        if(rules.size()!=campaign.stages.size())throw FormatError("world12: national Source programme changed before weather");for(size_t n=0;n<rules.size();++n)if(rules[n]!=campaign.stages[n].rules)throw FormatError("world12: national Source stage changed before weather");
        PreparedGroup group;group.national=plan.competition;group.source_entry=plan.source_entry;group.round=plan.round;group.leg=plan.leg;group.flags=plan.flags;group.revision=plan.revision;
        for(auto fixture:plan.matches){requests.push_back({fixture,group.flags});group.matches.push_back({fixture});}event.groups.push_back(std::move(group));
    }
    for(const auto& plan:ticket->friendlies){const auto& source=plan.national&&candidate.world.national_campaigns&&candidate.world.national_campaigns->primary_country==Country::england?sources.english:sources.german;if(plan.flags!=friendly_source_flags(source))throw FormatError("world12: friendly Source flags changed");PreparedGroup group;group.friendly=true;group.friendly_national=plan.national;group.source_entry=24;group.round=1;group.leg=1;group.flags=plan.flags;group.revision=plan.revision;for(auto fixture:plan.matches){requests.push_back({fixture,group.flags});group.matches.push_back({fixture});}event.groups.push_back(std::move(group));}
    GameRandom game(candidate.game_seed);WatcomRandom random(candidate.watcom_seed);
    WeatherPreparation weather;
    if(ticket->indoor.empty())weather=prepare_fixture_weather(candidate.world,month_code(candidate.date),requests,catalogue,game,random);
    else {auto referees=catalogue;for(auto& property:referees)property&=0x7f;for(const auto& request:requests){const auto& home=candidate.world.clubs.at(request.fixture.home);const auto drawn=draw_match_weather(int(home.country),month_code(candidate.date),!(request.flags&0x80),referees,game,random);weather.matches.push_back({request,drawn,Country::germany});}weather.game_draws=game.calls();weather.watcom_draws=random.calls();}
    weather.game_draws+=invitation.game_draws;weather.watcom_draws+=invitation.watcom_draws+nomination_draws;
    size_t index=0;
    for(auto& group:event.groups)for(auto& match:group.matches) {
        const auto& prepared=weather.matches.at(index++);match.temperature=std::bit_cast<int8_t>(prepared.weather.temperature);
        match.rain=prepared.weather.rain;match.referee=prepared.weather.referee;match.referee_country=prepared.referee_country;
    }
    candidate.prepared_event=std::move(event);candidate.game_seed=game.seed();candidate.watcom_seed=random.seed();candidate.validate();
    if(state!=before||owner.scope!=expected.scope||owner.revision!=expected.revision)
        throw FormatError("world12: event-weather state changed during preparation");
    state=std::move(candidate);++owner.revision;return weather;
}
}
