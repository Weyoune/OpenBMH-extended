// SPDX-License-Identifier: GPL-3.0-only
#include "game/world12/annual_qualification.hpp"
#include "game/world12/standings.hpp"
#include "game/european_rules.hpp"
#include "game/cup.hpp"
#include "game/driver.hpp"
#include <algorithm>
#include <limits>
#include <set>

namespace bmh::world12 {
namespace {
[[noreturn]] void invalid(const char* reason) {throw FormatError(reason);}
const ClubRecord& club(const World& world,ClubId id) {
    const auto at=world.clubs.find(id);
    if(at==world.clubs.end()||at->second.kind!=ClubKind::club)
        invalid("world12: annual qualification contains an unknown or non-club identity");
    return at->second;
}
AnnualCupFinal domestic_final(const CareerSnapshot& state,DomesticCup kind) {
    const auto last=std::find_if(state.cups.rbegin(),state.cups.rend(),[&](const auto& s){return s.cup==kind;});
    if(last==state.cups.rend()||last->next_entry||!last->complete()||last->rounds.back().fixtures.size()!=1)
        invalid("world12: annual qualification needs a completed domestic final");
    const auto winners=last->qualifiers();
    if(winners.size()!=1)invalid("world12: annual domestic final has no unique winner");
    const auto& fixture=last->rounds.back().fixtures.front();
    if(winners[0]!=fixture.home&&winners[0]!=fixture.away)
        invalid("world12: annual domestic winner differs from its played final");
    return {winners[0],winners[0]==fixture.home?fixture.away:fixture.home};
}
AnnualCupFinal european_final(const EuropeanCampaign& campaign) {
    if(!campaign.complete)invalid("world12: annual qualification needs all completed European campaigns");
    const auto at=std::find_if(campaign.stages.begin(),campaign.stages.end(),[&](const auto& s){return s.rules.entry==campaign.active_entry;});
    if(at==campaign.stages.end()||!at->rules.knockout()||at->rules.next||at->rules.seeds||
       (at->rules.teams>>at->rules.rounds)!=1)
        invalid("world12: annual European owner has no terminal knockout final");
    size_t first=0;
    for(int r=0;r<int(at->rules.rounds)-1;++r)first+=size_t(at->rules.teams)>>r;
    const auto home=at->teams.at(first),away=at->teams.at(first+1);
    MatchGroup group;group.desc.assign(0xd0,0);group.desc[1]=at->rules.flags;
    const bool two_legs=(at->rules.flags&1)!=0;
    group.cup_results={at->scores[0].at(first/2)};
    if(two_legs)group.cup_other_leg={at->scores[1].at(first/2)};
    const auto side=bmh::cup_tie(group,0);
    if(side==2)invalid("world12: annual European final is unresolved");
    const AnnualCupFinal final=side==0?AnnualCupFinal{home,away}:AnnualCupFinal{away,home};
    if(final.winner!=campaign.winner||final.runner_up!=campaign.runner_up||
       final.winner!=at->winner||final.runner_up!=at->runner_up)
        invalid("world12: annual European title differs from the actual final scores");
    return final;
}
std::vector<LeagueId> source_leagues(const World& world) {
    std::vector<LeagueId> result{{world.catalogue.scope.germany?0u:5u}};
    for(uint32_t id:{10u,5u,11u,12u})if(world.catalogue.leagues[id].active&&
        std::find(result.begin(),result.end(),LeagueId{id})==result.end())result.push_back({id});
    return result;
}
using ReservePools=std::array<std::vector<ClubId>,27>;
ReservePools reserve_pools(const World& world) {
    ReservePools result;
    for(const auto& [id,c]:world.clubs)if(c.kind==ClubKind::club&&!c.league.valid()&&
        uint8_t(c.country)>=5&&uint8_t(c.country)<32)result[uint8_t(c.country)-5].push_back(id);
    for(const auto& pool:result)if(pool.size()!=7)
        invalid("world12: annual European field needs seven actual reserve clubs per country");
    return result;
}
void draw_fields(const World& world,AnnualEuropeanFields& fields,GameRandom& game,WatcomRandom& random) {
    const auto pools=reserve_pools(world);
    using Token=uint64_t;constexpr Token scratch_tag=Token(1)<<63;
    std::array<std::vector<Token>,3> lists;
    for(size_t k=0;k<3;++k)for(auto id:fields.qualified[k])lists[k].push_back(id.value);
    const auto contains=[&](Token token){for(const auto& list:lists)if(std::find(list.begin(),list.end(),token)!=list.end())return true;return false;};
    bmh::fill_initial_european_fields([&](int k){return int(lists[size_t(k)].size());},
        [&](int group){Token token;do{const auto slot=size_t(random.range(0,6));token=group<27?pools[size_t(group)].at(slot).value:scratch_tag|Token(group*7+int(slot));}while(contains(token));return token;},
        [&](int k,Token token){lists[size_t(k)].push_back(token);},
        [&](int k,int first,int count){auto& list=lists[size_t(k)];for(int i=0;i<count;++i){
            const auto j=game.range(0,count-1);if(j<0||j>=count)invalid("world12: annual European draw index is outside its Source field");
            std::swap(list.at(size_t(first+i)),list.at(size_t(first+j)));}});
    const size_t sizes[]={64,24,32};std::set<ClubId> current;
    for(size_t k=0;k<3;++k) {
        for(size_t i=0;i<sizes[k];++i){const auto token=lists[k].at(i);
            if(token&scratch_tag)invalid("world12: annual European quota would publish a non-club scratch entrant");
            const ClubId id{uint32_t(token)};club(world,id);
            if(!current.insert(id).second)invalid("world12: annual European fields repeat a current club");
            fields.fields[k].push_back(id);
        }
        for(auto id:fields.qualified[k])if(std::find(fields.fields[k].begin(),fields.fields[k].end(),id)==fields.fields[k].end())
            invalid("world12: annual European qualifier was lost during its Source draw");
    }
    for(auto id:fields.holders)if(!current.contains(id))invalid("world12: annual European field lost a completed title holder");
}
}
UnpublishedAnnualQualification stage_annual_qualification(const CareerSnapshot& state) {
    state.validate();
    if(!state.world.catalogue.scope.club_play()||!state.progress||state.progress->phase!=ProgressPhase::season_end||
       !state.world.settings||!state.world.initial_competitions||!state.world.european_campaigns||
       state.season==std::numeric_limits<uint32_t>::max()||!state.world.annual_awards_completed||
       *state.world.annual_awards_completed!=state.season+1)
        invalid("world12: annual qualification requires the paid closing boundary and actual competition owners");
    const auto before=state;
    UnpublishedAnnualQualification out;out.source_binding=before;out.source_season=before.season;out.target_season=before.season+1;
    out.game_seed_before=before.game_seed;out.watcom_seed_before=before.watcom_seed;
    const auto& world=before.world;
    for(const auto& league:world.catalogue.leagues)if(league.active) {
        if(before.league_rounds[league.id.value]!=league.rounds()||
           std::any_of(before.results.leagues[league.id.value].begin(),before.results.leagues[league.id.value].end(),[](const auto& s){return !s.played;}))
            invalid("world12: annual qualification requires every completed league result and round cursor");
        TableQuery query;
        for(const auto& m:world.managers)if(m.match_state&&club(world,m.club).league==league.id) {
            const auto deduction=m.match_state->point_deduction;
            if((deduction>>5)==3)query.point_deductions[m.club]=deduction&31;
        }
        for(const auto& row:compute_standings(world,before.results,league.id,before.league_rounds[league.id.value],query))
            out.final_rankings[league.id.value].push_back(row.club);
    }
    for(auto country:{Country::germany,Country::england})if(world.catalogue.selectable(country)) {
        // Every configured domestic owner must have completed, including fixed
        // Super-Cups and the English League Cup; only the main cup qualifies CW.
        for(auto kind:world.catalogue.rules(country).cups)(void)domestic_final(before,kind);
        const auto kind=country==Country::germany?DomesticCup::german_cup:DomesticCup::fa_cup;
        out.domestic_titles.push_back({country,out.final_rankings[world.catalogue.starting_league(country,0).value].front(),domestic_final(before,kind)});
    }
    for(size_t k=0;k<3;++k){out.european_titles[k]=european_final((*world.european_campaigns)[k]);out.next_european.holders[k]=out.european_titles[k].winner;}
    GameRandom game(before.game_seed);WatcomRandom random(before.watcom_seed);
    auto& fields=out.next_european;
    const auto find=[&](ClubId id){for(size_t k=0;k<3;++k)if(std::find(fields.qualified[k].begin(),fields.qualified[k].end(),id)!=fields.qualified[k].end())return int(k)+1;return 0;};
    const auto add=[&](int kind,ClubId id){club(world,id);if(find(id))invalid("world12: annual European qualifier already owns another current place");fields.qualified[size_t(kind)].push_back(id);};
    const int extra=world.catalogue.scope.germany?0:1;
    for(auto league:source_leagues(world)) {
        const auto& descriptor=world.catalogue.league(league);const auto& ranked=out.final_rankings[league.value];
        AnnualLeagueQualification row;row.league=league;row.uefa_places=uint8_t((descriptor.country==Country::italy?3:4)+extra);
        row.source_random_domestic=!descriptor.playable;
        if(descriptor.playable){const auto title=std::find_if(out.domestic_titles.begin(),out.domestic_titles.end(),[&](const auto& t){return t.country==descriptor.country;});row.domestic=title->cup;}
        else {const auto& members=world.membership[league.value].clubs;
            const auto a=size_t(random.range(0,int(members.size())-1));size_t b;
            do b=size_t(random.range(0,int(members.size())-1));while(a==b);
            row.domestic={members[a],members[b]};}
        std::array<bool,3> in{};for(size_t k=0;k<3;++k)in[k]=club(world,fields.holders[k]).league==league;
        bmh::european_league_entrants(ranked,row.domestic.winner,row.domestic.runner_up,fields.holders,in,row.uefa_places,find,add);
        out.league_qualification.push_back(row);
    }
    for(size_t k=0;k<3;++k)if(!find(fields.holders[k]))add(int(k),fields.holders[k]);
    if(!world.catalogue.scope.germany) {
        bool assigned=false;
        for(auto id:out.final_rankings[5])if(!find(id)){add(2,id);assigned=true;break;}
        if(!assigned)invalid("world12: annual European field has no absent-country replacement place");
    }
    draw_fields(world,fields,game,random);
    out.game_seed_after=game.seed();out.watcom_seed_after=random.seed();out.game_draws=game.calls();out.watcom_draws=random.calls();
    if(state!=before)invalid("world12: annual qualification source changed while staging");
    return out;
}
void validate_annual_qualification(const CareerSnapshot& state,const UnpublishedAnnualQualification& packet) {
    if(packet.source_binding!=state)invalid("world12: annual qualification source binding is stale");
    if(stage_annual_qualification(state)!=packet)invalid("world12: annual qualification packet was changed");
}
ClubAttractionTitles annual_club_relations(const UnpublishedAnnualQualification& packet,ClubId id) {
    const auto& person=club(packet.source_binding.world,id);ClubAttractionTitles result;
    for(const auto& titles:packet.domestic_titles)if(titles.country==person.country){result.champion=titles.champion==id;result.holders[0]=titles.cup.winner==id;}
    for(size_t k=0;k<3;++k){result.holders[k+1]=packet.european_titles[k].winner==id;
        if(std::find(packet.next_european.fields[k].begin(),packet.next_european.fields[k].end(),id)!=packet.next_european.fields[k].end()) {
            if(result.european_cup)invalid("world12: annual relation club owns multiple current campaigns");result.european_cup=uint8_t(k+1);
        }}
    return result;
}
uint8_t annual_final_rank(const UnpublishedAnnualQualification& packet,ClubId id) {
    const auto& person=club(packet.source_binding.world,id);
    if(!person.league.valid())invalid("world12: annual final rank requires an actual league club");
    const auto& ranks=packet.final_rankings.at(person.league.value);const auto at=std::find(ranks.begin(),ranks.end(),id);
    if(at==ranks.end())invalid("world12: annual final ranking does not contain this club");
    return uint8_t(at-ranks.begin());
}
}
