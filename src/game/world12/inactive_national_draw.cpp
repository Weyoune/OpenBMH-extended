// SPDX-License-Identifier: GPL-3.0-only
#include "game/world12/inactive_national_draw.hpp"
#include "game/world12/world.hpp"
#include "game/national_draw.hpp"
#include "game/season.hpp"
#include "game/schedule.hpp"
#include "game/cup.hpp"
#include "formats/le.hpp"
#include <set>
namespace bmh::world12 {
void validate_inactive_national_draw(const World& world){
    if(!world.inactive_national_draw)return;const auto& carry=*world.inactive_national_draw;
    const size_t sizes[]={4,8,6,24};
    if(!world.catalogue.scope.club_play()||world.catalogue.scope.international||carry.season>32767||!carry.native_national_first||carry.native_first_small>=carry.native_national_first||carry.field_tokens.size()!=sizes[carry.season%4]||std::set<uint16_t>(carry.field_tokens.begin(),carry.field_tokens.end()).size()!=carry.field_tokens.size()||carry.six_seed_tokens!=std::array<uint16_t,2>{uint16_t(carry.native_first_small+7-carry.native_national_first),1})throw FormatError("world12: invalid inactive Source national draw receipt");
}
void initialize_inactive_national_draw(World& world,const std::vector<uint16_t>& four,const LeImage& source){
    world.validate();if(world.inactive_national_draw||world.catalogue.scope.international||!world.catalogue.scope.club_play()||four.size()!=4||std::any_of(four.begin(),four.end(),[](auto token){return token>=62;}))throw FormatError("world12: inactive national draw needs its fresh actual four-field receipt");
    const auto at=cup_entry_address(14)+0x22;if(!source.contains(at+3))throw FormatError("world12: actual Source national qualifier metadata is missing");
    GameState native;native.memory.add_block(at,4);native.memory.set16(at,source.u16(at));native.memory.set16(at+2,source.u16(at+2));initialize_european_qualification_seed(native);
    InactiveNationalDraw carry;carry.native_national_first=uint16_t(kNoTeam);carry.native_first_small=uint16_t(league_first_team(5));carry.field_tokens=four;
    for(size_t n=0;n<2;++n)carry.four_seed_tokens[n]=uint16_t(native.memory.u16(at+uint32_t(n*2))-carry.native_national_first);
    // CareerSeasonSetup writes these exact entry13 qualifier words before the
    // first draw. Disabled nationals never replace them with played winners.
    carry.six_seed_tokens={uint16_t(carry.native_first_small+7-carry.native_national_first),1};auto candidate=world;candidate.inactive_national_draw=std::move(carry);candidate.validate();world=std::move(candidate);
}
NationalDrawOutcome advance_inactive_national_draw(World& world,uint32_t target,GameRandom& game,WatcomRandom& random){
    world.validate();if(!world.inactive_national_draw||target!=world.inactive_national_draw->season+1||target>32767)throw FormatError("world12: inactive Source national draw history is unknown, stale or noncontiguous");
    const auto before=world;const auto gs=game;const auto ws=random;auto staged_game=game;auto staged_random=random;auto candidate=world;auto& carry=*candidate.inactive_national_draw;const auto& seed=target%4==1?carry.four_seed_tokens:carry.six_seed_tokens;
    carry.field_tokens=draw_national_tournament_field(target,carry.field_tokens,target%2?std::optional<uint16_t>{seed[0]}:std::nullopt,target%4==3?std::optional<uint16_t>{seed[1]}:std::nullopt,staged_game,staged_random);carry.season=target;candidate.validate();
    if(world!=before||game.seed()!=gs.seed()||game.calls()!=gs.calls()||random.seed()!=ws.seed()||random.calls()!=ws.calls())throw FormatError("world12: inactive national draw owner/RNG changed");
    const NationalDrawOutcome out{staged_game.calls()-gs.calls(),staged_random.calls()-ws.calls()};world=std::move(candidate);game=staged_game;random=staged_random;return out;
}
}
