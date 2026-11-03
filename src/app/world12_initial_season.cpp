// SPDX-License-Identifier: GPL-3.0-only
#include "app/world12_initial_season.hpp"
#include "game/world12/european.hpp"
#include "game/world12/match_projection.hpp"
#include "game/world12/market_auctions.hpp"
#include "game/season.hpp"
#include "game/lineup.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>
namespace bmh::app::world12 {
using namespace bmh::world12;
InitialCompetitionOutcome prepare_initial_competitions(CareerSnapshot& state,ManagerActionOwner& owner,
    ManagerActionOwner expected,const SeasonCalendar& calendar,const CupSources& sources,const GameState& reference) {
    state.validate();calendar.validate(state.world.catalogue);
    if(!owner.scope||owner.scope!=expected.scope||owner.revision!=expected.revision||owner.revision==std::numeric_limits<uint64_t>::max()||
        state.date||state.season||state.calendar||!state.cups.empty()||state.world.initial_competitions||!state.world.settings||
        state.world.managers.size()!=state.world.settings->planned_managers||
        std::any_of(state.league_rounds.begin(),state.league_rounds.end(),[](auto value){return value!=0;}))throw std::invalid_argument("world12: invalid initial competition owner/phase");
    for(const auto& manager:state.world.managers)if(!manager.career||!manager.career->season_records||!manager.match_state||!manager.finance||!manager.tactics)throw std::invalid_argument("world12: initial competitions need complete managers");
    if(reference.memory.pointer(layout::Career_CompetitionPointersEnglish_ExecutableField5)!=layout::Career_CompetitionPointersEnglish_RuntimeField14+16)
        throw std::invalid_argument("world12: unsupported Champions League qualifier layout");
    const auto before=state;auto candidate=state;GameRandom game(state.game_seed);WatcomRandom random(state.watcom_seed);
    const auto settings=*candidate.world.settings;std::vector<ClubId> clubs;
    for(auto group:{TableGroup::germany,TableGroup::england,TableGroup::europe})for(auto league:candidate.world.catalogue.simulation_leagues(group))
        clubs.insert(clubs.end(),candidate.world.membership[league.value].clubs.begin(),candidate.world.membership[league.value].clubs.end());
    for(auto club:clubs) {
        const bool human=std::any_of(candidate.world.managers.begin(),candidate.world.managers.end(),[&](const auto& m){return m.club==club;});
        auto projected=human?AiMatchProjection::create_for_humans(candidate,{club},reference,{settings.mode,settings.difficulty,settings.foreign_rule,true}):AiMatchProjection::create(candidate.world,{club},reference);
        bmh::club_balance(projected.state,0,random);bmh::auto_lineup(projected.state,0,human?0xe4:100);projected.commit_match_state(candidate.world);
    }
    auto fields=draw_initial_shared_fields(candidate.world,game,random);
    for(auto& manager:candidate.world.managers) {
        auto& history=*manager.career->season_records;history[1]=history[2]=0;
        const uint8_t rounds[]={1,3,2};
        for(size_t k=0;k<3;++k)if(std::find(fields.qualified[k].begin(),fields.qualified[k].end(),manager.club)!=fields.qualified[k].end())history[2]=uint8_t(rounds[k]+k*32);
    }
    candidate.world.initial_competitions=std::move(fields);
    candidate.world.european_campaigns=initial_european_campaigns(candidate.world,sources.german);
    if(candidate.world.catalogue.scope.germany)candidate.world.indoor=initial_indoor_season(candidate.world,sources.german,reference);
    candidate.game_seed=game.seed();candidate.watcom_seed=random.seed();
    CountryCupRanking germany,england;
    for(auto country:{Country::germany,Country::england})if(candidate.world.catalogue.selectable(country))for(uint8_t local=0;local<5;++local)
        (country==Country::germany?germany:england)[local]=candidate.world.membership[candidate.world.catalogue.starting_league(country,local).value].clubs;
    if(candidate.world.catalogue.scope.germany)candidate.cups=initial_domestic_cup_fields(candidate.world,Country::germany,germany,sources.german,game);
    if(candidate.world.catalogue.scope.england){auto fields=initial_domestic_cup_fields(candidate.world,Country::england,england,sources.english,game);candidate.cups.insert(candidate.cups.end(),fields.begin(),fields.end());}
    // Source starts both fixed finals from the reigning champion and national
    // cup holder. They have their own country owners despite sharing UI space.
    const auto& titles=*candidate.world.initial_competitions;
    for(auto country:{Country::germany,Country::england})if(candidate.world.catalogue.selectable(country)) {
        const auto champion=std::find_if(titles.champions.begin(),titles.champions.end(),[&](const auto& row){return row.first==country;});
        const auto holder=std::find_if(titles.domestic_holders.begin(),titles.domestic_holders.end(),[&](const auto& row){return row.first==country;});
        if(champion==titles.champions.end()||holder==titles.domestic_holders.end()||champion->second==holder->second)
            throw std::invalid_argument("world12: initial Super-Cup needs distinct verified title holders");
        const auto cup=country==Country::germany?DomesticCup::german_supercup:DomesticCup::english_supercup;
        candidate.cups.push_back(KnockoutStage::from_source(cup,country==Country::germany?sources.german:sources.english,
            23,{{champion->second,holder->second}},candidate.world));
    }
    candidate.game_seed=game.seed();std::vector<CupDrawNotice> notices;
    for(const auto& cup:candidate.cups)if(auto notice=cup_draw_notice(candidate.world,cup))notices.push_back(std::move(*notice));
    for(auto& manager:candidate.world.managers)for(const auto& cup:candidate.cups)if(cup.cup==DomesticCup::german_cup||cup.cup==DomesticCup::fa_cup){
        bool present=std::find(cup.seeded.begin(),cup.seeded.end(),manager.club)!=cup.seeded.end();for(const auto& f:cup.rounds.front().fixtures)present|=f.home==manager.club||f.away==manager.club;
        if(present)(*manager.career->season_records)[1]=1;
    }
    candidate.calendar=CalendarCursor{calendar,0,0};candidate.validate();
    if(state!=before||owner.scope!=expected.scope||owner.revision!=expected.revision)throw std::invalid_argument("world12: initial competition owner changed during draw");
    InitialCompetitionOutcome outcome{notices,game.calls(),random.calls()};
    state=std::move(candidate);++owner.revision;return outcome;
}
}

