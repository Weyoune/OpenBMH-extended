// SPDX-License-Identifier: GPL-3.0-only
#include "game/world12/weekly_players.hpp"
#include "game/world12/match_projection.hpp"
#include "game/world12/economy.hpp"
#include "game/weekly.hpp"
#include "game/week.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>
namespace bmh::world12 {
namespace {
HumanProjectionPolicy policy(const World& world) {
    if(!world.players_initialized||!world.settings)
        throw std::invalid_argument("world12: weekly players require an initialized career");
    const auto& s=*world.settings;
    return {s.mode,s.difficulty,s.foreign_rule,true};
}
uint32_t word(const Bytes& b,size_t at) {
    return uint32_t(b.at(at))|uint32_t(b.at(at+1))<<8|
        uint32_t(b.at(at+2))<<16|uint32_t(b.at(at+3))<<24;
}
void capture_notices(const AiMatchProjection& projection,std::vector<WeeklyPlayerNotice>& notices) {
    const auto& box=projection.state.mailbox;
    for(size_t row=0;row<kMailboxEntries;++row) {
        const auto at=row*kMailboxEntry;const auto local=word(box,at);
        if(!local)break;
        if(local&~((uint32_t(1)<<projection.human_managers())-1))
            throw std::invalid_argument("world12: weekly notice has an unqualified recipient");
        WeeklyPlayerNotice notice;
        for(uint8_t m=0;m<projection.human_managers();++m)
            if(local&(uint32_t(1)<<m))notice.recipients|=uint32_t(1)<<projection.global_manager(m);
        notice.date=uint16_t(uint16_t(box.at(at+8))<<8|box.at(at+9));
        std::copy_n(box.begin()+ptrdiff_t(at+10),kMailboxText,notice.text.begin());
        notices.push_back(notice);
    }
}
}
WeeklyPlayerOutcome update_weekly_players(CareerSnapshot& state,const GameState& reference,const TextDb& texts) {
    state.validate();const auto settings=policy(state.world);
    const auto before=state;auto candidate=state;
    GameRandom game(state.game_seed);WatcomRandom random(state.watcom_seed);
    std::vector<ClubId> clubs;
    for(auto table:{TableGroup::germany,TableGroup::england,TableGroup::europe})
        for(auto league:state.world.catalogue.table_leagues(table)) {
            const auto& membership=state.world.membership[league.value].clubs;
            clubs.insert(clubs.end(),membership.begin(),membership.end());
        }
    WeeklyPlayerOutcome result;
    // Keep complete club rosters together. Global IDs never enter the native
    // twelve-bit slots; at most 32 clubs occupy each numeric workspace.
    for(size_t first=0;first<clubs.size();first+=32) {
        const auto last=std::min(first+32,clubs.size());
        const std::vector<ClubId> batch(clubs.begin()+ptrdiff_t(first),clubs.begin()+ptrdiff_t(last));
        const bool human=std::any_of(candidate.world.managers.begin(),candidate.world.managers.end(),[&](const auto& m){
            return std::find(batch.begin(),batch.end(),m.club)!=batch.end();
        });
        auto projection=human?AiMatchProjection::create_for_humans(candidate,batch,reference,settings):
            AiMatchProjection::create(candidate.world,batch,reference);
        auto& local=projection.state;local.date=state.date;local.texts=&texts;
        local.mailbox.assign(kMailboxEntries*kMailboxEntry,0);
        MinuteReport report;
        for(size_t club=0;club<batch.size();++club) {
            const auto league=candidate.world.clubs.at(batch[club]).league;
            const bool foreign=!candidate.world.catalogue.league(league).playable;
            for(size_t row=0;row<24;++row) {
                const auto slot=club*24+row;const auto player=local.squad_slots.at(slot)&0xfff;
                if(player==0xfff)continue;
                const auto old=local.players.at(player).w35c>>27;
                weekly_player_update(local,slot,foreign,game,random,report);
                if(report.unsupported)throw FormatError(report.reason);
                const auto now=local.players.at(player).w35c>>27;
                ++result.roster_slots;
                result.injured+=old==0&&now!=0;
                result.recovered+=old!=0&&now==0;
                result.retired+=old!=31&&now==31;
                // One player's insurance and injury messages cannot overflow a
                // native mailbox. Retain their exact order, raw text and global
                // recipients for the application-owned persistent mailbox.
                const auto first_notice=result.notices.size();
                capture_notices(projection,result.notices);
                if(candidate.world.economy)for(size_t n=first_notice;n<result.notices.size();++n){const auto& m=result.notices[n];append_economy_message(*candidate.world.economy,{m.recipients,0,m.date,m.text});}
                std::fill(local.mailbox.begin(),local.mailbox.end(),0);
            }
        }
        projection.commit_match_state(candidate.world);
    }
    candidate.game_seed=game.seed();candidate.watcom_seed=random.seed();candidate.validate();
    if(state!=before)throw std::invalid_argument("world12: career changed during weekly player processing");
    result.game_draws=game.calls();result.watcom_draws=random.calls();
    state=std::move(candidate);return result;
}
TrainingOutcome train_manager_squads(CareerSnapshot& state,const GameState& reference) {
    state.validate();const auto settings=policy(state.world);
    if(state.world.managers.empty())throw std::invalid_argument("world12: training has no human manager");
    const auto before=state;auto candidate=state;std::vector<ClubId> clubs;
    for(const auto& manager:state.world.managers)clubs.push_back(manager.club);
    auto projection=AiMatchProjection::create_for_humans(candidate,clubs,reference,settings);
    GameRandom game(state.game_seed);WatcomRandom random(state.watcom_seed);
    familiarity_decay(projection.state,game,random);
    for(size_t manager=0;manager<projection.human_managers();++manager)
        train_squad(projection.state,int(manager),game,random);
    projection.commit_match_state(candidate.world);
    candidate.game_seed=game.seed();candidate.watcom_seed=random.seed();candidate.validate();
    if(state!=before)throw std::invalid_argument("world12: career changed during squad training");
    const TrainingOutcome result{game.calls(),random.calls(),projection.human_managers()};
    state=std::move(candidate);return result;
}
}
