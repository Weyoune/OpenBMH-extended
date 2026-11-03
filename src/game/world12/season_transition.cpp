// SPDX-License-Identifier: GPL-3.0-only
#include "game/world12/season_transition.hpp"
#include "game/season_exchange.hpp"
#include "game/season.hpp"
#include "game/world12/manager_finance.hpp"
#include "game/world12/economy.hpp"
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
// FLTD's fifth internal table is not a promotion group. Source Void runs
// four visible groups and refills the fourth from the ordered amateur pool.
// The hidden division keeps its independent simulation membership and roster.
std::vector<LadderGroup> source_annual_ladder(const Catalogue& catalogue,Country country) {
    auto ladder=catalogue.rules(country).ladder;
    if(country==Country::england) {
        if(ladder.size()!=5)invalid("world12: invalid English internal ladder");
        ladder.resize(4);
    }
    return ladder;
}
struct DecisionResolvers {
    std::function<std::vector<ClubId>(LeagueId,const std::vector<Slot>&,
        const std::array<LeagueMembers,kLeagueSlots>&)> licences;
    std::function<std::vector<ClubId>(size_t)> newcomers;
};
}
static AnnualPromotionPlan build_annual_plan(const World& world,const SeasonResults& results,
    const std::array<uint8_t,kLeagueSlots>& rounds,AnnualPromotionDecisions decisions,
    const DecisionResolvers* resolvers=nullptr) {
    world.validate();results.validate(world.catalogue);
    if(!world.catalogue.selectable(decisions.country))invalid("world12: annual country ladder is disabled");
    const auto ladder=source_annual_ladder(world.catalogue,decisions.country);
    AnnualPromotionPlan plan;plan.decisions=decisions;plan.source_results=results;plan.source_rounds=rounds;
    plan.before=plan.after=world.membership;
    std::map<LeagueId,std::vector<Slot>> ranked;
    std::map<ClubId,Slot> initial_slots;
    std::set<LeagueId> country_leagues;
    std::set<ClubId> managers;
    for(const auto& manager:world.managers)managers.insert(manager.club);
    for(const auto& group:ladder)for(auto id:group.leagues)country_leagues.insert(id);
    const auto table_group=decisions.country==Country::germany?TableGroup::germany:TableGroup::england;
    for(auto id:world.catalogue.simulation_leagues(table_group)) {
        const auto& league=world.catalogue.league(id);
        if(!league.active||rounds[id.value]!=league.rounds())
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
    auto occupant=[&](Slot slot){return plan.after.at(slot.league.value).clubs.at(slot.position);};
    for(size_t group_index=0;group_index<ladder.size();++group_index) {
        const auto& ladder_group=ladder[group_index];
        std::vector<Slot> down;
        for(auto id:ladder_group.leagues) {
            const auto quota=world.catalogue.league(id).relegation;
            if(resolvers)licences.at(id)=resolvers->licences(id,ranked.at(id),plan.after);
            const auto& withdrawals=licences.at(id);
            for(auto club:withdrawals) {
                const auto slot=std::find_if(ranked.at(id).begin(),ranked.at(id).end(),[&](Slot s){return occupant(s)==club;});
                if(slot==ranked.at(id).end()||std::find(down.begin(),down.end(),*slot)!=down.end())
                    invalid("world12: annual withdrawal is absent or repeated in its current league");
                down.push_back(*slot);
            }
            bmh::append_relegation_places(down,ranked.at(id),int(quota>withdrawals.size()?quota-withdrawals.size():0));
        }
        size_t expected_departures=0;
        for(auto id:ladder_group.leagues)expected_departures+=std::max<size_t>(world.catalogue.league(id).relegation,licences.at(id).size());
        if(down.size()!=expected_departures)invalid("world12: unbalanced annual departure quota");
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
        else {
            if(resolvers)decisions.newcomers=resolvers->newcomers(group.count);
            std::set<ClubId> newcomers;
            if(decisions.newcomers.size()!=group.count)
                invalid("world12: all annual newcomer draws are required");
            for(auto id:decisions.newcomers) {
                const auto club=world.clubs.find(id);
                if(club==world.clubs.end()||club->second.kind!=ClubKind::club||club->second.country!=decisions.country||
                   club->second.league.valid()||!named(club->second.name)||!newcomers.insert(id).second)
                    invalid("world12: invalid, repeated or foreign annual newcomer");
            }
            group.promoted=decisions.newcomers;
        }
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
    for(auto& record:decisions.licences)record.withdrawn=licences.at(record.league);
    plan.decisions=std::move(decisions);
    return plan;
}
AnnualPromotionPlan plan_annual_promotions(const World& world,const SeasonResults& results,
    const std::array<uint8_t,kLeagueSlots>& rounds,const AnnualPromotionDecisions& decisions) {
    return build_annual_plan(world,results,rounds,decisions);
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

namespace bmh::world12 {
namespace {
void validate_amateur_pool(const World& world,const AnnualAmateurPool& pool) {
    if(!world.catalogue.selectable(pool.country))
        invalid("world12: enabled country amateur slot order is required");
    std::set<ClubId> expected,actual;
    for(const auto& [id,club]:world.clubs)
        if(club.kind==ClubKind::club&&club.country==pool.country&&!club.league.valid())expected.insert(id);
    for(auto id:pool.slots)if(!actual.insert(id).second)invalid("world12: duplicate physical amateur slot");
    if(expected!=actual)invalid("world12: physical amateur slots do not match current country clubs");
}
void put_word(Bytes& bytes,size_t at,uint32_t value) {
    for(size_t b=0;b<4;++b)bytes.at(at+b)=uint8_t(value>>(b*8));
}
}
AnnualAmateurPool initial_annual_amateur_pool(const World& world,Country country) {
    world.validate();
    if(world.players_initialized||world.settings||world.economy||world.markets||!world.managers.empty()||!world.loans.empty())
        invalid("world12: initial amateur slot order requires the immutable master");
    AnnualAmateurPool pool;pool.country=country;
    if(world.amateur_slots){pool.slots=(*world.amateur_slots)[country==Country::germany?0:1];validate_amateur_pool(world,pool);return pool;}
    for(const auto& [id,club]:world.clubs)
        if(club.kind==ClubKind::club&&club.country==country&&!club.league.valid())pool.slots.push_back(id);
    validate_amateur_pool(world,pool);return pool;
}
SeasonLicenceAccount annual_licence_account(const CareerSnapshot& state,uint8_t seat) {
    const auto manager=std::find_if(state.world.managers.begin(),state.world.managers.end(),[&](const auto& m){return m.seat==seat;});
    if(manager==state.world.managers.end()||!manager->finance||!manager->career||!manager->match_state||
       !state.world.markets||!state.world.economy||!state.transfers)
        invalid("world12: complete post-account licence finances are required");
    // Install only canonical numeric operands needed by the authoritative
    // asset calculation; no unrelated legacy club/player workspace is created.
    GameState local;local.manager_records.assign(4*0xb0c,0);
    install_manager_finance(local,seat,*manager->finance);
    const auto record=size_t(seat)*0xb0c;
    local.manager_records[record+0x26f]=manager->career->board_patience;
    local.manager_records[record+0x220]=manager->match_state->point_deduction;
    put_word(local.manager_records,record+0x330,uint32_t(state.transfers->balance[seat]));
    local.stocks.assign(state.world.markets->stocks.begin(),state.world.markets->stocks.end());
    local.memory.add_block(layout::FundBlock,state.world.markets->funds.size());
    for(size_t b=0;b<state.world.markets->funds.size();++b)
        local.memory.set8(layout::FundBlock+uint32_t(b),state.world.markets->funds[b]);
    const auto& economy=state.world.economy->managers[seat];
    put_word(local.manager_records,record+0x318,economy.deposit);
    local.memory.add_block(layout::Loans,4*0x78);
    for(size_t lender=0;lender<5;++lender)for(size_t field=0;field<6;++field)
        local.memory.set32(layout::Loans+uint32_t(seat*0x78+lender*0x18+field*4),economy.loans[lender][field]);
    return bmh::season_licence_account(local,seat);
}
AnnualPromotionOutcome evaluate_annual_promotion_decisions(const CareerSnapshot& state,
    const AnnualDecisionContext& context) {
    state.validate();validate_amateur_pool(state.world,context.amateurs);
    if(!state.world.amateur_slots||(*state.world.amateur_slots)[context.amateurs.country==Country::germany?0:1]!=context.amateurs.slots)
        invalid("world12: saved canonical amateur order is unknown or changed");
    AnnualPromotionOutcome out;out.source_world=state.world;out.pool_before=out.pool_after=context.amateurs;
    out.date=state.date;out.season=state.season;out.game_seed_before=state.game_seed;out.watcom_seed_before=state.watcom_seed;
    GameRandom game(state.game_seed);WatcomRandom random(state.watcom_seed);
    std::map<uint8_t,SeasonLicenceAccount> accounts;std::set<uint8_t> withdrawn_managers;
    for(const auto& manager:state.world.managers)accounts.emplace(manager.seat,annual_licence_account(state,manager.seat));
    AnnualPromotionDecisions decisions;decisions.country=context.amateurs.country;
    decisions.regional=context.regional;decisions.regional_winners=context.regional_winners;
    for(const auto& group:source_annual_ladder(state.world.catalogue,decisions.country))
        for(auto league:group.leagues)decisions.licences.push_back({league,{}});
    DecisionResolvers resolvers;
    resolvers.licences=[&](LeagueId id,const std::vector<Slot>& ranked,
        const std::array<LeagueMembers,kLeagueSlots>& membership) {
        const auto& league=state.world.catalogue.league(id);const auto& members=membership[id.value].clubs;
        std::vector<uint8_t> ranks(members.size());
        for(size_t rank=0;rank<ranked.size();++rank)ranks[ranked[rank].position]=uint8_t(rank);
        const auto human=[&](size_t position){return std::any_of(state.world.managers.begin(),state.world.managers.end(),
            [&](const auto& manager){return manager.club==members[position];});};
        std::vector<ClubId> withdrawals;
        for(const auto& manager:state.world.managers) {
            const auto member=std::find(members.begin(),members.end(),manager.club);
            if(member==members.end()||ranks[size_t(member-members.begin())]<=5||withdrawn_managers.contains(manager.seat))continue;
            if(bmh::season_licence_broke(accounts.at(manager.seat))) {
                withdrawals.push_back(manager.club);withdrawn_managers.insert(manager.seat);
                out.licence_notices.push_back({manager.club,manager.seat,id});
            }
        }
        const auto ai=bmh::select_ai_licence_withdrawal(league.local_index,state.world.managers.size(),ranks,human,game,random);
        if(ai) {
            // Source decrements this remaining count without a zero guard for
            // AI. Reject its unsupported negative-count path instead of
            // pretending another balanced relegation happened.
            if(withdrawals.size()>=league.relegation)
                invalid("world12: Source AI licence withdrawal exhausted the departure quota");
            withdrawals.push_back(members[*ai]);out.licence_notices.push_back({members[*ai],{},id});
        }
        return withdrawals;
    };
    resolvers.newcomers=[&](size_t count){return bmh::draw_season_newcomers(count,context.amateurs.slots,
        [&](ClubId id){return named(state.world.clubs.at(id).name);},random);};
    out.plan=build_annual_plan(state.world,state.results,state.league_rounds,std::move(decisions),&resolvers);
    for(const auto& group:out.plan.groups)for(const auto& exchange:group.exchanges)
        if(!exchange.lower.valid()&&!exchange.protected_manager) {
            const auto slot=std::find(out.pool_after.slots.begin(),out.pool_after.slots.end(),exchange.promoted);
            if(slot==out.pool_after.slots.end())invalid("world12: sampled newcomer lost its amateur slot");
            *slot=exchange.relegated;
        }
    out.game_seed_after=game.seed();out.watcom_seed_after=random.seed();
    out.game_draws=game.calls();out.watcom_draws=random.calls();
    if(state.world!=out.source_world||state.results!=out.plan.source_results||state.league_rounds!=out.plan.source_rounds||
       state.date!=out.date||state.season!=out.season||state.game_seed!=out.game_seed_before||state.watcom_seed!=out.watcom_seed_before)
        invalid("world12: annual decision owner changed during processing");
    return out;
}
AnnualMembershipStage stage_annual_membership(const CareerSnapshot& state,
    const AnnualPromotionOutcome& outcome,const TextDb& texts) {
    state.validate();const auto before=state;
    if(!state.world.transfer_market||std::any_of(state.world.transfer_market->rows.begin(),state.world.transfer_market->rows.end(),[](const auto& row){return row.has_value();}))
        invalid("world12: annual membership requires a known settled transfer market");
    if(state.world!=outcome.source_world||state.date!=outcome.date||state.season!=outcome.season||
       state.game_seed!=outcome.game_seed_before||state.watcom_seed!=outcome.watcom_seed_before)
        invalid("world12: stale annual promotion decision owner");
    validate_annual_promotion_plan(state.world,state.results,state.league_rounds,outcome.plan);
    const AnnualDecisionContext context{outcome.pool_before,outcome.plan.decisions.regional,outcome.plan.decisions.regional_winners};
    const auto expected=evaluate_annual_promotion_decisions(state,context);
    if(expected.plan.decisions!=outcome.plan.decisions||expected.plan.groups!=outcome.plan.groups||expected.pool_after!=outcome.pool_after||
       expected.licence_notices!=outcome.licence_notices||expected.game_seed_after!=outcome.game_seed_after||
       expected.watcom_seed_after!=outcome.watcom_seed_after||expected.game_draws!=outcome.game_draws||expected.watcom_draws!=outcome.watcom_draws)
        invalid("world12: modified annual promotion decision receipt");
    AnnualMembershipStage stage{state.world,outcome.pool_after,outcome.game_seed_after,outcome.watcom_seed_after};
    auto& world=stage.world;
    const auto message=[&](uint32_t recipients,const Bytes& text) {
        if(!world.economy)invalid("world12: annual notice requires canonical mailbox ownership");
        EconomyMessage mail;mail.recipients=recipients;mail.date=uint16_t(std::min<uint32_t>(state.date,65535));
        std::copy_n(text.begin(),std::min(text.size(),mail.text.size()),mail.text.begin());
        append_economy_message(*world.economy,mail);
    };
    const auto source_text=[&](uint32_t pointer) -> const std::string& {
        const auto id=text_id(pointer);
        if(id>=texts.strings.size()||id>=texts.offsets.size()||texts.offsets[id]>=texts.blob.size())
            invalid("world12: annual message text is not installed");
        return texts.strings[id];
    };
    const auto source_raw=[&](uint32_t pointer) {
        source_text(pointer);return texts.raw(text_id(pointer),kMailboxText);
    };
    uint32_t all=0x80000000u;for(const auto& manager:world.managers)all|=1u<<manager.seat;
    const auto post_licence=[&](const AnnualLicenceNotice& notice) {
        if(notice.manager)message(1u<<*notice.manager,source_raw(layout::Season_LicenceWithdrawals_TextPointer));
        else {
            const auto text=source_text(layout::Season_LicenceWithdrawals_TextPointer2)+world.clubs.at(notice.club).name+
                source_text(layout::Season_LicenceWithdrawals_TextPointer3);
            message(all,Bytes(text.begin(),text.end()));
        }
    };
    const auto ladder=source_annual_ladder(world.catalogue,outcome.plan.decisions.country);
    for(size_t group_index=0;group_index<outcome.plan.groups.size();++group_index) {
        for(const auto& notice:outcome.licence_notices)
            if(std::find(ladder[group_index].leagues.begin(),ladder[group_index].leagues.end(),notice.league)!=ladder[group_index].leagues.end())post_licence(notice);
        for(const auto& exchange:outcome.plan.groups[group_index].exchanges) {
            if(exchange.protected_manager)continue;
            auto& outgoing=world.clubs.at(exchange.relegated);auto& incoming=world.clubs.at(exchange.promoted);
            if(!exchange.lower.valid()) {
                incoming.roster=outgoing.roster;outgoing.roster={};
                for(auto& loan:world.loans) {
                    if(loan.owner==outgoing.id)loan.owner=incoming.id;
                    if(loan.borrower==outgoing.id)loan.borrower=incoming.id;
                }
                incoming.fairplay_points=outgoing.fairplay_points;outgoing.fairplay_points=0;
            }else std::swap(outgoing.fairplay_points,incoming.fairplay_points);
            incoming.league=exchange.upper;outgoing.league=exchange.lower;
            const auto manager=std::find_if(world.managers.begin(),world.managers.end(),[&](const auto& m){return m.club==incoming.id;});
            if(manager!=world.managers.end()) {
                if(!manager->career||!manager->career->season_services)
                    invalid("world12: promoted manager lacks advertisement contracts");
                // The eighteenth Source row ends at the manager tail. Its byte6
                // is separate from the 17 regular contracts and taker references.
                auto& services=*manager->career->season_services;
                if(!services.promotion_tail_flags)invalid("world12: promoted manager tail flags are unknown");
                for(auto& contract:services.advertising_contracts)contract[6]|=0x80;
                *services.promotion_tail_flags|=0x80;
                message(1u<<manager->seat,source_raw(layout::Season_Promotion_TextPointer));
            }
        }
    }
    world.membership=outcome.plan.after;
    (*world.amateur_slots)[stage.amateurs.country==Country::germany?0:1]=stage.amateurs.slots;
    // Statistics and stored opponents follow stable club identities. In
    // contrast, Source's amateur-edge loans follow the inherited league slot.
    world.validate();validate_amateur_pool(world,stage.amateurs);
    for(const auto& manager:world.managers) {
        const auto& club=world.clubs.at(manager.club);
        const auto visible=world.catalogue.player_leagues(club.country);
        if(club.league!=state.world.clubs.at(manager.club).league&&
           std::find(visible.begin(),visible.end(),club.league)==visible.end())
            invalid("world12: annual human destination is outside the Source player ladder");
    }
    if(state!=before)invalid("world12: annual membership owner changed during staging");
    return stage;
}
}
