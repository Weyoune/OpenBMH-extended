// SPDX-License-Identifier: GPL-3.0-only
#include "game/world12/regional_playoff_codec.hpp"
#include "game/world12/catalogue.hpp"
#include "formats/binary.hpp"
namespace bmh::world12 {
namespace {
using J=cli::Json;using A=J::Array;using O=J::Object;
const J& field(const J& j,const char* key){if(!j.object()||!j.object()->contains(key))throw FormatError("world12: missing regional playoff field");return j.object()->at(key);}
uint32_t number(const J& j,uint32_t maximum=4294967294u){if(!j.integer()||*j.integer()<0||uint64_t(*j.integer())>maximum)throw FormatError("world12: regional playoff number out of range");return uint32_t(*j.integer());}
const A& array(const J& j,size_t size){if(!j.array()||j.array()->size()!=size)throw FormatError("world12: regional playoff array has wrong size");return *j.array();}
ClubId club(const J& j){if(std::holds_alternative<std::nullptr_t>(j.value))return {};return {number(j)};}
}
J encode_regional_playoff(const std::optional<RegionalPlayoffSeason>& value){
    if(!value)return nullptr;const auto& c=*value;A dates,entrants,scores,qualifiers,deductions;
    for(auto day:c.dates)dates.emplace_back(int(day));for(auto id:c.entrants)entrants.push_back(id.valid()?J(int64_t(id.value)):J(nullptr));
    for(auto score:c.scores)scores.emplace_back(int(score));for(auto id:c.qualifiers)qualifiers.push_back(id.valid()?J(int64_t(id.value)):J(nullptr));
    for(const auto& [club,value]:c.activation_deductions)deductions.emplace_back(O{{"club",int64_t(club.value)},{"points",int(value)}});
    return O{{"season",int64_t(c.season)},{"flags",int(c.rules.flags)},{"dates",std::move(dates)},
        {"activation_day",c.activation_day?J(int(*c.activation_day)):J(nullptr)},{"entrants",std::move(entrants)},
        {"activation_deductions",std::move(deductions)},{"played",int(c.played)},{"revision",int64_t(c.revision)},{"scores",std::move(scores)},{"qualifiers",std::move(qualifiers)}};
}
std::optional<RegionalPlayoffSeason> decode_regional_playoff(const J& value){
    if(std::holds_alternative<std::nullptr_t>(value.value))return {};RegionalPlayoffSeason c;
    c.season=number(field(value,"season"));c.rules.flags=uint8_t(number(field(value,"flags"),255));
    const auto& dates=array(field(value,"dates"),6);for(size_t i=0;i<6;++i)c.dates[i]=uint16_t(number(dates[i],365));
    const auto& activation=field(value,"activation_day");if(!std::holds_alternative<std::nullptr_t>(activation.value))c.activation_day=uint16_t(number(activation,365));
    const auto& entrants=array(field(value,"entrants"),4);for(size_t i=0;i<4;++i)c.entrants[i]=club(entrants[i]);
    const auto& deductions=field(value,"activation_deductions");if(!deductions.array()||deductions.array()->size()>4)throw FormatError("world12: invalid regional activation deductions");
    for(const auto& row:*deductions.array())if(!c.activation_deductions.emplace(ClubId{number(field(row,"club"))},uint8_t(number(field(row,"points"),31))).second)throw FormatError("world12: repeated regional activation deduction");
    c.played=uint8_t(number(field(value,"played"),6));c.revision=number(field(value,"revision"),6);
    const auto& scores=array(field(value,"scores"),12);for(size_t i=0;i<12;++i)c.scores[i]=uint16_t(number(scores[i],65535));
    const auto& qualifiers=array(field(value,"qualifiers"),2);for(size_t i=0;i<2;++i)c.qualifiers[i]=club(qualifiers[i]);return c;
}
}
