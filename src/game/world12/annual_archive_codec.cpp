// SPDX-License-Identifier: GPL-3.0-only
#include "game/world12/annual_archive_codec.hpp"
#include "game/world12/catalogue.hpp"
namespace bmh::world12 {
namespace {
using J=cli::Json;using A=J::Array;using O=J::Object;
const J& field(const J& j,const char* key){if(!j.object()||!j.object()->contains(key))throw FormatError("world12: missing title archive field");return j.object()->at(key);}
const A& array(const J& j){if(!j.array())throw FormatError("world12: title archive array expected");return *j.array();}
uint32_t number(const J& j,uint32_t maximum=4294967294u){if(!j.integer()||*j.integer()<0||uint64_t(*j.integer())>maximum)throw FormatError("world12: title archive number out of range");return uint32_t(*j.integer());}
bool boolean(const J& j){if(!j.boolean())throw FormatError("world12: title archive boolean expected");return *j.boolean();}
J id(uint32_t value){return J(int64_t(value));}
ClubId optional_club(const J& j){if(j.integer())return {number(j)};if(std::holds_alternative<std::nullptr_t>(j.value))return {};throw FormatError("world12: invalid optional archived identity");}
}
J encode_annual_title_archive(const std::optional<AnnualTitleArchive>& value) {
    if(!value)return nullptr;A seasons,labels;
    for(const auto& season:value->seasons) {
        A titles;
        for(const auto& row:season.titles) {
            A podium;for(auto club:row.podium)podium.push_back(club.valid()?id(club.value):J(nullptr));
            titles.emplace_back(O{{"kind",int(row.kind)},{"country",row.country?J(int(*row.country)):J(nullptr)},
                {"league",row.league.valid()?id(row.league.value):J(nullptr)},{"competition",int(row.competition)},
                {"source_category",row.source_category?J(int(*row.source_category)):J(nullptr)},
                {"winner",id(row.winner.value)},{"runner_up",row.runner_up.valid()?id(row.runner_up.value):J(nullptr)},
                {"podium",std::move(podium)}});
        }
        seasons.emplace_back(O{{"season",id(season.season)},{"titles",std::move(titles)}});
    }
    for(const auto& [season,names]:value->unresolved_source_labels) {
        A encoded;for(const auto& name:names){A raw;for(unsigned char byte:name)raw.emplace_back(int(byte));encoded.emplace_back(std::move(raw));}
        labels.emplace_back(O{{"season",id(season)},{"labels",std::move(encoded)}});
    }
    return O{{"earlier_history_unknown",value->earlier_history_unknown},{"first_retained_season",id(value->first_retained_season)},
        {"seasons",std::move(seasons)},{"unresolved_source_labels",std::move(labels)}};
}
std::optional<AnnualTitleArchive> decode_annual_title_archive(const J& value) {
    if(std::holds_alternative<std::nullptr_t>(value.value))return {};AnnualTitleArchive result;
    result.earlier_history_unknown=boolean(field(value,"earlier_history_unknown"));result.first_retained_season=number(field(value,"first_retained_season"));
    for(const auto& season:array(field(value,"seasons"))) {
        AnnualSeasonTitles record;record.season=number(field(season,"season"));
        for(const auto& entry:array(field(season,"titles"))) {
            AnnualTitleRow row;row.kind=AnnualTitleKind(number(field(entry,"kind"),4));
            const auto& country=field(entry,"country");if(country.integer())row.country=Country(number(country,61));else if(!std::holds_alternative<std::nullptr_t>(country.value))throw FormatError("world12: invalid archived title country");
            const auto& league=field(entry,"league");if(league.integer())row.league={number(league,kLeagueSlots-1)};else if(!std::holds_alternative<std::nullptr_t>(league.value))throw FormatError("world12: invalid archived title league");
            row.competition=uint8_t(number(field(entry,"competition"),255));const auto& category=field(entry,"source_category");
            if(category.integer())row.source_category=uint8_t(number(category,8));else if(!std::holds_alternative<std::nullptr_t>(category.value))throw FormatError("world12: invalid archived Source category");
            row.winner={number(field(entry,"winner"))};row.runner_up=optional_club(field(entry,"runner_up"));
            const auto& podium=array(field(entry,"podium"));if(podium.size()!=4)throw FormatError("world12: invalid archived podium dimensions");
            for(size_t i=0;i<4;++i)row.podium[i]=optional_club(podium[i]);record.titles.push_back(row);
        }
        result.seasons.push_back(std::move(record));
    }
    for(const auto& entry:array(field(value,"unresolved_source_labels"))) {
        std::array<std::string,9> names;const auto& labels=array(field(entry,"labels"));if(labels.size()!=9)throw FormatError("world12: invalid unresolved title dimensions");
        for(size_t i=0;i<9;++i){const auto& raw=array(labels[i]);if(raw.size()>65535)throw FormatError("world12: unresolved title label is too long");for(const auto& byte:raw)names[i].push_back(char(number(byte,255)));}
        if(!result.unresolved_source_labels.emplace(number(field(entry,"season"),32767),std::move(names)).second)throw FormatError("world12: duplicate unresolved Source title season");
    }
    return result;
}
}
