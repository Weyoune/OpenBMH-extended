// SPDX-License-Identifier: GPL-3.0-only
#include "game/world12/inactive_club_state_codec.hpp"
#include "formats/binary.hpp"
#include <algorithm>
namespace bmh::world12 {
namespace {
using J=cli::Json;
const J& field(const J& input,const char* key){if(!input.object())throw FormatError("world12: inactive club object expected");const auto at=input.object()->find(key);if(at==input.object()->end())throw FormatError("world12: inactive club field missing");return at->second;}
uint32_t number(const J& value,uint32_t maximum){const auto n=value.integer();if(!n||*n<0||uint64_t(*n)>maximum)throw FormatError("world12: invalid inactive club integer");return uint32_t(*n);}
J ids(const auto& input){J::Array output;for(auto id:input)output.emplace_back(int64_t(id.value));return output;}
std::vector<ClubId> ids(const J& input,size_t maximum){const auto array=input.array();if(!array||array->size()>maximum)throw FormatError("world12: invalid inactive club identities");std::vector<ClubId> result;for(const auto& value:*array)result.push_back({number(value,0xfffffffeu)});return result;}
}
cli::Json encode_inactive_club_state(const std::optional<InactiveClubState>& input){if(!input)return nullptr;const auto& state=*input;J::Array qualified,fields,pools,domestic;for(const auto& list:state.qualified)qualified.push_back(ids(list));for(const auto& list:state.fields)fields.push_back(ids(list));for(const auto& list:state.reserve_pools)pools.push_back(ids(list));for(const auto& list:state.domestic_fields)domestic.emplace_back(J::Object{{"source_entry",int(list.source_entry)},{"teams",ids(list.teams)},{"seeds",ids(list.seeds)}});return J::Object{{"season",int(state.season)},{"primary",int(state.primary)},{"primary_cup_runner_token",int(state.primary_cup_runner_token)},{"primary_cup_seed",int64_t(state.primary_cup_seed.value)},{"holder_seeds",ids(state.holder_seeds)},{"qualified",std::move(qualified)},{"fields",std::move(fields)},{"reserve_pools",std::move(pools)},{"domestic_fields",std::move(domestic)}};}
std::optional<InactiveClubState> decode_inactive_club_state(const J& input){if(std::holds_alternative<std::nullptr_t>(input.value))return {};InactiveClubState result;result.season=number(field(input,"season"),32767);result.primary=Country(number(field(input,"primary"),61));result.primary_cup_runner_token=uint16_t(number(field(input,"primary_cup_runner_token"),65535));result.primary_cup_seed={number(field(input,"primary_cup_seed"),0xfffffffeu)};const auto holders=ids(field(input,"holder_seeds"),3);if(holders.size()!=3)throw FormatError("world12: inactive holder seed dimensions invalid");std::copy(holders.begin(),holders.end(),result.holder_seeds.begin());
    const auto lists=[&](const char* name,auto& output,size_t maximum){const auto array=field(input,name).array();if(!array||array->size()!=output.size())throw FormatError("world12: inactive setup array dimensions invalid");for(size_t i=0;i<output.size();++i)output[i]=ids(array->at(i),maximum);};lists("qualified",result.qualified,64);lists("fields",result.fields,64);lists("reserve_pools",result.reserve_pools,7);
    const auto domestic=field(input,"domestic_fields").array();if(!domestic||domestic->size()>2)throw FormatError("world12: inactive domestic array dimensions invalid");for(const auto& row:*domestic)result.domestic_fields.push_back({uint8_t(number(field(row,"source_entry"),35)),ids(field(row,"teams"),64),ids(field(row,"seeds"),20)});return result;
}
}
