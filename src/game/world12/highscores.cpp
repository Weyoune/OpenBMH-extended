// SPDX-License-Identifier: GPL-3.0-only
#include "game/world12/highscores.hpp"
#include "game/world12/economy.hpp"
#include <algorithm>
#include <bit>
namespace bmh::world12 {
OwnedHighscoreProjection project_owned_highscores(const CareerSnapshot& state,const GameState& reference,
    const HighscoreTable& existing) {
    state.validate();if(!state.world.highscore_signature)throw FormatError("world12: saved highscore signature is unknown");const auto signature=*state.world.highscore_signature;const auto before=state;OwnedHighscoreProjection out;out.signature=signature;out.ranked=signature!=0;out.rows=existing;
    if(!state.world.catalogue.scope.club_play()) {
        const auto& world=state.world;if(!world.catalogue.scope.international||world.managers.size()!=1||!world.national_coaches||!world.national_campaigns||!world.markets||!world.economy)throw FormatError("world12: national score has unknown actual actor/accounts/history/portfolio ledger");
        // The explicit national-only scope has no Original hidden human club.
        // Its owned score is deliberately unranked, including resumed known
        // signatures retained from earlier development captures.
        out.ranked=false;const auto& actor=world.managers.front();const auto nation=world.clubs.find(actor.club);if(nation==world.clubs.end()||nation->second.kind!=ClubKind::national_team)throw FormatError("world12: national score actor is not its actual nation");const auto record=std::find_if(world.national_coaches->begin(),world.national_coaches->end(),[&](const auto& record){return record.team==actor.club;});if(record==world.national_coaches->end()||!record->account.finance.baseline)throw FormatError("world12: national score account is unknown");
        const auto read=[](const auto& bytes,size_t at,size_t count){uint32_t value=0;for(size_t n=0;n<count;++n)value|=uint32_t(bytes.at(at+n))<<(8*n);return value;};uint32_t assets=uint32_t(record->account.finance.balance)+record->account.deposit;
        for(size_t fund=0;fund<6;++fund){const auto at=fund*0x1c;assets+=read(world.markets->funds,at,4)*read(world.markets->funds,at+0x14+actor.seat*2,2);}for(size_t stock=0;stock<12;++stock){const auto at=stock*0x5c;assets+=read(world.markets->stocks,at+0x4e,2)*read(world.markets->stocks,at+0x50+actor.seat*2,2);}for(const auto& loan:world.economy->managers[actor.seat].loans)assets-=loan[0];
        std::vector<std::array<uint8_t,4>> podium;for(const auto& places:world.national_campaigns->honours){std::array<uint8_t,4> row;for(size_t place=0;place<4;++place)row[place]=uint8_t(128|uint8_t(world.clubs.at(places[place]).country));podium.push_back(row);}
        OwnedHighscore scored;scored.manager=actor.seat;scored.club=actor.club;scored.national_basis=true;scored.score.assets=std::min(std::bit_cast<int32_t>(assets)/50000,100);scored.score.history=national_highscore_history(podium,uint8_t(nation->second.country));scored.score.total=std::max(scored.score.assets+scored.score.history,0);scored.candidate.score=scored.score.total;scored.candidate.manager_name=actor.name;scored.candidate.club_name=nation->second.name;out.managers.push_back(std::move(scored));
        if(state!=before)throw FormatError("world12: national score owner changed during projection");return out;
    }
    if(!out.ranked)return out;
    if(!state.world.settings||!state.world.economy||!state.world.markets)throw FormatError("world12: ranked highscore has unknown actual accounts/portfolios");
    std::vector<ClubId> clubs;for(const auto& manager:state.world.managers){if(!manager.career||!manager.career->season_records||!manager.career->annual_records||!manager.career->cup_titles)throw FormatError("world12: ranked highscore has unknown actual title/history backing");clubs.push_back(manager.club);}
    if(clubs.empty())throw FormatError("world12: ranked highscore has no actual manager");
    const auto& settings=*state.world.settings;auto projection=AiMatchProjection::create_for_humans(state,clubs,reference,{settings.mode,settings.difficulty,settings.foreign_rule,true});install_career_economy(projection,state.world);
    auto& local=projection.state;local.memory.add_block(layout::CareerSession_FormatError_RuntimeField7,2);local.memory.set16(layout::CareerSession_FormatError_RuntimeField7,signature);
    for(uint8_t manager=0;manager<projection.human_managers();++manager){OwnedHighscore result;result.manager=projection.global_manager(manager);result.club=projection.global_club(uint32_t(local.manager_teams[manager]));result.score=highscore_score(local,manager,HighscoreContext::OwnedManagerAssets);result.candidate=highscore_candidate(local,manager,HighscoreContext::OwnedManagerAssets);insert_highscore(out.rows,result.candidate);out.managers.push_back(std::move(result));}
    if(state!=before)throw FormatError("world12: ranked highscore owner changed during projection");return out;
}
}
