// SPDX-License-Identifier: GPL-3.0-only
// Validation and JSON form of ExtendedFeatures (see extended_features.hpp).
// JSON layout (member "extended" of the world document, schema 45):
//   { "desk_icons":    [ {"seat":n, "places":[[x,y],[x,y]]} ],
//     "almanac":       [ {"season":n, "leagues":[ {"league":id,
//                          "table":[[club,games,wins,draws,defeats,for,against,points]],
//                          "scorers":[ {"name":[bytes], "club":id, "goals":n} ]} ]} ],
//     "manager_clubs": [ {"seat":n, "seasons":[[season,club,league]]} ],
//     "job_offer_season": n,
//     "job_offers":    [ {"seat":n, "clubs":[id]} ] }
// Names are stored as byte arrays because they use the character set of the original game, not UTF-8.
#include "game/world12/extended_features.hpp"
#include "game/world12/world.hpp"
#include "formats/binary.hpp"
#include <set>
namespace bmh::world12 {
namespace {
using J=cli::Json;using A=J::Array;using O=J::Object;
[[noreturn]] void invalid(const char* reason){throw FormatError(std::string("world12: ")+reason);}
const J& field(const J& j,const char* key){
    if(!j.object()||!j.object()->contains(key))invalid("missing extended feature field");
    return j.object()->at(key);
}
const A& array(const J& j){if(!j.array())invalid("extended feature array expected");return *j.array();}
uint32_t number(const J& j,uint32_t maximum=4294967294u){
    if(!j.integer()||*j.integer()<0||uint64_t(*j.integer())>maximum)invalid("extended feature number out of range");
    return uint32_t(*j.integer());
}
J id(uint32_t value){return J(int64_t(value));}
}

std::array<DeskIconPlace,2> desk_icon_places(const World& world,uint8_t seat){
    if(world.extended)if(const auto found=world.extended->desk_icons.find(seat);found!=world.extended->desk_icons.end())return found->second;
    return kDefaultDeskIcons;
}

void validate_extended_features(const World& world){
    if(!world.extended)return;
    const auto& e=*world.extended;
    const auto real_club=[&](ClubId club){const auto found=world.clubs.find(club);return found!=world.clubs.end()&&found->second.kind==ClubKind::club;};
    const auto league_known=[&](LeagueId league){return league.valid()&&league.value<kLeagueSlots;};
    for(const auto& [seat,places]:e.desk_icons){
        if(seat>3)invalid("desk icon seat is out of range");
        for(const auto& place:places)if(place.x>320-32||place.y>240-25)invalid("additional desk icon lies outside the desk");
    }
    for(size_t i=0;i<e.almanac.size();++i){
        const auto& season=e.almanac[i];
        if(i&&season.season!=e.almanac[i-1].season+1)invalid("almanac seasons are not consecutive");
        std::set<uint32_t> leagues;
        for(const auto& league:season.leagues){
            if(!league_known(league.league)||!leagues.insert(league.league.value).second)invalid("almanac names an unknown or repeated league");
            if(league.table.size()<2||league.table.size()>32||league.scorers.size()>kAlmanacScorers)invalid("almanac league has invalid dimensions");
            std::set<ClubId> clubs;
            for(const auto& row:league.table){
                if(!real_club(row.club)||!clubs.insert(row.club).second)invalid("almanac table names an unknown or repeated club");
                if(unsigned(row.wins)+row.draws+row.defeats!=row.games)invalid("almanac table row does not add up");
            }
            for(const auto& scorer:league.scorers)if(!real_club(scorer.club)||scorer.name.size()>64)invalid("almanac scorer is invalid");
        }
    }
    for(const auto& [seat,seasons]:e.manager_clubs){
        if(seat>3)invalid("manager club history seat is out of range");
        for(size_t i=0;i<seasons.size();++i){
            if(i&&seasons[i].season<=seasons[i-1].season)invalid("manager club history is not ascending");
            if(!real_club(seasons[i].club)||!league_known(seasons[i].league))invalid("manager club history names an unknown club or league");
        }
    }
    std::set<ClubId> offered;
    for(const auto& [seat,offers]:e.job_offers){
        if(std::none_of(world.managers.begin(),world.managers.end(),[&](const auto& manager){return manager.seat==seat;}))invalid("job offer has no manager");
        if(offers.empty()||offers.size()>kMaximumJobOffers)invalid("job offer count is invalid");
        for(const auto& offer:offers){
            if(!real_club(offer.club)||!offered.insert(offer.club).second)invalid("job offer names an unknown or twice offered club");
            if(std::any_of(world.managers.begin(),world.managers.end(),[&](const auto& manager){return manager.club==offer.club;}))invalid("job offer names a club that has a manager");
        }
    }
}

J encode_extended_features(const std::optional<ExtendedFeatures>& value){
    if(!value)return nullptr;
    A icons,almanac,histories,offers;
    for(const auto& [seat,places]:value->desk_icons){
        A rows;for(const auto& place:places)rows.emplace_back(A{J(int(place.x)),J(int(place.y))});
        icons.emplace_back(O{{"seat",int(seat)},{"places",std::move(rows)}});
    }
    for(const auto& season:value->almanac){
        A leagues;
        for(const auto& league:season.leagues){
            A table,scorers;
            for(const auto& r:league.table)table.emplace_back(A{id(r.club.value),J(int(r.games)),J(int(r.wins)),J(int(r.draws)),J(int(r.defeats)),J(int(r.goals_for)),J(int(r.goals_against)),J(int(r.points))});
            for(const auto& s:league.scorers){
                A name;for(unsigned char byte:s.name)name.emplace_back(int(byte));
                scorers.emplace_back(O{{"name",std::move(name)},{"club",id(s.club.value)},{"goals",int(s.goals)}});
            }
            leagues.emplace_back(O{{"league",id(league.league.value)},{"table",std::move(table)},{"scorers",std::move(scorers)}});
        }
        almanac.emplace_back(O{{"season",id(season.season)},{"leagues",std::move(leagues)}});
    }
    for(const auto& [seat,seasons]:value->manager_clubs){
        A rows;for(const auto& s:seasons)rows.emplace_back(A{id(s.season),id(s.club.value),id(s.league.value)});
        histories.emplace_back(O{{"seat",int(seat)},{"seasons",std::move(rows)}});
    }
    for(const auto& [seat,list]:value->job_offers){
        A clubs;for(const auto& offer:list)clubs.emplace_back(id(offer.club.value));
        offers.emplace_back(O{{"seat",int(seat)},{"clubs",std::move(clubs)}});
    }
    return O{{"desk_icons",std::move(icons)},{"almanac",std::move(almanac)},{"manager_clubs",std::move(histories)},
        {"job_offer_season",id(value->job_offer_season)},{"job_offers",std::move(offers)}};
}

std::optional<ExtendedFeatures> decode_extended_features(const J& value){
    if(std::holds_alternative<std::nullptr_t>(value.value))return {};
    ExtendedFeatures result;
    for(const auto& entry:array(field(value,"desk_icons"))){
        const auto& rows=array(field(entry,"places"));if(rows.size()!=2)invalid("desk icon entry needs two places");
        std::array<DeskIconPlace,2> places;
        for(size_t i=0;i<2;++i){const auto& pair=array(rows[i]);if(pair.size()!=2)invalid("desk icon place needs x and y");places[i]={uint16_t(number(pair[0],319)),uint8_t(number(pair[1],239))};}
        if(!result.desk_icons.emplace(uint8_t(number(field(entry,"seat"),3)),places).second)invalid("repeated desk icon seat");
    }
    for(const auto& entry:array(field(value,"almanac"))){
        AlmanacSeason season;season.season=number(field(entry,"season"));
        for(const auto& item:array(field(entry,"leagues"))){
            AlmanacLeagueSeason league;league.league={number(field(item,"league"),uint32_t(kLeagueSlots-1))};
            for(const auto& raw:array(field(item,"table"))){
                const auto& c=array(raw);if(c.size()!=8)invalid("almanac table row needs eight values");
                league.table.push_back({{number(c[0])},uint8_t(number(c[1],255)),uint8_t(number(c[2],255)),uint8_t(number(c[3],255)),
                    uint8_t(number(c[4],255)),uint8_t(number(c[5],255)),uint8_t(number(c[6],255)),uint8_t(number(c[7],255))});
            }
            for(const auto& raw:array(field(item,"scorers"))){
                AlmanacScorer scorer;for(const auto& byte:array(field(raw,"name")))scorer.name.push_back(char(number(byte,255)));
                scorer.club={number(field(raw,"club"))};scorer.goals=uint8_t(number(field(raw,"goals"),255));
                league.scorers.push_back(std::move(scorer));
            }
            season.leagues.push_back(std::move(league));
        }
        result.almanac.push_back(std::move(season));
    }
    for(const auto& entry:array(field(value,"manager_clubs"))){
        std::vector<ManagerClubSeason> seasons;
        for(const auto& raw:array(field(entry,"seasons"))){
            const auto& c=array(raw);if(c.size()!=3)invalid("manager club season needs three values");
            seasons.push_back({number(c[0]),{number(c[1])},{number(c[2],uint32_t(kLeagueSlots-1))}});
        }
        if(!result.manager_clubs.emplace(uint8_t(number(field(entry,"seat"),3)),std::move(seasons)).second)invalid("repeated manager club history seat");
    }
    result.job_offer_season=number(field(value,"job_offer_season"));
    for(const auto& entry:array(field(value,"job_offers"))){
        std::vector<JobOffer> list;for(const auto& club:array(field(entry,"clubs")))list.push_back({{number(club)}});
        if(!result.job_offers.emplace(uint8_t(number(field(entry,"seat"),3)),std::move(list)).second)invalid("repeated job offer seat");
    }
    return result;
}
}
