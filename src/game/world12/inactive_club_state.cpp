// SPDX-License-Identifier: GPL-3.0-only
#include "game/world12/inactive_club_state.hpp"
#include "game/world12/cup_draw.hpp"
#include "game/european_rules.hpp"
#include "game/cup.hpp"
#include <algorithm>
#include <set>
namespace bmh::world12 {
namespace {
const NationalStartupReceipt& startup(const World& world) {
    if(world.catalogue.scope.club_play()||!world.catalogue.scope.international||!world.national_coaches||!world.national_origins)
        throw FormatError("world12: inactive club setup needs the actual national-only population");
    const NationalStartupReceipt* receipt=nullptr;
    for(const auto& record:*world.national_coaches)if(record.startup){if(receipt)throw FormatError("world12: ambiguous national-only startup owner");receipt=&*record.startup;}
    if(!receipt)throw FormatError("world12: inactive club setup has unknown Source startup facts");return *receipt;
}
World population(const World& source) {
    auto result=source;result.catalogue=Catalogue::dual_classic();result.managers.clear();result.national_coaches.reset();result.national_squads.clear();result.national_campaigns.reset();result.inactive_club_state.reset();result.amateur_slots.reset();
    for(const auto& league:result.catalogue.leagues){std::vector<const NationalRosterOrigin*> slots;for(const auto& origin:*source.national_origins)if(origin.club.league==league.id)slots.push_back(&origin);
        std::sort(slots.begin(),slots.end(),[](const auto* a,const auto* b){return a->rank<b->rank;});
        if(slots.size()!=league.clubs)throw FormatError("world12: inactive setup lacks actual complete league origins");
        auto& membership=result.membership[league.id.value];membership.league=league.id;membership.clubs.clear();
        for(size_t rank=0;rank<slots.size();++rank){if(slots[rank]->rank!=rank||!slots[rank]->source_initialized||!result.clubs.emplace(slots[rank]->club.id,slots[rank]->club).second)throw FormatError("world12: inactive setup has repeated/unknown origin slots");membership.clubs.push_back(slots[rank]->club.id);}
    }
    result.national_origins.reset();result.validate();return result;
}
std::array<LeagueId,5> top_leagues(Country primary){return {LeagueId{primary==Country::germany?0u:5u},LeagueId{10},LeagueId{primary==Country::germany?5u:0u},LeagueId{11},LeagueId{12}};}
void draw_fields(const World& master,InactiveClubState& result,const LeImage& de,const LeImage& en,GameRandom& game,WatcomRandom& random) {
    const auto leagues=top_leagues(result.primary);
    const auto find=[&](ClubId id){for(size_t k=0;k<3;++k)if(std::find(result.qualified[k].begin(),result.qualified[k].end(),id)!=result.qualified[k].end())return int(k)+1;return 0;};
    const auto add=[&](int kind,ClubId id){result.qualified[size_t(kind)].push_back(id);};
    result.qualified={};result.fields={};result.domestic_fields.clear();
    for(size_t index=0;index<leagues.size();++index){const auto league=leagues[index];const auto& slots=master.membership[league.value].clubs;ClubId winner,runner;
        if(index==0){winner=result.primary_cup_seed;if(result.primary_cup_runner_token<slots.size())runner=slots[result.primary_cup_runner_token];}
        else{const auto a=size_t(random.range(0,int(slots.size())-1));size_t b;do b=size_t(random.range(0,int(slots.size())-1));while(a==b);winner=slots[a];runner=slots[b];}
        std::array<bool,3> in{};for(size_t k=0;k<3;++k)in[k]=master.clubs.at(result.holder_seeds[k]).league==league;
        // The primary's never-played Source seed is distinct from its physical
        // champion slot. Its unused runner may remain an opaque raw word.
        if(index==0&&find(winner))throw FormatError("world12: inactive primary cup seed unexpectedly needs its opaque runner");
        european_league_entrants(slots,winner,runner,result.holder_seeds,in,master.clubs.at(slots[0]).country==Country::italy?3:4,find,add);
    }
    for(size_t kind=0;kind<3;++kind)if(!find(result.holder_seeds[kind]))add(int(kind),result.holder_seeds[kind]);
    const auto& pools=result.reserve_pools;
    for(const auto& pool:pools)if(pool.size()!=7)throw FormatError("world12: inactive setup lacks actual seven-club foreign pools");
    // Apply the same bounded CW correction as the common club engine. These
    // physical unplayed slots do not become completed standings or trophies.
    if(result.qualified[2].size()<5)for(auto league:leagues){if(result.qualified[2].size()>=5)break;const auto country=master.catalogue.league(league).country;
        if(std::any_of(result.qualified[2].begin(),result.qualified[2].end(),[&](auto id){return master.clubs.at(id).country==country;}))continue;
        const auto& slots=master.membership[league.value].clubs;const auto available=std::find_if(slots.begin(),slots.end(),[&](auto id){return !find(id);});if(available==slots.end())throw FormatError("world12: inactive CW correction has no real physical slot");add(2,*available);}
    if(result.qualified[2].size()<5)throw FormatError("world12: inactive CW field has no country owner");
    using Token=uint64_t;constexpr Token scratch=Token(1)<<63;std::array<std::vector<Token>,3> fields;
    for(size_t kind=0;kind<3;++kind)for(auto id:result.qualified[kind])fields[kind].push_back(id.value);
    const auto contains=[&](Token token){for(const auto& field:fields)if(std::find(field.begin(),field.end(),token)!=field.end())return true;return false;};
    fill_initial_european_fields([&](int kind){return int(fields[size_t(kind)].size());},[&](int group){Token token;do{const auto slot=size_t(random.range(0,6));token=group<27?pools[size_t(group)].at(slot).value:scratch|Token(group*7+int(slot));}while(contains(token));return token;},[&](int kind,Token token){fields[size_t(kind)].push_back(token);},[&](int kind,int first,int count){auto& field=fields[size_t(kind)];for(int i=0;i<count;++i){const auto j=game.range(0,count-1);if(j<0||j>=count)throw FormatError("world12: inactive Source draw index is invalid");std::swap(field.at(size_t(first+i)),field.at(size_t(first+j)));}});
    const size_t sizes[]={64,24,32};for(size_t kind=0;kind<3;++kind)for(size_t i=0;i<sizes[kind];++i){const auto token=fields[kind].at(i);if(token&scratch)throw FormatError("world12: inactive setup would retain a non-club scratch tail");result.fields[kind].push_back({uint32_t(token)});}
    CountryCupRanking ranking;for(uint8_t local=0;local<5;++local)ranking[local]=master.membership[master.catalogue.starting_league(result.primary,local).value].clubs;
    const auto cups=initial_domestic_cup_fields(master,result.primary,ranking,result.primary==Country::germany?de:en,game);
    for(const auto& cup:cups){InactiveDomesticField field;field.source_entry=cup.source_entry;field.seeds=cup.seeded;for(const auto& fixture:cup.rounds.front().fixtures){field.teams.push_back(fixture.home);field.teams.push_back(fixture.away);}result.domestic_fields.push_back(std::move(field));}
}
}
void validate_inactive_club_state(const World& world) {
    if(!world.inactive_club_state)return;const auto& state=*world.inactive_club_state;const auto& receipt=startup(world);
    if(state.primary!=receipt.primary||state.season>32767||!state.primary_cup_seed.valid())throw FormatError("world12: invalid inactive club setup identity/season");
    const auto leagues=top_leagues(state.primary);
    const auto physical_seventh=[&](LeagueId league){for(const auto& origin:*world.national_origins)if(origin.club.league==league&&origin.rank==7)return origin.club.id;throw FormatError("world12: inactive Source seed has no real physical origin");};
    if(state.primary_cup_seed!=physical_seventh(leagues[0])||state.holder_seeds!=std::array<ClubId,3>{physical_seventh(leagues[2]),physical_seventh(leagues[3]),physical_seventh(leagues[1])})throw FormatError("world12: inactive setup seed differs from its real Source physical origin");
    std::set<ClubId> clubs;for(const auto& [id,club]:world.clubs)if(club.kind==ClubKind::club)clubs.insert(id);for(const auto& origin:*world.national_origins)clubs.insert(origin.club.id);
    std::set<ClubId> reserves;for(const auto& pool:state.reserve_pools){if(pool.size()!=7)throw FormatError("world12: inactive reserve country must retain seven actual Source identities");for(auto id:pool)if(!id.valid()||clubs.contains(id)||world.clubs.contains(id)||!reserves.insert(id).second)throw FormatError("world12: inactive reserve identities are invalid, repeated or overlap an actual team");}
    clubs.insert(reserves.begin(),reserves.end());
    const size_t sizes[]={64,24,32};std::set<ClubId> current;
    for(size_t kind=0;kind<3;++kind){if(state.fields[kind].size()!=sizes[kind]||state.qualified[kind].empty()||!clubs.contains(state.holder_seeds[kind]))throw FormatError("world12: invalid inactive Source field dimensions/seed");for(auto id:state.fields[kind])if(!clubs.contains(id)||!current.insert(id).second)throw FormatError("world12: inactive Source field has an unknown/repeated club");for(auto id:state.qualified[kind])if(std::find(state.fields[kind].begin(),state.fields[kind].end(),id)==state.fields[kind].end())throw FormatError("world12: inactive Source qualifier missing from field");}
    if(state.domestic_fields.size()!=(state.primary==Country::germany?1:2))throw FormatError("world12: invalid inactive domestic field count");
    for(size_t i=0;i<state.domestic_fields.size();++i){const auto& field=state.domestic_fields[i];const bool german=state.primary==Country::germany;
        if(field.source_entry!=(i?25:0)||field.teams.size()!=(german?64u:i?24u:48u)||field.seeds.size()!=(german?0u:20u))throw FormatError("world12: invalid inactive domestic Source dimensions/entry");
        std::set<ClubId> unique;for(const auto* ids:{&field.teams,&field.seeds})for(auto id:*ids)if(!unique.insert(id).second||std::none_of(world.national_origins->begin(),world.national_origins->end(),[&](const auto& origin){return origin.club.id==id&&origin.club.country==state.primary;}))throw FormatError("world12: inactive domestic field lost or repeated its actual country origin");}
}
InactiveClubStartupOutcome initialize_inactive_club_state(World& world,const World& actual_master,const LeImage& de,const LeImage& en,const GameState& reference,GameRandom& game,WatcomRandom& random) {
    world.validate();actual_master.validate();const auto master_before=actual_master;const auto before=world;const auto receipt=startup(world);if(world.inactive_club_state||world.national_campaigns)throw FormatError("world12: inactive club startup is already owned or national draw was repeated");
    if(actual_master.catalogue.scope!=GameScope{})throw FormatError("world12: inactive Source setup needs the actual untouched full master scope");
    for(const auto& origin:*world.national_origins){const auto found=actual_master.clubs.find(origin.club.id);const auto& slots=actual_master.membership[origin.club.league.value].clubs;if(found==actual_master.clubs.end()||found->second.country!=origin.club.country||found->second.league!=origin.club.league||origin.rank>=slots.size()||slots[origin.rank]!=origin.club.id)throw FormatError("world12: inactive Source startup origin does not bind its actual master slot");}
    auto g=game;auto w=random;const auto gs=game;const auto ws=random;const auto master=population(world);InactiveClubState state;state.primary=receipt.primary;const auto leagues=top_leagues(state.primary);
    state.primary_cup_seed=master.membership[leagues[0].value].clubs.at(7);state.holder_seeds={master.membership[leagues[2].value].clubs.at(7),master.membership[leagues[3].value].clubs.at(7),master.membership[leagues[1].value].clubs.at(7)};
    const auto& source=state.primary==Country::germany?de:en;const auto at=cup_entry_address(state.primary==Country::germany?0:28)+0x24;if(!source.contains(at+1))throw FormatError("world12: inactive Source cup seed metadata is missing");state.primary_cup_runner_token=source.u16(at);
    for(const auto& [id,club]:actual_master.clubs)if(club.kind==ClubKind::club&&!club.league.valid()&&uint8_t(club.country)>=5&&uint8_t(club.country)<32)state.reserve_pools[uint8_t(club.country)-5].push_back(id);
    const auto four=initial_national_four(g,w);InactiveClubStartupOutcome outcome;outcome.initial_national_field.assign(four.begin(),four.end());draw_fields(master,state,de,en,g,w);auto candidate=world;candidate.inactive_club_state=std::move(state);candidate.validate();
    if(world!=before||actual_master!=master_before||game.seed()!=gs.seed()||game.calls()!=gs.calls()||random.seed()!=ws.seed()||random.calls()!=ws.calls())throw FormatError("world12: inactive Source startup owner/RNG changed");outcome.game_draws=g.calls()-gs.calls();outcome.watcom_draws=w.calls()-ws.calls();world=std::move(candidate);game=g;random=w;(void)reference;return outcome;
}
std::pair<uint64_t,uint64_t> advance_inactive_club_state(World& world,uint32_t target,const LeImage& de,const LeImage& en,GameRandom& game,WatcomRandom& random) {
    world.validate();if(!world.inactive_club_state||target!=world.inactive_club_state->season+1||target>32767||!world.national_campaigns||!world.national_campaigns->seasons.contains(target))throw FormatError("world12: inactive Source annual fields need their saved state and actual new national draw");
    const auto before=world;const auto gs=game;const auto ws=random;auto g=game;auto w=random;auto candidate=world;auto& state=*candidate.inactive_club_state;state.season=target;draw_fields(population(world),state,de,en,g,w);candidate.validate();if(world!=before||game.seed()!=gs.seed()||game.calls()!=gs.calls()||random.seed()!=ws.seed()||random.calls()!=ws.calls())throw FormatError("world12: inactive Source annual owner/RNG changed");const auto calls=std::pair{g.calls()-gs.calls(),w.calls()-ws.calls()};world=std::move(candidate);game=g;random=w;return calls;
}
}
