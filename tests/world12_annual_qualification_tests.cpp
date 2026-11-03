// SPDX-License-Identifier: GPL-3.0-only
#include "annual_qualification_fixture.hpp"
#include "fixture_tables.hpp"
#include "game/world12/annual_qualification.hpp"
#include "game/world12/codec.hpp"
#include <iostream>
#include <set>
using namespace bmh;using namespace bmh::world12;
namespace {
void check(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
template<class F>void rejects(F f,const char* reason){bool refused=false;try{f();}catch(const std::exception&){refused=true;}check(refused,reason);}
// Independent reconstructed Source loops: physical random foreign cup slots,
// rank-based league places, exact country pool progression and scratch draws.
AnnualEuropeanFields source_oracle(const CareerSnapshot& state,const UnpublishedAnnualQualification& facts,
    GameRandom& game,WatcomRandom& random,bool qualification_only=false) {
    AnnualEuropeanFields out;for(size_t k=0;k<3;++k)out.holders[k]=(*state.world.european_campaigns)[k].winner;
    const auto find=[&](ClubId club){for(int k=0;k<3;++k)if(std::find(out.qualified[k].begin(),out.qualified[k].end(),club)!=out.qualified[k].end())return k+1;return 0;};
    std::vector<LeagueId> leagues{{state.world.catalogue.scope.germany?0u:5u}};
    for(uint32_t k:{10u,5u,11u,12u})if(state.world.catalogue.leagues[k].active&&std::find(leagues.begin(),leagues.end(),LeagueId{k})==leagues.end())leagues.push_back({k});
    for(auto league:leagues) {
        const auto& description=state.world.catalogue.league(league);const auto& ranks=facts.final_rankings[league.value];
        ClubId winner,runner;
        if(description.playable) {
            const auto final=std::find_if(state.cups.rbegin(),state.cups.rend(),[&](const auto& c){return c.cup==(description.country==Country::germany?DomesticCup::german_cup:DomesticCup::fa_cup);});
            winner=final->qualifiers().at(0);const auto& f=final->rounds.back().fixtures.at(0);runner=f.home==winner?f.away:f.home;
        }else {const auto& members=state.world.membership[league.value].clubs;const auto a=size_t(random.range(0,int(members.size())-1));size_t b;do b=size_t(random.range(0,int(members.size())-1));while(a==b);winner=members[a];runner=members[b];}
        int uefa=(description.country==Country::italy?3:4)+(state.world.catalogue.scope.germany?0:1);
        out.qualified[1].push_back(ranks[0]);
        if(state.world.clubs.at(out.holders[1]).league==league)out.qualified[1].push_back(find(out.holders[1])?ranks[1]:out.holders[1]);
        if(!find(winner))out.qualified[2].push_back(winner);else if(!find(runner))out.qualified[2].push_back(runner);
        if(state.world.clubs.at(out.holders[2]).league==league){if(!find(out.holders[2]))out.qualified[2].push_back(out.holders[2]);else if(!find(runner))out.qualified[2].push_back(runner);}
        if(state.world.clubs.at(out.holders[0]).league==league){if(!find(out.holders[0]))out.qualified[0].push_back(out.holders[0]);else ++uefa;}
        for(size_t rank=1;uefa;++rank)if(!find(ranks.at(rank))){out.qualified[0].push_back(ranks[rank]);--uefa;}
    }
    for(int k=0;k<3;++k)if(!find(out.holders[k]))out.qualified[k].push_back(out.holders[k]);
    if(!state.world.catalogue.scope.germany)for(auto id:facts.final_rankings[5])if(!find(id)){out.qualified[2].push_back(id);break;}
    if(qualification_only)return out;
    using Token=uint64_t;std::array<std::vector<Token>,3> lists;
    for(size_t k=0;k<3;++k)for(auto id:out.qualified[k])lists[k].push_back(id.value);
    std::array<std::vector<ClubId>,27> pools;
    for(const auto& [id,c]:state.world.clubs)if(c.kind==ClubKind::club&&!c.league.valid()&&uint8_t(c.country)>=5&&uint8_t(c.country)<32)pools[uint8_t(c.country)-5].push_back(id);
    const auto pick=[&](int country){Token value;bool used;do {const auto slot=random.range(0,6);value=country<27?pools[country].at(size_t(slot)).value:(Token(1)<<63)|Token(country*7+slot);used=false;for(const auto& list:lists)used|=std::find(list.begin(),list.end(),value)!=list.end();}while(used);return value;};
    const auto shuffle=[&](int cup,int first,int count){for(int p=0;p<count;++p){const auto q=game.range(0,count-1);std::swap(lists[cup].at(size_t(first+p)),lists[cup].at(size_t(first+q)));}};
    for(int c=0,n=int(lists[2].size());n<32;++n,++c)lists[2].push_back(pick(c));shuffle(2,0,32);
    for(int c=0,n=int(lists[1].size());n<24;++n,++c)lists[1].push_back(pick(c));shuffle(1,8,16);
    const int first=int(lists[0].size());int per=2;
    for(int c=0,n=first;n<64;++n,++c){for(int p=0;p<per;++p)lists[0].push_back(pick(c));if(n-first==16)per=1;}shuffle(0,0,64);
    const int counts[]={64,24,32};for(int k=0;k<3;++k)for(int i=0;i<counts[k];++i){check(!(lists[k][i]>>63),"oracle published a Source scratch token");out.fields[k].push_back({uint32_t(lists[k][i])});}
    return out;
}
CareerSnapshot* external_state=nullptr;
void disturb(int){draw_hook=nullptr;++external_state->game_seed;}
void ownership(CareerSnapshot state,const UnpublishedAnnualQualification& packet) {
    const auto before=state;auto changed=packet;changed.next_european.fields[0][0]=changed.next_european.fields[1][0];
    rejects([&]{validate_annual_qualification(state,changed);},"tampered next field accepted");check(state==before,"tampered packet changed its source");
    ++state.game_seed;const auto stale=state;rejects([&]{validate_annual_qualification(state,packet);},"stale seed owner accepted");check(state==stale,"stale validation rewound its new source");state=before;
    state.world.annual_awards_completed=state.season;rejects([&]{stage_annual_qualification(state);},"unpaid awards accepted");state=before;
    state.world.european_campaigns.reset();rejects([&]{stage_annual_qualification(state);},"unknown European finals reused starting holders");state=before;
    state.cups.erase(std::remove_if(state.cups.begin(),state.cups.end(),[](const auto& c){return c.cup==DomesticCup::fa_cup;}),state.cups.end());
    if(state.world.catalogue.scope.england)rejects([&]{stage_annual_qualification(state);},"unknown domestic final accepted");state=before;
    auto& campaign=(*state.world.european_campaigns)[0];std::swap(campaign.winner,campaign.runner_up);auto& final=*std::find_if(campaign.stages.begin(),campaign.stages.end(),[&](const auto& s){return s.rules.entry==campaign.active_entry;});std::swap(final.winner,final.runner_up);
    rejects([&]{stage_annual_qualification(state);},"title identities inconsistent with played final accepted");state=before;
    external_state=&state;draw_hook=disturb;rejects([&]{stage_annual_qualification(state);},"changed source owner accepted during random draw");draw_hook=nullptr;external_state=nullptr;auto expected=before;++expected.game_seed;check(state==expected,"staging rewound an external change or published partial fields");
}
}
int main()try {
    set_fixture_tables(bmh::test::authored_fixture_tables());size_t scopes=0;
    for(int mask=1;mask<8;++mask) {
        const GameScope scope{bool(mask&1),bool(mask&2),bool(mask&4)};if(!scope.club_play())continue;
        auto state=bmh::test::world12::completed_qualification_closing(scope);const auto before=state;
        const auto packet=stage_annual_qualification(state);validate_annual_qualification(state,packet);
        check(state==before&&packet.source_binding==before,"qualification changed its canonical source");
        check(packet.target_season==1&&packet.game_draws==112&&packet.game_seed_before==state.game_seed&&packet.watcom_seed_before==state.watcom_seed,"qualification lost source/target or staged RNG ownership");
        GameRandom game(state.game_seed);WatcomRandom random(state.watcom_seed);const auto expected=source_oracle(state,packet,game,random);
        check(packet.corrections.empty(),"valid Source qualification was unnecessarily corrected");
        check(packet.next_european==expected&&packet.game_seed_after==game.seed()&&packet.watcom_seed_after==random.seed()&&packet.watcom_draws==random.calls(),"next fields/order/G/W differ from independent Source loops");
        std::set<ClubId> unique;const size_t sizes[]={64,24,32};for(size_t k=0;k<3;++k){check(packet.next_european.fields[k].size()==sizes[k],"European field size changed");for(auto id:packet.next_european.fields[k])check(unique.insert(id).second&&state.world.clubs.at(id).kind==ClubKind::club,"duplicate or non-club current campaign");}
        const auto holder=packet.european_titles[0].winner;const auto relation=annual_club_relations(packet,holder);
        check(relation.holders[1]&&relation.champion&&relation.european_cup==2&&annual_final_rank(packet,holder)==0,"completed UEFA holder/champion lost its new CL ownership or Source zero rank");
        check(read_save(make_save(state))==state,"qualification source snapshot did not roundtrip");
        ownership(state,packet);++scopes;
    }
    for(const GameScope scope: {GameScope{true,true,false},GameScope{true,false,false},GameScope{false,true,false}}) {
        const auto state=bmh::test::world12::completed_qualification_closing(scope,true);const auto before=state;
        const auto packet=stage_annual_qualification(state);validate_annual_qualification(state,packet);
        GameRandom game(state.game_seed);WatcomRandom random(state.watcom_seed);
        const auto source=source_oracle(state,packet,game,random,true);
        check(source.qualified[2].size()==4&&packet.corrections.size()==1,
              "authored duplicate-title case did not exercise the actual Source CW deficit");
        const auto& correction=packet.corrections[0];const auto primary=LeagueId{scope.germany?0u:5u};
        const auto allocated=[&](ClubId id){for(const auto& ids:source.qualified)if(std::find(ids.begin(),ids.end(),id)!=ids.end())return true;return false;};
        const auto& ranked=packet.final_rankings[primary.value];const auto expected=std::find_if(ranked.begin(),ranked.end(),[&](auto id){return !allocated(id);});
        check(correction.league==primary&&!correction.reserve_pool&&expected!=ranked.end()&&correction.club==*expected,
              "CW deficit did not use the highest remaining actual same-country rank");
        for(size_t k=0;k<3;++k)check(std::equal(source.qualified[k].begin(),source.qualified[k].end(),packet.next_european.qualified[k].begin()),
              "CW correction changed an ordinary Source qualification");
        std::set<ClubId> actual;for(const auto& field:packet.next_european.fields)for(auto id:field)
            check(state.world.clubs.at(id).kind==ClubKind::club&&actual.insert(id).second,
                  "corrected European fields contain a national, scratch or repeated identity");
        const auto holder=packet.european_titles[2].winner;const auto relation=annual_club_relations(packet,holder);
        check(relation.holders[3]&&relation.european_cup==2&&actual.size()==120&&state==before,
              "CW title/CL participation or canonical source was lost in the correction");
        check(stage_annual_qualification(state)==packet,"corrected field draws were not deterministic");
    }
    std::cout<<"Annual qualification: "<<scopes<<" scopes, completed authored cup/European score programmes, Source G/W/fields, zero-based ranks, stale/tampered ownership passed; not played seasons\n";
    return 0;
}catch(const std::exception& e){draw_hook=nullptr;std::cerr<<e.what()<<'\n';return 1;}