namespace bmh::app::world12 {
namespace {
// Exact local relation projection for the two scalar popularity routines. No
// unrequested club is manufactured or persisted in the bounded workspace.
void install_season_relations(bmh::GameState& local,const bmh::world12::World& world,bmh::world12::ClubId id) {
    using namespace bmh;using namespace bmh::world12;const auto& fields=*world.initial_competitions;const auto country=world.clubs.at(id).country;
    const auto find=[&](const auto& rows){return std::find_if(rows.begin(),rows.end(),[&](const auto& row){return row.first==country;});};
    const auto champion=find(fields.champions),cup=find(fields.domestic_holders);
    if(champion==fields.champions.end()||cup==fields.domestic_holders.end())throw std::invalid_argument("world12: initial title relations are unavailable");
    const auto word=[&](uint32_t at,uint16_t value){local.memory.add_block(at,2);local.memory.set16(at,value);};
    const auto byte=[&](uint32_t at,uint8_t value){local.memory.add_block(at,1);local.memory.set8(at,value);};
    const uint32_t lists[]={layout::Career_CompetitionPointersEnglish_ExecutableField25,layout::Career_CompetitionPointersEnglish_RuntimeField14,layout::Career_CompetitionPointersEnglish_ExecutableField19};
    const uint32_t counts[]={layout::Career_EuropeanClub_ExecutableField,layout::Career_EuropeanClub_ExecutableField2,layout::Career_EuropeanClub_ExecutableField3};
    for(size_t k=0;k<3;++k){const bool present=std::find(fields.fields[k].begin(),fields.fields[k].end(),id)!=fields.fields[k].end();byte(counts[k],uint8_t(present));word(lists[k],present?0:0xffff);}
    word(layout::Cup_AdvanceCup_RuntimeField6,champion->second==id?0:0xffff);
    word(layout::Season_CareerSeasonSetup_RuntimeField,cup->second==id?0:0xffff);
    const uint32_t holders[]={layout::Season_CareerSeasonSetup_RuntimeField4,layout::Season_CareerSeasonSetup_RuntimeField5,layout::Season_CareerSeasonSetup_RuntimeField6};
    for(size_t k=0;k<3;++k)word(holders[k],fields.holders[k]==id?0:0xffff);
}
}
InitialManagerServicesOutcome initialize_first_season_manager_services(bmh::world12::CareerSnapshot& state,
    ManagerActionOwner& owner,ManagerActionOwner expected,const bmh::GameState& reference) {
    using namespace bmh;using namespace bmh::world12;state.validate();
    if(!owner.scope||owner.scope!=expected.scope||owner.revision!=expected.revision||owner.revision==std::numeric_limits<uint64_t>::max()||
        state.date||state.season||!state.world.loans.empty()||!state.world.initial_competitions||state.world.initial_competitions->manager_services_complete||!state.world.settings||
        !state.calendar||state.calendar->next_day||std::any_of(state.league_rounds.begin(),state.league_rounds.end(),[](auto n){return n!=0;}))throw std::invalid_argument("world12: invalid first-season manager service phase");
    if(state.world.annual_awards_completed&&*state.world.annual_awards_completed!=0)throw std::invalid_argument("world12: fresh services cannot overwrite completed annual awards");
    for(const auto& m:state.world.managers)if(!m.career||!m.career->season_records||m.career->season_services)throw std::invalid_argument("world12: initial manager service state is missing or already present");
    const auto before=state;auto candidate=state;const auto settings=*state.world.settings;WatcomRandom random(state.watcom_seed);InitialManagerServicesOutcome outcome;
    // Original sells every manager's tickets before resetting any manager.
    std::vector<std::pair<uint8_t,ClubId>> managers;for(const auto& m:state.world.managers)managers.emplace_back(m.seat,m.club);
    for(const auto& [seat,club]:managers){auto projected=AiMatchProjection::create_for_humans(candidate,{club},reference,{settings.mode,settings.difficulty,settings.foreign_rule,true});
        install_season_relations(projected.state,candidate.world,club);initialize_season_tickets(projected.state,0,random);projected.commit_match_state(candidate.world,true);
        const auto m=std::find_if(candidate.world.managers.begin(),candidate.world.managers.end(),[&](const auto& m){return m.seat==seat;});const auto& data=*m->career->season_services;outcome.tickets.push_back({seat,data.tickets_sold,data.ticket_income});}
    for(const auto& [seat,club]:managers){auto projected=AiMatchProjection::create_for_humans(candidate,{club},reference,{settings.mode,settings.difficulty,settings.foreign_rule,true});
        install_season_relations(projected.state,candidate.world,club);
        const auto loans=layout::Career_CareerManagersEnd_RuntimeField;projected.state.memory.add_block(loans,24);for(size_t i=0;i<12;++i)projected.state.memory.set16(loans+uint32_t(i*2),0xfff);
        MinuteReport report;reset_manager_season(projected.state,0,random,report);if(report.unsupported)throw std::invalid_argument(report.reason);
        projected.commit_match_state(candidate.world,true);
        capture_transfer_history(candidate,projected);
    }
    for(auto& [id,club]:candidate.world.clubs)if(club.league.valid())club.fairplay_points=0;
    if(candidate.world.annual_title_archive&&(*candidate.world.annual_title_archive!=AnnualTitleArchive{}))
        throw std::invalid_argument("world12: fresh manager services cannot overwrite title history");
    candidate.world.annual_title_archive.emplace();
    candidate.world.annual_awards_completed=0;
    candidate.results=SeasonResults::empty(candidate.world.catalogue);candidate.world.initial_competitions->manager_services_complete=true;candidate.watcom_seed=random.seed();candidate.validate();
    if(state!=before||owner.scope!=expected.scope||owner.revision!=expected.revision)throw std::invalid_argument("world12: manager service owner changed during initialization");
    outcome.watcom_draws=random.calls();state=std::move(candidate);++owner.revision;return outcome;
}
}
