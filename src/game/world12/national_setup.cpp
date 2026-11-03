// SPDX-License-Identifier: GPL-3.0-only
#include "game/world12/national_setup.hpp"
#include "game/state.hpp"
namespace bmh::world12 {
NationalStartupReceipt initialize_national_only_players(World& selected,const World& master,Country primary,
    const InitialStrengthRanges& ranges,const GameState& reference,bool foreign_rule,GameRandom& game,WatcomRandom& random){
    master.validate();selected.validate();const GameScope scope{false,false,true};
    if((primary!=Country::germany&&primary!=Country::england)||selected!=select_world_scope(master,scope))throw FormatError("world12: national startup requires its untouched actual master selection");
    const auto before=selected,original_master=master;const auto gs=game;const auto ws=random;auto staged_game=game;auto staged_random=random;auto complete=master;
    NationalStartupReceipt receipt;receipt.primary=primary;std::vector<ClubId> source_clubs;
    for(auto group:{TableGroup::germany,TableGroup::england,TableGroup::europe})for(auto league:master.catalogue.simulation_leagues(group))for(auto id:master.membership[league.value].clubs)source_clubs.push_back(id);
    for(size_t n=0;n<4;++n){const auto raw=reference.memory.u16(layout::EditorList+40*0x58+uint32_t(n*2));if(raw!=0xffff){if(raw>=source_clubs.size())throw FormatError("world12: national startup has unresolved Source entry40 bidder scratch");receipt.entry40_bidders[n]=source_clubs[raw];}}
    const auto plan=make_player_initialization_plan(master,GameScope{},ranges);
    initialize_career_players(complete,plan,staged_game,staged_random);initialize_career_markets_and_moods(complete,reference,staged_game,staged_random);initialize_career_player_pools(complete,plan,reference,foreign_rule,staged_game,staged_random,primary);
    auto candidate=selected;candidate.players=complete.players;candidate.players_initialized=true;candidate.markets=complete.markets;candidate.free_players=complete.free_players;
    for(auto& [id,club]:candidate.clubs)club=complete.clubs.at(id);
    if(!candidate.national_origins)throw FormatError("world12: national startup lost real roster origins");
    for(auto& origin:*candidate.national_origins){origin.club=complete.clubs.at(origin.club.id);origin.source_initialized=true;}
    candidate.amateur_slots.emplace();receipt.initialized_players=uint32_t(candidate.players.size());receipt.origin_clubs=uint32_t(candidate.national_origins->size());receipt.game_draws=staged_game.calls()-gs.calls();receipt.watcom_draws=staged_random.calls()-ws.calls();candidate.validate();
    if(selected!=before||master!=original_master||game.seed()!=gs.seed()||game.calls()!=gs.calls()||random.seed()!=ws.seed()||random.calls()!=ws.calls())throw FormatError("world12: national startup master/owner/RNG changed");
    selected=std::move(candidate);game=staged_game;random=staged_random;return receipt;
}
SeasonCalendar make_national_only_calendar(const Catalogue& catalogue,const std::vector<uint16_t>& source){
    if(catalogue.scope.club_play()||!catalogue.scope.international||(source.size()!=365&&source.size()!=366))throw FormatError("world12: national calendar requires its actual sole Source programme");
    SeasonCalendar result;result.days=uint16_t(source.size());for(size_t day=0;day<source.size();++day){const auto raw=source[day];if(raw&0x8000u)throw FormatError("world12: initial national calendar contains a runtime postponement");if(raw==0x4006)result.others.push_back({uint16_t(day),CalendarDomain::shared,raw});else if(!(raw&0x4000)&&(raw&0x1e00u))result.others.push_back({uint16_t(day),CalendarDomain::shared,uint16_t(raw&0x1e00u)});}
    result.validate(catalogue);return result;
}
NationalViewPreferences initial_national_view_preferences(const GameState& reference,uint8_t seat){
    if(seat>3)throw FormatError("world12: invalid national preference actor");NationalViewPreferences p;
    const auto word=[&](uint32_t at,std::optional<uint16_t>& value){if(reference.memory.has(at+1))value=reference.memory.u16(at);};
    word(layout::LeagueTableDisplayModes+uint32_t(seat)*2,p.table_display_word);word(layout::ManagerSettings+uint32_t(seat)*2,p.strengths_display_word);
    const auto pairings=layout::CareerSession_CloseLeagueFamily_ExecutableField+uint32_t(seat),other=layout::CareerSession_DispatchOtherGames_RuntimeField+uint32_t(seat);
    if(reference.memory.has(pairings))p.pairings_display_byte=reference.memory.u8(pairings);if(reference.memory.has(other))p.other_games_view=reference.memory.u8(other);const auto flags=layout::Flags+uint32_t(seat)*3;if(reference.memory.has(flags))p.statistics_flags=reference.memory.u8(flags);if(reference.memory.has(flags+2))p.player_statistics_flags=reference.memory.u8(flags+2);return p;
}
}
