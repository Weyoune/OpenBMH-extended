// SPDX-License-Identifier: GPL-3.0-only
// Almanach and Wechsel: the functions that build the two screen states from the career
// (world12_extended_pages.hpp). The screens themselves are in world12_extended_screens.cpp.
#include "ui/world12_extended_pages.hpp"
#include "ui/manager.hpp"
#include "ui/setup.hpp"
#include "formats/executable_layout.hpp"
#include "game/world12/job_offers.hpp"
#include "game/world12/manager_change.hpp"
#include <algorithm>
#include <cctype>
#include <map>
namespace bmh::ui {
using namespace bmh::world12;
namespace {
constexpr uint8_t kWhite=0x5f,kOwn=0x7a,kOtherHuman=0x75,kYellow=0x27;

std::string season_label(int season_year,uint32_t current,uint32_t season){
    const int year=season_year-int(current-season);
    const int next=(year+1)%100;
    return std::to_string(year)+"/"+(next<10?"0":"")+std::to_string(next);
}
uint8_t club_colour(const World& world,ClubId club,uint8_t seat){
    for(const auto& manager:world.managers)if(manager.club==club)return manager.seat==seat?kOwn:kOtherHuman;
    return kWhite;
}
std::string club_name(const World& world,ClubId club){
    const auto found=world.clubs.find(club);return found==world.clubs.end()?std::string("?"):utf8_to_original(found->second.name);
}
// Names of the competitions of a title row. Cup and European names come from the text file of the game
// where the desk takes them from too; without texts plain German names are used.
std::string title_name(const World& world,const AnnualTitleRow& row,const GameState& reference){
    const auto text=[&](size_t index,const char* fallback){
        const size_t id=(layout::CupDraw_FormatError_ExecutableField6-layout::TextPointerTable)/4+index;
        return reference.texts&&id<reference.texts->strings.size()&&!reference.texts->strings[id].empty()?reference.texts->strings[id]:std::string(fallback);
    };
    switch(row.kind){
    case AnnualTitleKind::league:return utf8_to_original(world.catalogue.league(row.league).name);
    case AnnualTitleKind::domestic:
        switch(DomesticCup(row.competition)){
        case DomesticCup::german_cup:return "DFB-Pokal";
        case DomesticCup::german_supercup:return "Supercup";
        case DomesticCup::fa_cup:return "FA Cup";
        case DomesticCup::league_cup:return "League Cup";
        case DomesticCup::english_supercup:return "Charity Shield";
        }
        return "Pokal";
    case AnnualTitleKind::european:return text(row.competition,row.competition==1?"UEFA-Cup":row.competition==2?"Champions League":"Pokalsieger-Cup");
    case AnnualTitleKind::indoor:return "Hallen-Masters";
    case AnnualTitleKind::national:return text(row.competition,"L{nderturnier");
    }
    return "?";
}
// The three display options of the career that these screens honour (saved per career where known,
// otherwise the values of the reference state).
void display_options(const CareerSnapshot& state,const GameState& reference,bool& decorated,bool& textured,bool& gradient){
    const auto option=[&](uint32_t address){return reference.memory.has(address)?reference.memory.u8(address):uint8_t(0);};
    decorated=option(layout::PlainBackground)==0;textured=option(layout::SolidPanelBackground)==0;gradient=option(layout::TextGradientDisabled)==0;
    if(state.world.economy&&state.world.economy->presentation_options)
        decorated=(*state.world.economy->presentation_options)[layout::PlainBackground-layout::LeaguePresentationOptions]==0;
}
AlmanacTab empty_tab(std::string label,const char* message){
    AlmanacTab tab;tab.label=std::move(label);tab.first_labels={""};tab.second_labels={""};
    AlmanacView view;view.columns={{12,false,""}};view.rows.push_back({{message},kWhite});tab.views.push_back(std::move(view));return tab;
}
}

AlmanacState world12_almanac_state(const CareerSnapshot& state,uint8_t seat,int season_year,const GameState& reference){
    const auto& world=state.world;
    AlmanacState out;
    const auto label=[&](uint32_t season){return season_label(season_year,state.season,season);};
    static const char* kNothing="Noch keine abgeschlossene Saison aufgezeichnet.";

    // --- Saison: the title holders of every archived season ---
    if(world.annual_title_archive&&!world.annual_title_archive->seasons.empty()){
        AlmanacTab tab;tab.label="Saison";tab.second_labels={""};
        for(const auto& season:world.annual_title_archive->seasons){
            tab.first_labels.push_back(label(season.season));
            AlmanacView view;view.columns={{12,false,"Wettbewerb"},{112,false,"Sieger"},{216,false,"Zweiter"}};
            for(const auto& row:season.titles)
                view.rows.push_back({{title_name(world,row,reference),club_name(world,row.winner),row.runner_up.valid()?club_name(world,row.runner_up):std::string()},club_colour(world,row.winner,seat)});
            tab.views.push_back(std::move(view));
        }
        tab.first=int(tab.first_labels.size())-1;out.tabs.push_back(std::move(tab));
    }else out.tabs.push_back(empty_tab("Saison",kNothing));

    // --- Tabelle and Torjaeger: recorded since OpenBMH-Extended 1.0 Beta 5 ---
    const ExtendedFeatures none;const auto& extended=world.extended?*world.extended:none;
    std::vector<LeagueId> leagues;
    for(const auto& season:extended.almanac)for(const auto& league:season.leagues)if(std::find(leagues.begin(),leagues.end(),league.league)==leagues.end())leagues.push_back(league.league);
    std::sort(leagues.begin(),leagues.end());
    if(!extended.almanac.empty()){
        AlmanacTab tables,scorers;tables.label="Tabelle";scorers.label="Torj{ger";
        for(auto league:leagues){const auto name=utf8_to_original(world.catalogue.league(league).name);tables.second_labels.push_back(name);scorers.second_labels.push_back(name);}
        const auto own=std::find_if(world.managers.begin(),world.managers.end(),[&](const auto& m){return m.seat==seat;});
        for(const auto& season:extended.almanac){
            tables.first_labels.push_back(label(season.season));scorers.first_labels.push_back(label(season.season));
            for(auto league:leagues){
                AlmanacView table,list;
                table.columns={{22,true,"Pl"},{28,false,"Verein"},{176,true,"Sp"},{196,true,"S"},{214,true,"U"},{232,true,"N"},{270,true,"Tore"},{304,true,"Pkt"}};
                list.columns={{22,true,"Pl"},{28,false,"Spieler"},{150,false,"Verein"},{304,true,"Tore"}};
                const auto record=std::find_if(season.leagues.begin(),season.leagues.end(),[&](const auto& item){return item.league==league;});
                if(record!=season.leagues.end()){
                    for(size_t i=0;i<record->table.size();++i){const auto& r=record->table[i];
                        table.rows.push_back({{std::to_string(i+1),club_name(world,r.club),std::to_string(r.games),std::to_string(r.wins),std::to_string(r.draws),std::to_string(r.defeats),
                            std::to_string(r.goals_for)+":"+std::to_string(r.goals_against),std::to_string(r.points)},club_colour(world,r.club,seat)});}
                    for(size_t i=0;i<record->scorers.size();++i){const auto& s=record->scorers[i];
                        list.rows.push_back({{std::to_string(i+1),s.name,club_name(world,s.club),std::to_string(s.goals)},club_colour(world,s.club,seat)});}
                }
                tables.views.push_back(std::move(table));scorers.views.push_back(std::move(list));
            }
        }
        // Open on the last season and the league of the manager's club.
        int own_league=0;
        if(own!=world.managers.end()){const auto club=world.clubs.find(own->club);if(club!=world.clubs.end())for(size_t i=0;i<leagues.size();++i)if(leagues[i]==club->second.league)own_league=int(i);}
        tables.first=scorers.first=int(tables.first_labels.size())-1;tables.second=scorers.second=own_league;
        out.tabs.push_back(std::move(tables));out.tabs.push_back(std::move(scorers));
    }else{out.tabs.push_back(empty_tab("Tabelle",kNothing));out.tabs.push_back(empty_tab("Torj{ger",kNothing));}

    // --- Ehrentafel: titles counted per club, one table per competition ---
    if(world.annual_title_archive&&!world.annual_title_archive->seasons.empty()){
        AlmanacTab tab;tab.label="Ehrentafel";tab.second_labels={""};
        std::vector<std::string> names;std::map<std::string,std::map<ClubId,int>> counts;
        for(const auto& season:world.annual_title_archive->seasons)for(const auto& row:season.titles){
            const auto name=title_name(world,row,reference);
            if(!counts.contains(name))names.push_back(name);
            ++counts[name][row.winner];
        }
        for(const auto& name:names){
            std::vector<std::pair<ClubId,int>> order(counts[name].begin(),counts[name].end());
            std::stable_sort(order.begin(),order.end(),[](const auto& a,const auto& b){return a.second>b.second;});
            AlmanacView view;view.columns={{22,true,"Pl"},{28,false,"Verein"},{304,true,"Titel"}};
            for(size_t i=0;i<order.size();++i)view.rows.push_back({{std::to_string(i+1),club_name(world,order[i].first),std::to_string(order[i].second)},club_colour(world,order[i].first,seat)});
            tab.first_labels.push_back(name);tab.views.push_back(std::move(view));
        }
        out.tabs.push_back(std::move(tab));
    }else out.tabs.push_back(empty_tab("Ehrentafel",kNothing));

    // --- Ewige Tabelle: the all-time counters of the original (highest division of each country) ---
    {
        AlmanacTab tab;tab.label="Ewige Tab.";tab.second_labels={""};
        for(const auto& league:world.catalogue.leagues){
            if(!league.active||league.local_index!=0)continue;
            struct Line{ClubId club;int points,goals_for,goals_against,wins,draws,defeats;};
            std::vector<Line> lines;
            for(const auto& [id,club]:world.clubs){
                if(club.kind!=ClubKind::club||club.country!=league.country)continue;
                const auto& b=club.season_stats.counters;const auto word=[&](size_t at){return int(b[at]|(b[at+1]<<8));};
                const Line line{id,word(30),word(32),word(34),word(36),word(38),word(40)};
                if(line.wins+line.draws+line.defeats>0)lines.push_back(line);
            }
            std::stable_sort(lines.begin(),lines.end(),[](const Line& a,const Line& b){
                return a.points!=b.points?a.points>b.points:(a.goals_for-a.goals_against)>(b.goals_for-b.goals_against);});
            AlmanacView view;view.columns={{22,true,"Pl"},{28,false,"Verein"},{176,true,"Sp"},{196,true,"S"},{214,true,"U"},{232,true,"N"},{270,true,"Tore"},{304,true,"Pkt"}};
            for(size_t i=0;i<lines.size();++i){const auto& l=lines[i];
                view.rows.push_back({{std::to_string(i+1),club_name(world,l.club),std::to_string(l.wins+l.draws+l.defeats),std::to_string(l.wins),std::to_string(l.draws),std::to_string(l.defeats),
                    std::to_string(l.goals_for)+":"+std::to_string(l.goals_against),std::to_string(l.points)},club_colour(world,l.club,seat)});}
            if(view.rows.empty())view.rows.push_back({{"","Noch kein Spiel gewertet."},kWhite});
            tab.first_labels.push_back(utf8_to_original(league.name));tab.views.push_back(std::move(view));
        }
        if(tab.views.empty())out.tabs.push_back(empty_tab("Ewige Tab.","Keine Liga im Spiel."));else out.tabs.push_back(std::move(tab));
    }

    // --- Manager: the career of every human manager ---
    {
        AlmanacTab tab;tab.label="Manager";tab.second_labels={""};
        for(const auto& manager:world.managers){
            AlmanacView view;view.columns={{12,false,"Saison"},{64,false,"Verein"},{190,false,"Liga"},{304,true,"Platz"}};
            if(const auto history=extended.manager_clubs.find(manager.seat);history!=extended.manager_clubs.end())
                for(const auto& entry:history->second){
                    std::string place;
                    for(const auto& season:extended.almanac)if(season.season==entry.season)for(const auto& league:season.leagues)if(league.league==entry.league)
                        for(size_t i=0;i<league.table.size();++i)if(league.table[i].club==entry.club)place=std::to_string(i+1)+".";
                    view.rows.push_back({{label(entry.season),club_name(world,entry.club),utf8_to_original(world.catalogue.league(entry.league).name),place},manager.seat==seat?kOwn:kOtherHuman});
                }
            // The running season and the title counters of the original manager record.
            if(const auto club=world.clubs.find(manager.club);club!=world.clubs.end()&&club->second.kind==ClubKind::club&&club->second.league.valid())
                view.rows.push_back({{label(state.season),club_name(world,manager.club),utf8_to_original(world.catalogue.league(club->second.league).name),"l{uft"},kWhite});
            view.rows.push_back({{""},kWhite});
            if(manager.career&&manager.career->annual_records)view.rows.push_back({{"","Meistertitel: "+std::to_string(manager.career->annual_records->league_titles)},kYellow});
            if(manager.career&&manager.career->cup_titles)view.rows.push_back({{"","Pokalsiege national: "+std::to_string((*manager.career->cup_titles)[0])+"   europ{isch: "+std::to_string((*manager.career->cup_titles)[1])},kYellow});
            tab.first_labels.push_back(utf8_to_original(manager.name));tab.views.push_back(std::move(view));
            if(manager.seat==seat)tab.first=int(tab.first_labels.size())-1;
        }
        if(tab.views.empty())out.tabs.push_back(empty_tab("Manager","Kein Manager."));else out.tabs.push_back(std::move(tab));
    }
    for(auto& tab:out.tabs){
        const bool seasons=tab.label=="Saison"||tab.label=="Tabelle"||tab.label=="Torj{ger";
        tab.first_previous=seasons?"Fr}her":"Vorige";tab.first_next=seasons?"Sp{ter":"N{chste";
        tab.heading=tab.label=="Tabelle"?"Abschlusstabelle":tab.label=="Ewige Tab."?"Ewige Tabelle":tab.label=="Saison"?"Titeltr{ger":tab.label;
    }
    display_options(state,reference,out.decorated_background,out.textured_window,out.gradient_text);
    return out;
}

JobOffersState world12_job_offers_state(const CareerSnapshot& state,uint8_t seat,const GameState& reference){
    const auto& world=state.world;JobOffersState out;
    const auto manager=std::find_if(world.managers.begin(),world.managers.end(),[&](const auto& m){return m.seat==seat;});
    if(manager==world.managers.end())throw FormatError("world12: job offers have no manager");
    out.manager_name=utf8_to_original(manager->name);out.current_club=club_name(world,manager->club);
    if(world.extended&&world.extended->job_offer_season==state.season)
        if(const auto offers=world.extended->job_offers.find(seat);offers!=world.extended->job_offers.end())
            for(const auto& offer:offers->second){
                const auto& club=world.clubs.at(offer.club);
                JobOfferRow row;row.club=offer.club;row.club_name=utf8_to_original(club.name);row.league_name=utf8_to_original(world.catalogue.league(club.league).name);row.strength=club_strength(world,offer.club);
                if(!world.extended->almanac.empty())for(const auto& league:world.extended->almanac.back().leagues)
                    for(size_t i=0;i<league.table.size();++i)if(league.table[i].club==offer.club)
                        row.place=std::to_string(i+1)+". "+utf8_to_original(world.catalogue.league(league.league).name);
                out.offers.push_back(std::move(row));
            }
    out.refusal=utf8_to_original(manager_change_refusal(state,seat));
    display_options(state,reference,out.decorated_background,out.textured_window,out.gradient_text);
    return out;
}

}
