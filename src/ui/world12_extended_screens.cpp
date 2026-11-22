// SPDX-License-Identifier: GPL-3.0-only
// Almanach and Wechsel: the two screens (world12_extended_pages.hpp). Their states are built in
// world12_extended_pages.cpp.
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
#include "ui/sprite_font.hpp"
#include "ui/draw.hpp"
#include "formats/font.hpp"
#include "formats/executable_layout.hpp"
#include <algorithm>
#include <cctype>
namespace bmh::ui {
namespace {
constexpr uint8_t kWhite=0x5f,kYellow=0x27,kBeige=0x6a;
constexpr int kWindowTop=19,kWindowBottom=200,kFirstBaseline=38,kLastBaseline=172;
std::string upper(std::string_view text){std::string out(text);for(auto& c:out)c=char(std::toupper(static_cast<unsigned char>(c)));return out;}
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
