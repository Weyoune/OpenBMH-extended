// SPDX-License-Identifier: GPL-3.0-only
#include "game/world12/season_transition.hpp"
#include "game/season_exchange.hpp"
#include "game/schedule.hpp"
#include "game/table.hpp"
#include <iostream>
#include <numeric>
#include <stdexcept>

namespace {
using namespace bmh::world12;
void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
template<class F>void rejects(F f,const char* why){bool bad=false;try{f();}catch(const std::exception&){bad=true;}check(bad,why);}
void fixtures(){
    bmh::FixtureTableCache tables;
    for(int n:{18,20}) {
        std::vector<int> ring(size_t(n),0);std::iota(ring.begin(),ring.end(),0);
        for(int r=0;r<n-1;++r) {
            std::string row;
            for(int m=0;m<n/2;++m){row+=char('a'+ring[size_t(m)]);row+=char('a'+ring[size_t(n-1-m)]);}
            tables[n].push_back(row);const auto last=ring.back();
            for(int i=n-1;i>1;--i)ring[size_t(i)]=ring[size_t(i-1)];ring[1]=last;
        }
    }
    bmh::set_fixture_tables(std::move(tables));
}
World fixture(){
    World w;uint32_t player=100000,club=70000;
    for(const auto& league:w.catalogue.leagues) {
        auto& members=w.membership[league.id.value];members.league=league.id;
        for(int c=0;c<league.clubs;++c) {
            ClubRecord rec;rec.id={club++};rec.league=league.id;rec.country=league.country;rec.name="Club "+std::to_string(rec.id.value);rec.crest= {rec.id.value+9000};
            w.crests.emplace(rec.crest,CrestResource{"C/CREST.VGA",0});
            for(int s=0;s<11;++s){PlayerRecord p;p.id={player++};p.attributes={"Player "+std::to_string(p.id.value),50u|(60u<<7)|(65u<<14),0,0};w.players.emplace(p.id,p);rec.roster[size_t(s)]={p.id,1};}
            members.clubs.push_back(rec.id);w.clubs.emplace(rec.id,rec);
        }
    }
    for(auto country:{Country::germany,Country::england})for(int i=0;i<8;++i) {
        ClubRecord rec;rec.id={club++};rec.country=country;rec.name="Amateur "+std::to_string(rec.id.value);rec.crest={rec.id.value+9000};
        w.crests.emplace(rec.crest,CrestResource{"C/CREST.VGA",0});w.clubs.emplace(rec.id,rec);
    }
    w.validate();return w;
}
AnnualPromotionDecisions decisions(const World& w,Country country){
    AnnualPromotionDecisions d;d.country=country;d.regional=country==Country::germany?RegionalPromotion::champions_only:RegionalPromotion::not_applicable;
    const auto& rules=w.catalogue.rules(country);
    for(const auto& group:rules.ladder)for(auto l:group.leagues)if(country!=Country::england||w.catalogue.league(l).local_index<4)d.licences.push_back({l,{}});
    for(const auto& [id,c]:w.clubs)if(c.country==country&&!c.league.valid()&&d.newcomers.size()<rules.ladder.back().relegation)d.newcomers.push_back(id);
    return d;
}
}
int main()try{
    fixtures();auto w=fixture();const auto before=w;
    auto results=SeasonResults::empty(w.catalogue);std::array<uint8_t,kLeagueSlots> rounds{};
    for(const auto& l:w.catalogue.leagues) {
        rounds[l.id.value]=uint8_t(l.rounds());
        for(int r=1;r<=l.rounds();++r)for(int m=0;m<l.clubs/2;++m)
            results.result(w.catalogue,l.id,r,m)={true,uint8_t((r+m)%4),uint8_t((r+2*m)%3)};
    }
    auto de=decisions(w,Country::germany),en=decisions(w,Country::england);
    auto germany=plan_annual_promotions(w,results,rounds,de);
    auto england=plan_annual_promotions(w,results,rounds,en);
    check(germany.groups.size()==4&&england.groups.size()==4,"country ladder groups collapsed");
    check(germany.groups[0].count==3&&germany.groups[1].count==2&&germany.groups[2].count==4&&germany.groups[3].count==3,"German no-playoff Source quotas changed");
    for(const auto& g:england.groups)check(g.count==4,"English four visible-group quota changed");
    check(germany.requires_amateur_roster_renewal&&england.requires_amateur_roster_renewal,"amateur roster obligation hidden");
    for(size_t l=0;l<kLeagueSlots;++l) {
        if(l>=5)check(germany.after[l]==w.membership[l],"German plan changes another country");
        if(l<5||l>=9)check(england.after[l]==w.membership[l],"English plan changed hidden or foreign membership");
        check(germany.after[l].clubs.size()==w.membership[l].clubs.size()&&england.after[l].clubs.size()==w.membership[l].clubs.size(),"league capacity changed");
    }
    // Independent Original table oracle: use packed synthetic result bytes,
    // retaining the Source rank-to-draw-slot interpretation (flags 7, mode 1).
    const auto original_size=bmh::kLeagueSizes[0];int comparisons=0;
    for(const auto& l:w.catalogue.leagues)if(l.id.value<10&&l.clubs!=10) {
        bmh::kLeagueSizes[0]=l.clubs;bmh::GameState legacy;legacy.results.assign(9*0x17c,0xcc);legacy.league_rounds.assign(9,0);legacy.league_rounds[0]=uint8_t(l.rounds());
        for(int r=1;r<=l.rounds();++r)for(int m=0;m<l.clubs/2;++m) {
            const auto& s=results.result(w.catalogue,l.id,r,m);legacy.results[size_t(r-1)*10+size_t(m)]=uint8_t(s.home<<4|s.away);
        }
        const auto table=bmh::compute_league_table(legacy,0,0,l.rounds(),7,1);
        const auto& actual=(l.id.value<5?germany:england).standings[l.id.value];
        for(const auto& row:actual) {
            const auto& members=w.membership[l.id.value].clubs;const auto pos=std::find(members.begin(),members.end(),row.club)-members.begin();
            check(row.rank==table.rank(int(pos))+1&&row.points==uint8_t(table.points[size_t(pos)]),"annual final standings diverge from Original");++comparisons;
        }
    }
    bmh::kLeagueSizes[0]=original_size;
    // Shared ordering kernel: Source puts each regional winner before its
    // champion, while absent play-offs reduce the visible exchanged prefix.
    const std::vector<std::vector<int>> feeders{{10,11,12},{20,21,22}};
    auto paired=bmh::select_promotion_candidates(4,feeders,std::optional<std::vector<int>>{{12,21}});
    check(paired.count==4&&paired.clubs==std::vector<int>({12,10,21,20}),"Source regional winner/champion order changed");
    auto champions=bmh::select_promotion_candidates(4,feeders);
    check(champions.count==2&&champions.clubs==std::vector<int>({10,20}),"Source champions-only prefix changed");
    std::vector<int> down{11};bmh::append_relegation_places(down,std::vector<int>{10,11,12,13},2);
    check(down==std::vector<int>({11,13,12}),"licence withdrawal priority or duplicate exclusion changed");
    de.regional=RegionalPromotion::playoff_complete;
    de.regional_winners={germany.standings[3][2].club,germany.standings[2][1].club};
    auto playoff=plan_annual_promotions(w,results,rounds,de);
    check(playoff.groups[1].count==4&&playoff.groups[1].promoted[0]==de.regional_winners[0]&&playoff.groups[1].promoted[2]==de.regional_winners[1],"German stored play-off winner order changed");
    // Prior-group exchanges change occupants; frozen slot ranks must still
    // select the correct slots when the next group's withdrawals are applied.
    auto licence=decisions(w,Country::germany);
    const auto withdrawn=germany.standings[0][8].club;licence.licences[0].withdrawn={withdrawn};
    auto licensed=plan_annual_promotions(w,results,rounds,licence);
    check(licensed.groups[0].relegated[0]==withdrawn,"settled licence withdrawal not first");
    const auto lower_champion=germany.standings[1][0].club;
    licence.licences[1].withdrawn={withdrawn};
    licensed=plan_annual_promotions(w,results,rounds,licence);
    check(licensed.groups[1].relegated[0]==withdrawn&&licensed.groups[0].promoted[0]==lower_champion,"Source changed-slot lineage lost between groups");
    // Annual ranking includes the manager's existing Source point sanction.
    const auto penalized_club=germany.standings[0][0].club;
    auto penalized=w;Manager sanctioned{0,"Sanction",penalized_club,{0},0};
    sanctioned.match_state=ManagerMatchState{};sanctioned.match_state->point_deduction=uint8_t(3*32+31);
    penalized.managers.push_back(sanctioned);
    const auto penalty_plan=plan_annual_promotions(penalized,results,rounds,decisions(penalized,Country::germany));
    TableQuery penalty_query;penalty_query.point_deductions[penalized_club]=31;
    const auto penalty_table=compute_standings(penalized,results,{0},rounds[0],penalty_query);
    for(size_t i=0;i<penalty_table.size();++i)
        check(penalty_plan.standings[0][i].club==penalty_table[i].club&&penalty_plan.standings[0][i].points==penalty_table[i].points,
              "annual ranking omitted saved point sanction");
    // Original retains human clubs at the amateur edge, while retaining their
    // newcomer draw and visible row. No manager, squad or player id moves here.
    const auto protected_club=germany.groups.back().relegated[0];
    w.managers.push_back({0,"Manager",protected_club,{4},0});
    const auto protected_plan=plan_annual_promotions(w,results,rounds,decisions(w,Country::germany));
    check(protected_plan.groups.back().exchanges[0].protected_manager,"lowest human club not protected");
    check(std::find(protected_plan.after[4].clubs.begin(),protected_plan.after[4].clubs.end(),protected_club)!=protected_plan.after[4].clubs.end(),"human relegated into unplayable amateur pool");
    validate_annual_promotion_plan(w,results,rounds,protected_plan);
    auto changed=results;changed.leagues[0][0].home=uint8_t((changed.leagues[0][0].home+1)%4);
    rejects([&]{validate_annual_promotion_plan(w,changed,rounds,protected_plan);},"changed score reused annual plan");
    auto tampered=protected_plan;std::swap(tampered.after[0].clubs[0],tampered.after[0].clubs[1]);
    rejects([&]{validate_annual_promotion_plan(w,results,rounds,tampered);},"modified annual membership accepted");
    auto incomplete=results;incomplete.leagues[9][0]={};
    rejects([&]{plan_annual_promotions(w,incomplete,rounds,en);},"unplayed fifth-tier result accepted");
    auto short_rounds=rounds;--short_rounds[4];
    rejects([&]{plan_annual_promotions(w,results,short_rounds,decisions(w,Country::germany));},"incomplete country cursor accepted");
    auto bad=en;bad.licences.pop_back();rejects([&]{plan_annual_promotions(w,results,rounds,bad);},"missing licence owner accepted");
    bad=en;bad.newcomers[0]=de.newcomers[0];rejects([&]{plan_annual_promotions(w,results,rounds,bad);},"cross-country newcomer accepted");
    bad=en;bad.newcomers[1]=bad.newcomers[0];rejects([&]{plan_annual_promotions(w,results,rounds,bad);},"duplicate newcomer accepted");
    bad=de;bad.regional_winners[0]=germany.standings[2][0].club;rejects([&]{plan_annual_promotions(w,results,rounds,bad);},"champion repeated as playoff winner");
    bad=de;bad.regional=RegionalPromotion::not_applicable;rejects([&]{plan_annual_promotions(w,results,rounds,bad);},"missing regional disposition accepted");
    w.managers.clear();check(w==before,"annual planning or rejected plan mutated world/player records");
    auto scoped=select_world_scope(w,{true,false,false});
    rejects([&]{plan_annual_promotions(scoped,SeasonResults::empty(scoped.catalogue),rounds,en);},"AI-only English fallback gained annual ladder");
    std::cout<<comparisons<<" Original annual rank comparisons; German/English slot lineage, quotas, amateur protection and stale-plan rejection passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
