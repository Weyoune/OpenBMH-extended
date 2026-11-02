// SPDX-License-Identifier: GPL-3.0-only
#include "game/world12/season_transition.hpp"
#include "game/season_exchange.hpp"
#include <set>
#include <stdexcept>

namespace bmh::world12 {
namespace {
struct Slot {
    LeagueId league;size_t position=0;
    bool operator==(const Slot&)const=default;
};
[[noreturn]] void invalid(const char* reason) {throw std::invalid_argument(reason);}
bool named(const std::string& name) {
    return std::any_of(name.begin(),name.end(),[](char c){return (c>='A'&&c<='Z')||(c>='a'&&c<='z');});
}
}
AnnualPromotionPlan plan_annual_promotions(const World& world,const SeasonResults& results,
    const std::array<uint8_t,kLeagueSlots>& rounds,const AnnualPromotionDecisions& decisions) {
    world.validate();results.validate(world.catalogue);
    if(!world.catalogue.selectable(decisions.country))invalid("world12: annual country ladder is disabled");
    const auto& ladder=world.catalogue.rules(decisions.country).ladder;
    AnnualPromotionPlan plan;plan.decisions=decisions;plan.source_results=results;plan.source_rounds=rounds;
    plan.before=plan.after=world.membership;
    std::map<LeagueId,std::vector<Slot>> ranked;
    std::map<ClubId,Slot> initial_slots;
    std::set<LeagueId> country_leagues;
    std::set<ClubId> managers;
    for(const auto& manager:world.managers)managers.insert(manager.club);
    for(const auto& group:ladder)for(auto id:group.leagues) {
        const auto& league=world.catalogue.league(id);country_leagues.insert(id);
        if(!league.active||!league.playable||rounds[id.value]!=league.rounds())
            invalid("world12: annual promotion requires complete country round cursors");
        for(const auto& score:results.leagues[id.value])if(!score.played)
            invalid("world12: annual promotion requires every country league result");
        TableQuery query;
        for(const auto& manager:world.managers)if(manager.match_state&&world.clubs.at(manager.club).league==id) {
            const auto cut=manager.match_state->point_deduction;
            if((cut>>5)==3)query.point_deductions[manager.club]=cut&31;
        }
        plan.standings[id.value]=compute_standings(world,results,id,rounds[id.value],query);
        const auto& members=world.membership[id.value].clubs;
        for(size_t pos=0;pos<members.size();++pos)initial_slots.emplace(members[pos],Slot{id,pos});
        for(const auto& row:plan.standings[id.value])ranked[id].push_back(initial_slots.at(row.club));
    }
    std::map<LeagueId,std::vector<ClubId>> licences;
    for(const auto& decision:decisions.licences)
        if(!country_leagues.contains(decision.league)||!licences.emplace(decision.league,decision.withdrawn).second)
            invalid("world12: repeated or foreign annual licence decision");
    if(licences.size()!=country_leagues.size())invalid("world12: complete annual licence decisions are required");
    const bool german=decisions.country==Country::germany;
    if((german&&decisions.regional!=RegionalPromotion::champions_only&&decisions.regional!=RegionalPromotion::playoff_complete)||
       (!german&&decisions.regional!=RegionalPromotion::not_applicable))
        invalid("world12: explicit country play-off disposition is required");
    if(decisions.regional!=RegionalPromotion::playoff_complete&&
       (decisions.regional_winners[0].valid()||decisions.regional_winners[1].valid()))
        invalid("world12: unexpected annual play-off winners");
    std::set<ClubId> newcomers;
    if(decisions.newcomers.size()!=ladder.back().relegation)
        invalid("world12: all annual newcomer draws are required");
    for(auto id:decisions.newcomers) {
        const auto club=world.clubs.find(id);
        if(club==world.clubs.end()||club->second.kind!=ClubKind::club||club->second.country!=decisions.country||
           club->second.league.valid()||!named(club->second.name)||!newcomers.insert(id).second)
            invalid("world12: invalid, repeated or foreign annual newcomer");
    }
    auto occupant=[&](Slot slot){return plan.after.at(slot.league.value).clubs.at(slot.position);};
    for(size_t group_index=0;group_index<ladder.size();++group_index) {
        const auto& ladder_group=ladder[group_index];
        std::vector<Slot> down;
        for(auto id:ladder_group.leagues) {
            const auto quota=world.catalogue.league(id).relegation;
            const auto& withdrawals=licences.at(id);
            if(withdrawals.size()>quota)invalid("world12: annual withdrawals exceed league quota");
            for(auto club:withdrawals) {
                const auto slot=std::find_if(ranked.at(id).begin(),ranked.at(id).end(),[&](Slot s){return occupant(s)==club;});
                if(slot==ranked.at(id).end()||std::find(down.begin(),down.end(),*slot)!=down.end())
                    invalid("world12: annual withdrawal is absent or repeated in its current league");
                down.push_back(*slot);
            }
            bmh::append_relegation_places(down,ranked.at(id),int(quota-withdrawals.size()));
        }
        if(down.size()!=ladder_group.relegation)invalid("world12: unbalanced annual departure quota");
        AnnualPromotionGroup group;group.count=down.size();
        std::vector<Slot> up;
        if(group_index+1<ladder.size()) {
            std::vector<std::vector<Slot>> feeders;
            for(auto id:ladder[group_index+1].leagues)feeders.push_back(ranked.at(id));
            std::optional<std::vector<Slot>> winners;
            if(feeders.size()>1&&decisions.regional==RegionalPromotion::playoff_complete) {
                winners.emplace();
                for(size_t i=0;i<feeders.size();++i) {
                    const auto winner=initial_slots.find(decisions.regional_winners[i]);
                    if(winner==initial_slots.end()||
                       std::none_of(feeders.begin(),feeders.end(),[&](const auto& feeder){
                           return std::find(feeder.begin()+1,feeder.end(),winner->second)!=feeder.end();
                       })||std::find(winners->begin(),winners->end(),winner->second)!=winners->end())
                        invalid("world12: play-off winner is not a distinct non-champion regional club");
                    // Stored play-off fields can cross regions: winner B can
                    // precede champion A. Do not infer a winner's home league
                    // from the index of its Source presentation field.
                    winners->push_back(winner->second);
                }
            }
            const auto selected=bmh::select_promotion_candidates(down.size(),feeders,winners);
            group.count=selected.count;up=selected.clubs;
            if(group.count>down.size()||group.count>up.size())invalid("world12: incomplete annual promotion pairings");
        }
        for(auto slot:down)group.relegated.push_back(occupant(slot));
        if(group_index+1<ladder.size())for(auto slot:up)group.promoted.push_back(occupant(slot));
        else group.promoted=decisions.newcomers;
        for(size_t i=0;i<group.count;++i) {
            const bool amateur=group_index+1==ladder.size();
            const bool protect=amateur&&managers.contains(group.relegated[i]);
            group.exchanges.push_back({group.relegated[i],group.promoted[i],down[i].league,
                amateur?LeagueId{}:up[i].league,protect});
            if(protect)continue;
            if(amateur) {
                plan.after[down[i].league.value].clubs[down[i].position]=group.promoted[i];
                plan.requires_amateur_roster_renewal=true;
            }else std::swap(plan.after[down[i].league.value].clubs[down[i].position],
                           plan.after[up[i].league.value].clubs[up[i].position]);
        }
        plan.groups.push_back(std::move(group));
    }
    return plan;
}
void validate_annual_promotion_plan(const World& world,const SeasonResults& results,
    const std::array<uint8_t,kLeagueSlots>& rounds,const AnnualPromotionPlan& plan) {
    const auto expected=plan_annual_promotions(world,results,rounds,plan.decisions);
    if(expected.source_results!=plan.source_results||expected.source_rounds!=plan.source_rounds||
       expected.before!=plan.before||expected.after!=plan.after||expected.groups!=plan.groups||
       expected.requires_amateur_roster_renewal!=plan.requires_amateur_roster_renewal)
        invalid("world12: stale or modified annual promotion plan");
    // StandingRow is a projection, not a save record. Compare every field so
    // even a changed final score that preserves the promoted clubs is stale.
    for(size_t l=0;l<kLeagueSlots;++l) {
        const auto& a=expected.standings[l];const auto& b=plan.standings[l];
        if(a.size()!=b.size())invalid("world12: changed annual standings");
        for(size_t i=0;i<a.size();++i) {
            const auto& x=a[i];const auto& y=b[i];
            if(x.club!=y.club||x.rank!=y.rank||x.previous_rank!=y.previous_rank||x.games!=y.games||
               x.wins!=y.wins||x.draws!=y.draws||x.defeats!=y.defeats||x.goals_for!=y.goals_for||
               x.goals_against!=y.goals_against||x.points!=y.points||x.form!=y.form)
                invalid("world12: changed annual standings");
        }
    }
}
}
