// SPDX-License-Identifier: GPL-3.0-only
#include "game/world12/annual_accounts.hpp"
#include "game/world12/match_projection.hpp"
#include "game/world12/economy.hpp"
#include "game/world12/market_auctions.hpp"
#include <algorithm>
namespace bmh::world12 {
namespace {
AnnualTaxEvent event(const AiMatchProjection& projection,const SeasonTaxEvent& source) {
    AnnualTaxEvent out;out.manager=projection.global_manager(uint8_t(source.manager_slot));
    out.club=projection.global_club(uint32_t(source.team));out.source=source;
    out.source.manager_slot=out.manager;out.source.team=-1;return out;
}
}
UnpublishedAnnualAccounts stage_annual_accounts(const UnpublishedAnnualQualification& qualification,
    const GameState& reference,const TextDb& texts,const AnnualAccountsHooks& hooks) {
    const auto binding=qualification;
    validate_annual_qualification(binding.source_binding,binding);
    UnpublishedAnnualAccounts result;result.qualification=binding;result.snapshot=binding.source_binding;
    auto& candidate=result.snapshot;const auto& world=candidate.world;
    if(!world.settings||!world.economy||!world.markets||!candidate.transfers)
        throw FormatError("world12: annual accounts require canonical settings, economy, investments and transfers");
    for(const auto& manager:world.managers)if(!manager.career||!manager.career->annual_records||!manager.career->season_services||
        !world.economy->managers[manager.seat].monthly||!world.economy->managers[manager.seat].depreciation||
        !manager.match_state||!manager.match_state->attendance)
        throw FormatError("world12: annual accounts have unknown Source account, youth, attendance or tax backing");
    assign_transfer_loan_slots(candidate);
    GameRandom game(binding.game_seed_after);WatcomRandom random(binding.watcom_seed_after);SeasonTaxState tax;
    const auto settings=*world.settings;
    // All humans are included in every projection. Source manager-count tests
    // and academy exclusions therefore keep their original meaning.
    const auto managers=world.managers;
    for(const auto& manager:managers) {
        const auto country=world.clubs.at(manager.club).country;
        auto projection=AiMatchProjection::create_for_annual_accounts(candidate,country,reference,
            {settings.mode,settings.difficulty,settings.foreign_rule,true});
        install_career_economy(projection,candidate.world);install_transfer_loans(projection,candidate);
        auto& local=projection.state;local.texts=&texts;local.season=int(binding.target_season);
        uint8_t seat=0;for(;seat<projection.human_managers();++seat)if(projection.global_manager(seat)==manager.seat)break;
        if(seat==projection.human_managers())throw FormatError("world12: annual accounts lost manager identity");
        local.memory.set8(layout::ActiveManager,seat);
        for(size_t club=0;club<local.projected_match_clubs.size();++club)
            local.projected_match_clubs[club].table_rank=annual_final_rank(binding,projection.global_club(uint32_t(club)));
        const auto own=projection.local_club(manager.club);const auto roster=candidate.world.clubs.at(manager.club).roster;
        SeasonTaxHooks tax_hooks;
        tax_hooks.observe=[&](const GameState&,const SeasonTaxEvent& source){
            const auto notice=event(projection,source);
            if(!hooks.present||!hooks.present(notice))throw FormatError("world12: annual tax acknowledgement requires an owned presenter");
            result.tax_events.push_back(notice);
        };
        tax_hooks.choose=[&](const GameState&,const SeasonTaxEvent& source){
            const auto question=event(projection,source);
            if(!hooks.choose)throw FormatError("world12: annual tax decision requires an owned presenter");
            const auto answer=hooks.choose(question);if(!answer)throw FormatError("world12: annual tax decision cancelled");
            result.tax_decisions.push_back({question,*answer});return *answer;
        };
        MinuteReport report;
        settle_manager_annual_accounts(local,seat,game,random,report,tax_hooks,tax,annual_club_relations(binding,manager.club));
        if(report.unsupported)throw FormatError(report.reason);
        PlayerId recruited;
        for(size_t row=0;row<24;++row){const auto raw=local.squad_slots.at(own*24+row)&0xfff;if(raw==0xfff)continue;
            const auto player=projection.global_player(raw);
            if(std::none_of(roster.begin(),roster.end(),[&](const auto& slot){return slot.player==player;})){
                if(recruited.valid())throw FormatError("world12: annual academy produced multiple unowned players");recruited=player;
            }
        }
        if(recruited.valid()) {
            ClubId donor;
            for(const auto& [id,club]:candidate.world.clubs)if(std::any_of(club.roster.begin(),club.roster.end(),[&](const auto& slot){return slot.player==recruited;})){
                if(donor.valid())throw FormatError("world12: annual academy donor is ambiguous");donor=id;
            }
            if(!donor.valid()||donor==manager.club)throw FormatError("world12: annual academy lacks an actual donor");
            projection.commit_transfer_state(candidate.world,recruited,donor,manager.club,true);
            result.academy.push_back({manager.seat,recruited,donor,manager.club});
        } else projection.commit_match_state(candidate.world,true);
        capture_career_economy(candidate.world,projection);
        result.pending_sanctions[manager.seat]=local.memory.u8(layout::Season_SeasonAccounts_RuntimeField+seat);
    }
    candidate.game_seed=game.seed();candidate.watcom_seed=random.seed();candidate.validate();
    result.carried_fine=tax.carried_fine;result.game_draws=game.calls();result.watcom_draws=random.calls();
    if(qualification!=binding)throw FormatError("world12: annual qualification owner changed during tax presentation");
    return result;
}
void validate_annual_accounts(const UnpublishedAnnualAccounts& packet,const GameState& reference,const TextDb& texts) {
    size_t event_at=0,choice_at=0;AnnualAccountsHooks hooks;
    hooks.present=[&](const AnnualTaxEvent& event){return event_at<packet.tax_events.size()&&packet.tax_events[event_at++]==event;};
    hooks.choose=[&](const AnnualTaxEvent& event)->std::optional<bool>{if(choice_at>=packet.tax_decisions.size()||packet.tax_decisions[choice_at].question!=event)return {};return packet.tax_decisions[choice_at++].adjust;};
    const auto rebuilt=stage_annual_accounts(packet.qualification,reference,texts,hooks);
    if(rebuilt!=packet||event_at!=packet.tax_events.size()||choice_at!=packet.tax_decisions.size())
        throw FormatError("world12: stale or altered annual accounts packet");
}
}
