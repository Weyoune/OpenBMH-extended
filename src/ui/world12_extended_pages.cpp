// SPDX-License-Identifier: GPL-3.0-only
// Almanach and Wechsel: state builders and the two screens (world12_extended_pages.hpp).
//
// Screen layout (320x240, small original font, 6 pixels high):
//   y   4  title
//   y  20  tabs (almanac only)
//   y  36  first selector  "<  label  >"        y 46 second selector (if the tab has one)
//   y  58  column headers, y 68.. rows, 8 pixels apart, at most kVisibleRows; longer tables scroll
//   y 231  hint line
// Mouse: left click on a tab, on the left/right third of a selector line, on the upper/lower half of the
// table (scrolls a long table); right click closes. Keys: left/right first selector, up/down second
// selector or scrolling, Escape closes.
#include "ui/world12_extended_pages.hpp"
#include "ui/manager.hpp"
#include "ui/setup.hpp"
#include "formats/font.hpp"
#include "formats/executable_layout.hpp"
#include "game/world12/job_offers.hpp"
#include "game/world12/manager_change.hpp"
#include <algorithm>
#include <cctype>
#include <map>
namespace bmh::ui {
using namespace bmh::world12;
namespace {
constexpr int kVisibleRows=20,kRowHeight=8,kFirstRowY=68;
constexpr uint8_t kWhite=0x5f,kOwn=0x7a,kOtherHuman=0x75,kYellow=0x27,kHeader=0x34,kPanel=0x3e,kPanelLight=0x3b,kEdge=0x31;

std::string upper(std::string_view text){std::string out(text);for(auto& c:out)c=char(std::toupper(static_cast<unsigned char>(c)));return out;}
void fill(Image& out,int x,int y,int w,int h,uint8_t colour){
    for(int yy=std::max(0,y);yy<std::min(out.height,y+h);++yy)for(int xx=std::max(0,x);xx<std::min(out.width,x+w);++xx)out.at(xx,yy)=colour;
}
// A filled panel with a light upper/left and a dark lower/right edge, like the panels of the original.
void panel(Image& out,int x,int y,int w,int h,uint8_t colour){
    fill(out,x,y,w,h,colour);fill(out,x,y,w,1,kEdge);fill(out,x,y,1,h,kEdge);fill(out,x,y+h-1,w,1,0x40);fill(out,x+w-1,y,1,h,0x40);
}
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
    if(world.economy&&world.economy->presentation_options)
        out.decorated_background=(*world.economy->presentation_options)[layout::PlainBackground-layout::LeaguePresentationOptions]==0;
    return out;
}

JobOffersState world12_job_offers_state(const CareerSnapshot& state,uint8_t seat){
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
    if(world.economy&&world.economy->presentation_options)
        out.decorated_background=(*world.economy->presentation_options)[layout::PlainBackground-layout::LeaguePresentationOptions]==0;
    return out;
}

namespace {
// Shared drawing base: background, title, hint line.
class Page:public World12ExtendedScreen {
protected:
    Page(const std::filesystem::path& data,bool decorated)
        :font_(load_font(asset_path(data/"F","BMH3.FNT"))),background_(decorated?load_vga(asset_path(data/"KB","129.VGA")):Image(320,240)){
        if(background_.width!=320||background_.height!=240)throw FormatError("extended page: unexpected background size");
    }
    Image frame(std::string_view title,std::string_view hint)const{
        auto out=background_;
        panel(out,4,3,312,13,kPanelLight);centred(out,160,7,title,kYellow);
        panel(out,4,17,312,210,kPanel);
        fill(out,0,228,320,12,0x44);centred(out,160,231,hint,kWhite);
        return out;
    }
    void left(Image& out,int x,int y,std::string_view text,uint8_t colour)const{font_.draw(out,x,y,text,colour);}
    void right(Image& out,int x,int y,std::string_view text,uint8_t colour)const{font_.draw(out,x-font_.text_width(text),y,text,colour);}
    void centred(Image& out,int x,int y,std::string_view text,uint8_t colour)const{font_.draw(out,x-font_.text_width(text)/2,y,text,colour);}
    Font font_;
    Image background_;
};

class AlmanacScreen final:public Page {
public:
    AlmanacScreen(const std::filesystem::path& data,std::shared_ptr<const AlmanacState> state)
        :Page(data,state->decorated_background),state_(std::move(state)){
        if(state_->tabs.empty())throw FormatError("almanac: no tab");
        for(const auto& tab:state_->tabs){
            if(tab.first_labels.empty()||tab.second_labels.empty()||tab.views.size()!=tab.first_labels.size()*tab.second_labels.size())throw FormatError("almanac: tab dimensions do not match");
            selection_.push_back({std::clamp(tab.first,0,int(tab.first_labels.size())-1),std::clamp(tab.second,0,int(tab.second_labels.size())-1),0});
        }
    }
    Image render()const override{
        auto out=frame("ALMANACH","Linke Maustaste: bl{ttern     Rechte Maustaste: zur}ck");
        const int count=int(state_->tabs.size()),width=304/count;
        for(int i=0;i<count;++i){
            panel(out,8+i*width,20,width-2,11,i==tab_?kPanelLight:0x3f);
            centred(out,8+i*width+(width-2)/2,23,state_->tabs[size_t(i)].label,i==tab_?kYellow:kWhite);
        }
        const auto& tab=state_->tabs[size_t(tab_)];const auto& chosen=selection_[size_t(tab_)];
        selector(out,36,tab.first_labels,chosen.first);
        if(tab.second_labels.size()>1)selector(out,46,tab.second_labels,chosen.second);
        const auto& view=current();
        for(const auto& column:view.columns){if(column.right_aligned)right(out,column.x,58,column.header,kHeader);else left(out,column.x,58,column.header,kHeader);}
        if(view.rows.empty())left(out,28,kFirstRowY,"Keine Aufzeichnung.",kWhite);
        for(int i=0;i<kVisibleRows&&chosen.scroll+i<int(view.rows.size());++i){
            const auto& row=view.rows[size_t(chosen.scroll+i)];
            for(size_t c=0;c<row.cells.size()&&c<view.columns.size();++c){
                const auto& column=view.columns[c];const int y=kFirstRowY+i*kRowHeight;
                if(column.right_aligned)right(out,column.x,y,row.cells[c],row.colour);else left(out,column.x,y,row.cells[c],row.colour);
            }
        }
        if(chosen.scroll>0)centred(out,311,60,"^",kYellow);
        if(chosen.scroll+kVisibleRows<int(view.rows.size()))centred(out,311,220,"v",kYellow);
        return out;
    }
    bool move(int,int)override{return false;}
    bool pointer_down(int x,int y,bool right_button)override{
        if(right_button){closed_=true;return true;}
        auto& chosen=selection_[size_t(tab_)];const auto& tab=state_->tabs[size_t(tab_)];
        if(y>=20&&y<31&&x>=8&&x<312){const int count=int(state_->tabs.size());const int index=std::min(count-1,(x-8)/(304/count));if(index==tab_)return false;tab_=index;return true;}
        if(y>=34&&y<44)return step(chosen.first,int(tab.first_labels.size()),x<107?-1:x>213?1:0,chosen);
        if(y>=44&&y<54&&tab.second_labels.size()>1)return step(chosen.second,int(tab.second_labels.size()),x<107?-1:x>213?1:0,chosen);
        if(y>=kFirstRowY)return scroll(y<kFirstRowY+kVisibleRows*kRowHeight/2?-10:10);
        return false;
    }
    bool key(std::string_view name)override{
        const auto k=upper(name);auto& chosen=selection_[size_t(tab_)];const auto& tab=state_->tabs[size_t(tab_)];
        if(k=="ESC"||k=="ESCAPE"){closed_=true;return true;}
        if(k=="LEFT")return step(chosen.first,int(tab.first_labels.size()),-1,chosen);
        if(k=="RIGHT")return step(chosen.first,int(tab.first_labels.size()),1,chosen);
        if(k=="UP"||k=="DOWN"){
            const int direction=k=="UP"?-1:1;
            if(int(current().rows.size())>kVisibleRows)return scroll(direction*5);
            if(tab.second_labels.size()>1)return step(chosen.second,int(tab.second_labels.size()),direction,chosen);
            return false;
        }
        if(k=="TAB"){tab_=(tab_+1)%int(state_->tabs.size());return true;}
        return false;
    }
    std::string state_json()const override{
        const auto& chosen=selection_[size_t(tab_)];
        return "{\"screen\":\"almanac\",\"tab\":"+std::to_string(tab_)+",\"first\":"+std::to_string(chosen.first)+",\"second\":"+std::to_string(chosen.second)+
            ",\"rows\":"+std::to_string(current().rows.size())+",\"scroll\":"+std::to_string(chosen.scroll)+"}";
    }
private:
    struct Chosen{int first=0,second=0,scroll=0;};
    const AlmanacView& current()const{
        const auto& tab=state_->tabs[size_t(tab_)];const auto& chosen=selection_[size_t(tab_)];
        return tab.views[size_t(chosen.first)*tab.second_labels.size()+size_t(chosen.second)];
    }
    void selector(Image& out,int y,const std::vector<std::string>& labels,int index)const{
        if(labels.size()>1){centred(out,60,y,index>0?"<":"",kYellow);centred(out,260,y,index+1<int(labels.size())?">":"",kYellow);}
        centred(out,160,y,labels[size_t(index)],kWhite);
    }
    bool step(int& value,int count,int direction,Chosen& chosen){
        const int next=std::clamp(value+direction,0,count-1);if(next==value)return false;value=next;chosen.scroll=0;return true;
    }
    bool scroll(int rows){
        auto& chosen=selection_[size_t(tab_)];const int most=std::max(0,int(current().rows.size())-kVisibleRows);
        const int next=std::clamp(chosen.scroll+rows,0,most);if(next==chosen.scroll)return false;chosen.scroll=next;return true;
    }
    std::shared_ptr<const AlmanacState> state_;
    std::vector<Chosen> selection_;
    int tab_=0;
};

class JobOffersScreen final:public Page {
public:
    JobOffersScreen(const std::filesystem::path& data,std::shared_ptr<const JobOffersState> state)
        :Page(data,state->decorated_background),state_(std::move(state)){}
    Image render()const override{
        auto out=frame("WECHSEL",confirm_>=0?"JA: Wechsel vollziehen     NEIN oder rechte Maustaste: zur}ck":"Angebot anklicken     Rechte Maustaste: zur}ck");
        left(out,12,24,state_->manager_name+", "+state_->current_club,kWhite);
        if(state_->offers.empty()){left(out,12,44,"Es liegt kein Angebot eines anderen Vereins vor.",kWhite);
            left(out,12,60,"Angebote kommen zum Saisonwechsel und gelten nur",kHeader);left(out,12,68,"am ersten Schreibtisch der neuen Saison.",kHeader);return out;}
        left(out,12,40,"Folgende Vereine bieten Ihnen das Traineramt an:",kWhite);
        left(out,20,56,"Verein",kHeader);left(out,140,56,"Liga",kHeader);right(out,232,56,"St{rke",kHeader);left(out,244,56,"Vorsaison",kHeader);
        for(size_t i=0;i<state_->offers.size();++i){
            const auto& row=state_->offers[i];const int y=row_y(int(i));const bool lit=int(i)==hover_||int(i)==confirm_;
            if(lit)fill(out,10,y-3,300,12,kPanelLight);
            const uint8_t colour=lit?kYellow:kWhite;
            left(out,20,y,row.club_name,colour);left(out,140,y,row.league_name,colour);right(out,232,y,std::to_string(row.strength),colour);left(out,244,y,row.place,colour);
        }
        if(!state_->refusal.empty())left(out,12,150,state_->refusal,0xf9);
        else left(out,12,150,"Der Wechsel ist ein Neuanfang: Kasse, Stadion und Sponsoren",kHeader),left(out,12,158,"des neuen Vereins; Ihre Laufbahn und Titel bleiben erhalten.",kHeader);
        if(confirm_>=0){
            panel(out,60,170,200,44,kPanelLight);centred(out,160,176,"Zu "+state_->offers[size_t(confirm_)].club_name+" wechseln?",kWhite);
            panel(out,84,192,56,14,kPanel);centred(out,112,196,"JA",kYellow);panel(out,180,192,56,14,kPanel);centred(out,208,196,"NEIN",kYellow);
        }
        return out;
    }
    bool move(int x,int y)override{
        if(confirm_>=0)return false;
        const int next=row_at(x,y);if(next==hover_)return false;hover_=next;return true;
    }
    bool pointer_down(int x,int y,bool right_button)override{
        if(confirm_>=0){
            if(right_button||(y>=192&&y<206&&x>=180&&x<236)){confirm_=-1;return true;}
            if(y>=192&&y<206&&x>=84&&x<140){accepted_=confirm_;closed_=true;return true;}
            return false;
        }
        if(right_button){closed_=true;return true;}
        const int row=row_at(x,y);
        if(row>=0&&state_->refusal.empty()){confirm_=row;return true;}
        return false;
    }
    bool key(std::string_view name)override{
        const auto k=upper(name);
        if(confirm_>=0){
            if(k=="J"||k=="ENTER"||k=="RETURN"){accepted_=confirm_;closed_=true;return true;}
            if(k=="N"||k=="ESC"||k=="ESCAPE"){confirm_=-1;return true;}
            return false;
        }
        if(k=="ESC"||k=="ESCAPE"){closed_=true;return true;}
        return false;
    }
    std::string state_json()const override{
        return "{\"screen\":\"job_offers\",\"offers\":"+std::to_string(state_->offers.size())+",\"confirm\":"+std::to_string(confirm_)+",\"blocked\":"+(state_->refusal.empty()?"false":"true")+"}";
    }
private:
    static int row_y(int index){return 70+index*16;}
    int row_at(int x,int y)const{
        for(int i=0;i<int(state_->offers.size());++i)if(x>=10&&x<310&&y>=row_y(i)-3&&y<row_y(i)+9)return i;
        return -1;
    }
    std::shared_ptr<const JobOffersState> state_;
    int hover_=-1,confirm_=-1;
};
}

std::unique_ptr<World12ExtendedScreen> make_almanac_screen(const std::filesystem::path& data,std::shared_ptr<const AlmanacState> state){
    return std::make_unique<AlmanacScreen>(data,std::move(state));
}
std::unique_ptr<World12ExtendedScreen> make_job_offers_screen(const std::filesystem::path& data,std::shared_ptr<const JobOffersState> state){
    return std::make_unique<JobOffersScreen>(data,std::move(state));
}
}
