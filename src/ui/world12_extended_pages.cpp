// SPDX-License-Identifier: GPL-3.0-only
// Almanach and Wechsel: state builders and the two screens (world12_extended_pages.hpp).
//
// Both screens follow the original statistics screens (opportunities.cpp, alltime_table.cpp):
//   - the desk backdrop KB/129.VGA with the title in the sprite font in its title bar (or the plain
//     background of the display options), the dark strip at the bottom,
//   - a window from y 19 to y 200: texture KB/56.VGA with a light upper/left and dark lower/right edge,
//     the heading in the large font F/BMH2.FNT (colour 0x6a), column captions in the small font on the
//     same line, a line under them, rows in the small font from baseline 38 with the original spacing
//     (8, 7 or 6 pixels depending on the number of rows),
//   - original buttons (KB/0.VGA strips, sprite font captions): selector rows at y 177 and 189 like the
//     country buttons of the all-time table, page buttons at y 203 and 215 like the statistics menu.
// Left click presses a button, right click leaves the screen, as everywhere in the game.
// Not from the original: a table longer than the window scrolls by a click in its upper or lower half.
#include "ui/world12_extended_pages.hpp"
#include "ui/manager.hpp"
#include "ui/setup.hpp"
#include "formats/font.hpp"
#include "ui/sprite_font.hpp"
#include "ui/draw.hpp"
#include "formats/executable_layout.hpp"
#include "game/world12/job_offers.hpp"
#include "game/world12/manager_change.hpp"
#include <algorithm>
#include <cctype>
#include <map>
namespace bmh::ui {
using namespace bmh::world12;
namespace {
constexpr uint8_t kWhite=0x5f,kOwn=0x7a,kOtherHuman=0x75,kYellow=0x27,kBeige=0x6a;
constexpr int kWindowTop=19,kWindowBottom=200,kFirstBaseline=38,kLastBaseline=172;

std::string upper(std::string_view text){std::string out(text);for(auto& c:out)c=char(std::toupper(static_cast<unsigned char>(c)));return out;}
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

namespace {
// Sprite font captions are upper case; the state holds texts in the original character set, where the
// German umlauts are the bracket characters ({ | } small, [ \ ] capital).
std::string caption(std::string_view original){
    std::string out(original);
    for(auto& c:out){
        if(c=='{')c='[';else if(c=='|')c='\\';else if(c=='}')c=']';
        else c=char(std::toupper(static_cast<unsigned char>(c)));
    }
    return original_to_utf8(out);
}

// Shared drawing of both screens: backdrop, title, window, heading, buttons - all as the original
// statistics screens draw them.
class Page:public World12ExtendedScreen {
protected:
    Page(const std::filesystem::path& data,const LeImage& rules,bool decorated,bool textured,bool gradient)
        :rules_(rules),body_(load_font(asset_path(data/"F","BMH3.FNT"))),header_(load_font(asset_path(data/"F","BMH2.FNT"))),sprite_(data,rules),
         backdrop_(load_vga(asset_path(data/"KB","129.VGA"))),buttons_(load_vga(asset_path(data/"KB","0.VGA"))),window_(load_vga(asset_path(data/"KB","56.VGA"))),
         decorated_(decorated),textured_(textured),gradient_(gradient){
        if(backdrop_.width!=320||backdrop_.height!=240)throw FormatError("extended page: unexpected backdrop size");
    }
    // Backdrop with the title in the title bar, the bottom strip and the window with heading and line.
    Image frame(std::string_view title,std::string_view heading)const{
        Image out=backdrop_;
        if(!decorated_){out=Image(320,240);draw::fill(out,0,4,320,13,0x44);draw::fill(out,1,5,318,11,0x39);}
        sprite_.draw(out,SpriteFont::centered_title_x(sprite_.text_width(title)),13,title,5);
        draw::fill(out,0,228,320,11,decorated_?0x44:0);
        const int left=0,top=kWindowTop,right=319,bottom=kWindowBottom;
        if(textured_&&draw::inside(window_,left,top-19,right-left-1,bottom-top-1))draw::copy_clipped(out,window_,left,top-19,left+1,top+1,right-left-1,bottom-top-1,0xc0);
        else draw::fill(out,left,top,right-left+1,bottom-top+1,0x42);
        draw::fill(out,left,top,1,bottom-top+1,0x46);draw::fill(out,left,top,right-left+1,1,0x46);
        draw::fill(out,left,bottom,right-left+1,1,0x40);draw::fill(out,right,top,1,bottom-top+1,0x40);
        header_.draw(out,left+5,top+10-header_.baseline,heading,kBeige);
        draw::fill(out,left+3,top+11,right-left-6,1,kBeige);
        return out;
    }
    // Small font on a baseline, left aligned at x or right aligned to x.
    void text(Image& out,int x,int baseline,std::string_view value,uint8_t colour,bool right_aligned=false)const{
        body_.draw(out,right_aligned?x-body_.text_width(value):x,baseline-body_.baseline,value,colour);
    }
    // A column caption on the heading line: small font in the heading colour, shaded row by row with the
    // colour ramp of the original when the career has shaded text switched on.
    void heading_text(Image& out,int x,std::string_view value,bool right_aligned=false)const{
        const int width=body_.text_width(value);if(width<=0)return;
        if(right_aligned)x-=width;
        const int top=kWindowTop+5;
        if(!gradient_||rules_.u8(layout::TextGradientMode)!=2){body_.draw(out,x,kWindowTop+10-body_.baseline,value,kBeige);return;}
        Image mask(width,6);body_.draw(mask,0,5-body_.baseline,value,1);
        int ramp=0;while(ramp<32&&rules_.u8(layout::Advertising_Text_RuntimeField2+uint32_t(ramp))!=kBeige)++ramp;
        for(int y=0;y<6;++y)for(int xx=0;xx<width;++xx)
            if(mask.at(xx,y)&&x+xx>=0&&x+xx<out.width)out.at(x+xx,top+y)=ramp<32?rules_.u8(layout::Advertising_Text_RuntimeField+uint32_t(ramp*6+y)):kBeige;
    }
    // An original button, 96 pixels wide and 10 high: three strips of KB/0.VGA, sprite font caption;
    // selected = the light, pressed-in form; unavailable = the dim caption.
    void button(Image& out,int x,int y,std::string_view label,bool selected,bool available=true)const{
        constexpr int width=96,height=10;
        int dx=x,dy=y;
        if(selected){const auto ground=out.at(x,y+height);draw::fill(out,x,y,width+1,2,ground);draw::fill(out,x,y,2,height+1,ground);dx+=2;dy+=2;}
        else{
            const int right=std::min(319,x+width+1),bottom=std::min(239,y+height+1);
            draw::fill(out,x+2,y+height,right-x-1,bottom-y-height+1,0x40);draw::fill(out,x+width,y+2,right-x-width+1,bottom-y-1,0x40);
        }
        const int sy=selected?44:74;
        draw::copy_clipped(out,buttons_,24,sy,dx,dy,31,height,0xc0);
        int used=31;
        while(width-used>20){const int count=std::min(8,width-used-20);draw::copy_clipped(out,buttons_,24,sy+10,dx+used,dy,count,height,0xc0);used+=count;}
        const int tail=width-used;draw::copy_clipped(out,buttons_,45-tail,sy+20,dx+used,dy,tail,height,0xc0);
        const auto shown=caption(label);
        sprite_.draw(out,dx+width/2-sprite_.text_width(shown)/2,dy+height/2+2,shown,available?rules_.u8(layout::Advertising_Button_RuntimeField+uint32_t(selected)):4);
    }
    static bool in_button(int x,int y,int bx,int by){return x>=bx&&x<=bx+96&&y>=by&&y<=by+10;}
    // Places: selector rows like the country buttons of the all-time table, page rows like the statistics menu.
    static int selector_x(int column){return 12+column*99;}
    static int selector_y(int row){return 177+row*12;}
    static int page_x(int column){return 3+column*0x6c;}
    static int page_y(int row){return 0xcb+row*0x0c;}
    LeImage rules_;
    Font body_,header_;
    SpriteFont sprite_;
    Image backdrop_,buttons_,window_;
    bool decorated_,textured_,gradient_;
};

class AlmanacScreen final:public Page {
public:
    AlmanacScreen(const std::filesystem::path& data,std::shared_ptr<const AlmanacState> state,const LeImage& rules)
        :Page(data,rules,state->decorated_background,state->textured_window,state->gradient_text),state_(std::move(state)){
        if(state_->tabs.empty()||state_->tabs.size()>6)throw FormatError("almanac: needs one to six pages");
        for(const auto& tab:state_->tabs){
            if(tab.first_labels.empty()||tab.second_labels.empty()||tab.views.size()!=tab.first_labels.size()*tab.second_labels.size())throw FormatError("almanac: page dimensions do not match");
            selection_.push_back({std::clamp(tab.first,0,int(tab.first_labels.size())-1),std::clamp(tab.second,0,int(tab.second_labels.size())-1),0});
        }
    }
    Image render()const override{
        const auto& tab=state_->tabs[size_t(tab_)];const auto& chosen=selection_[size_t(tab_)];const auto& view=current();
        auto out=frame("ALMANACH",tab.heading);
        // Column captions share the line of the heading; as in the original all-time table the heading stands
        // in for the caption of the columns it covers.
        const int heading_end=5+header_.text_width(tab.heading)+6;
        for(const auto& column:view.columns)
            if((column.right_aligned?column.x-body_.text_width(column.header):column.x)>=heading_end)heading_text(out,column.x,column.header,column.right_aligned);
        // Rows: the original spacing rule of the all-time table.
        const int rows=int(view.rows.size()),shown=std::min(rows-chosen.scroll,visible_rows(rows)),spacing=row_spacing(rows);
        for(int i=0;i<shown;++i){
            const auto& row=view.rows[size_t(chosen.scroll+i)];const int baseline=kFirstBaseline+i*spacing;
            for(size_t c=0;c<row.cells.size()&&c<view.columns.size();++c)text(out,view.columns[c].x,baseline,row.cells[c],row.colour,view.columns[c].right_aligned);
        }
        if(rows==0)text(out,5,kFirstBaseline,"Keine Aufzeichnung.",kWhite);
        // A longer table: small markers at the right edge show that it continues.
        if(chosen.scroll>0)for(int i=0;i<3;++i)draw::fill(out,313-i,34+i,1+2*i,1,kBeige);
        if(chosen.scroll+shown<rows)for(int i=0;i<3;++i)draw::fill(out,311+i,170+i,5-2*i,1,kBeige);
        // Selector rows: step back, the current choice (shown pressed in), step forward.
        // The current choice is shown even when there is nothing to step to (one recorded season).
        if(tab.first_labels.size()>1||!tab.first_labels[0].empty()){
            button(out,selector_x(0),selector_y(0),tab.first_previous,pressed_==10,chosen.first>0);
            button(out,selector_x(1),selector_y(0),tab.first_labels[size_t(chosen.first)],true);
            button(out,selector_x(2),selector_y(0),tab.first_next,pressed_==12,chosen.first+1<int(tab.first_labels.size()));
        }
        if(tab.second_labels.size()>1){
            button(out,selector_x(0),selector_y(1),"Vorige",pressed_==13,chosen.second>0);
            button(out,selector_x(1),selector_y(1),tab.second_labels[size_t(chosen.second)],true);
            button(out,selector_x(2),selector_y(1),"N{chste",pressed_==15,chosen.second+1<int(tab.second_labels.size()));
        }
        for(int i=0;i<int(state_->tabs.size());++i)button(out,page_x(i%3),page_y(i/3),state_->tabs[size_t(i)].label,i==tab_);
        return out;
    }
    bool move(int,int)override{return false;}
    bool pointer_down(int x,int y,bool right_button)override{
        if(right_button){closed_=true;return true;}
        auto& chosen=selection_[size_t(tab_)];const auto& tab=state_->tabs[size_t(tab_)];
        for(int i=0;i<int(state_->tabs.size());++i)if(in_button(x,y,page_x(i%3),page_y(i/3))){if(i==tab_)return false;tab_=i;return true;}
        if(tab.first_labels.size()>1){
            if(in_button(x,y,selector_x(0),selector_y(0)))return step(chosen.first,int(tab.first_labels.size()),-1,chosen);
            if(in_button(x,y,selector_x(2),selector_y(0)))return step(chosen.first,int(tab.first_labels.size()),1,chosen);
        }
        if(tab.second_labels.size()>1){
            if(in_button(x,y,selector_x(0),selector_y(1)))return step(chosen.second,int(tab.second_labels.size()),-1,chosen);
            if(in_button(x,y,selector_x(2),selector_y(1)))return step(chosen.second,int(tab.second_labels.size()),1,chosen);
        }
        if(y>=31&&y<175)return scroll(y<103?-8:8);
        return false;
    }
    bool key(std::string_view name)override{
        const auto k=upper(name);auto& chosen=selection_[size_t(tab_)];const auto& tab=state_->tabs[size_t(tab_)];
        if(k=="ESC"||k=="ESCAPE"){closed_=true;return true;}
        if(k=="LEFT")return step(chosen.first,int(tab.first_labels.size()),-1,chosen);
        if(k=="RIGHT")return step(chosen.first,int(tab.first_labels.size()),1,chosen);
        if(k=="UP"||k=="DOWN"){
            const int direction=k=="UP"?-1:1;
            if(int(current().rows.size())>visible_rows(int(current().rows.size())))return scroll(direction*4);
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
    // 8 pixels for short tables, 7 up to a full English division (20 clubs), 6 beyond (the original all-time
    // table changes to 6 one row earlier; 7 keeps twenty rows apart and still fits the window).
    static int row_spacing(int rows){return rows<15?8:rows>20?6:7;}
    static int visible_rows(int rows){return (kLastBaseline-kFirstBaseline)/row_spacing(rows)+1;}
    const AlmanacView& current()const{
        const auto& tab=state_->tabs[size_t(tab_)];const auto& chosen=selection_[size_t(tab_)];
        return tab.views[size_t(chosen.first)*tab.second_labels.size()+size_t(chosen.second)];
    }
    bool step(int& value,int count,int direction,Chosen& chosen){
        const int next=std::clamp(value+direction,0,count-1);if(next==value)return false;value=next;chosen.scroll=0;return true;
    }
    bool scroll(int rows){
        auto& chosen=selection_[size_t(tab_)];const int total=int(current().rows.size()),most=std::max(0,total-visible_rows(total));
        const int next=std::clamp(chosen.scroll+rows,0,most);if(next==chosen.scroll)return false;chosen.scroll=next;return true;
    }
    std::shared_ptr<const AlmanacState> state_;
    std::vector<Chosen> selection_;
    int tab_=0,pressed_=-1;
};

class JobOffersScreen final:public Page {
public:
    JobOffersScreen(const std::filesystem::path& data,std::shared_ptr<const JobOffersState> state,const LeImage& rules)
        :Page(data,rules,state->decorated_background,state->textured_window,state->gradient_text),state_(std::move(state)){}
    Image render()const override{
        auto out=frame("WECHSEL","Stellenangebote");
        if(state_->offers.empty()){
            text(out,5,kFirstBaseline,"Es liegt kein Angebot eines anderen Vereins vor.",kWhite);
            text(out,5,kFirstBaseline+16,"Angebote kommen zum Saisonwechsel und gelten nur am ersten",kBeige);
            text(out,5,kFirstBaseline+24,"Schreibtisch der neuen Saison.",kBeige);
            button(out,page_x(2),page_y(0),"Zur}ck",false);
            return out;
        }
        heading_text(out,150,"Liga");heading_text(out,244,"St{rke",true);heading_text(out,252,"Vorsaison");
        text(out,5,kFirstBaseline,state_->manager_name+", "+state_->current_club+": folgende Vereine bieten Ihnen das Traineramt an.",kWhite);
        for(size_t i=0;i<state_->offers.size();++i){
            const auto& row=state_->offers[i];const int baseline=row_baseline(int(i));
            const uint8_t colour=int(i)==selected_?kYellow:kWhite;
            text(out,5,baseline,std::to_string(i+1)+".",colour);text(out,18,baseline,row.club_name,colour);text(out,150,baseline,row.league_name,colour);
            text(out,244,baseline,std::to_string(row.strength),colour,true);text(out,252,baseline,row.place,colour);
        }
        if(!state_->refusal.empty())text(out,5,120,state_->refusal,0xf9);
        else{
            text(out,5,120,"Der Wechsel ist ein Neuanfang: Kasse, Stadion und Sponsoren des neuen",kBeige);
            text(out,5,128,"Vereins. Ihre Laufbahn und Ihre Titel bleiben erhalten.",kBeige);
        }
        if(confirm_){
            text(out,5,150,"Wirklich zu "+state_->offers[size_t(selected_)].club_name+" wechseln?",kYellow);
            button(out,page_x(0),page_y(0),"Ja",false);button(out,page_x(2),page_y(0),"Nein",false);
        }else{
            if(selected_<0&&state_->refusal.empty())text(out,5,150,"Bitte einen Verein anklicken.",kBeige);
            button(out,page_x(0),page_y(0),"Annehmen",false,selected_>=0&&state_->refusal.empty());
            button(out,page_x(2),page_y(0),"Zur}ck",false);
        }
        return out;
    }
    bool move(int,int)override{return false;}
    bool pointer_down(int x,int y,bool right_button)override{
        if(confirm_){
            if(right_button||in_button(x,y,page_x(2),page_y(0))){confirm_=false;return true;}
            if(in_button(x,y,page_x(0),page_y(0))){accepted_=selected_;closed_=true;return true;}
            return false;
        }
        if(right_button||in_button(x,y,page_x(2),page_y(0))){closed_=true;return true;}
        for(int i=0;i<int(state_->offers.size());++i)
            if(x>=3&&x<316&&y>row_baseline(i)-8&&y<=row_baseline(i)+3){if(selected_==i)return false;selected_=i;return true;}
        if(in_button(x,y,page_x(0),page_y(0))&&selected_>=0&&state_->refusal.empty()){confirm_=true;return true;}
        return false;
    }
    bool key(std::string_view name)override{
        const auto k=upper(name);
        if(confirm_){
            if(k=="J"||k=="ENTER"||k=="RETURN"){accepted_=selected_;closed_=true;return true;}
            if(k=="N"||k=="ESC"||k=="ESCAPE"){confirm_=false;return true;}
            return false;
        }
        if(k=="ESC"||k=="ESCAPE"){closed_=true;return true;}
        return false;
    }
    std::string state_json()const override{
        return "{\"screen\":\"job_offers\",\"offers\":"+std::to_string(state_->offers.size())+",\"selected\":"+std::to_string(selected_)+
            ",\"confirm\":"+(confirm_?"true":"false")+",\"blocked\":"+(state_->refusal.empty()?"false":"true")+"}";
    }
private:
    static int row_baseline(int index){return kFirstBaseline+18+index*12;}
    std::shared_ptr<const JobOffersState> state_;
    int selected_=-1;
    bool confirm_=false;
};
}

std::unique_ptr<World12ExtendedScreen> make_almanac_screen(const std::filesystem::path& data,std::shared_ptr<const AlmanacState> state,const LeImage& rules){
    return std::make_unique<AlmanacScreen>(data,std::move(state),rules);
}
std::unique_ptr<World12ExtendedScreen> make_job_offers_screen(const std::filesystem::path& data,std::shared_ptr<const JobOffersState> state,const LeImage& rules){
    return std::make_unique<JobOffersScreen>(data,std::move(state),rules);
}
}
