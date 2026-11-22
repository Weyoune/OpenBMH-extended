// SPDX-License-Identifier: GPL-3.0-only
// The two screens behind the additional desk icons of OpenBMH-Extended:
//   Almanach (menu kind 30) - the reference book of the career: titles, final tables, scorers, honours,
//                             all-time table, manager careers,
//   Wechsel  (menu kind 31) - the job offers of the manager at the desk; an offer can be accepted.
// Both are new screens without a model in the original game. They are built from plain text tables
// (AlmanacState) that a pure function makes from the career; the screen itself only pages through them.
// All texts are in the character set of the original game (ISO 646-DE), ready for the small font.
// The screens are hosted by World12ReadonlyScreen (world12_readonly_screen.hpp).
// OpenBMH deviates from the original: the desk of the shared world has two additional icons, Almanach (a reference book of the career) and Wechsel (job offers), outside the original desk layout.
#pragma once
#include "game/world12/save.hpp"
#include "game/state.hpp"
#include "formats/le.hpp"
#include "formats/vga.hpp"
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
namespace bmh::ui {
inline constexpr int kMenuAlmanac=30,kMenuJobOffers=31;

struct AlmanacColumn {
    int x=0;               // left edge, or right edge if right_aligned
    bool right_aligned=false;
    std::string header;
};
struct AlmanacRow {
    std::vector<std::string> cells;
    uint8_t colour=0x5f;   // palette index: 0x5f white, 0x7a the manager's own club, 0x75 another human manager
};
// One table of the book.
struct AlmanacView {
    std::vector<AlmanacColumn> columns;
    std::vector<AlmanacRow> rows;
};
// One tab. A tab has one or two selectors (for example season and league); views holds one table per
// combination, first selector major: views[first*second_labels.size()+second]. A tab without a second
// selector has one empty second label.
struct AlmanacTab {
    std::string label;      // on the page button
    std::string heading;    // in the window, large font
    // Captions of the buttons that step the first selector back and forth (seasons: earlier/later).
    std::string first_previous,first_next;
    std::vector<std::string> first_labels,second_labels;
    std::vector<AlmanacView> views;
    int first=0,second=0;   // selection shown first
};
struct AlmanacState {
    std::vector<AlmanacTab> tabs;
    // Display options of the career, as every original screen honours them: picture or plain background,
    // textured or plain window, shaded or plain heading text.
    bool decorated_background=true,textured_window=true,gradient_text=true;
};
// The almanac of the career as seen by the manager of `seat`. `season_year` is the calendar year in which
// the current season started. Never changes the career.
AlmanacState world12_almanac_state(const bmh::world12::CareerSnapshot&,uint8_t seat,int season_year,const GameState& reference);

struct JobOfferRow {
    bmh::world12::ClubId club;
    std::string club_name,league_name,place;   // place: last season's final place, or empty
    int strength=0;
};
struct JobOffersState {
    std::string manager_name,current_club;
    std::vector<JobOfferRow> offers;
    std::string refusal;        // why no offer can be accepted right now (German); empty if one can
    bool decorated_background=true,textured_window=true,gradient_text=true;
};
JobOffersState world12_job_offers_state(const bmh::world12::CareerSnapshot&,uint8_t seat,const GameState& reference);

// Common face of both screens for the host.
class World12ExtendedScreen {
public:
    virtual ~World12ExtendedScreen()=default;
    virtual Image render()const=0;
    virtual bool move(int x,int y)=0;                       // true if the picture changed
    virtual bool pointer_down(int x,int y,bool right)=0;
    virtual bool key(std::string_view key)=0;
    virtual std::string state_json()const=0;
    bool closed()const{return closed_;}
    // Index of the accepted job offer, -1 if none (always -1 for the almanac).
    int accepted()const{return accepted_;}
protected:
    bool closed_=false;
    int accepted_=-1;
};
// `rules` is the rule image of the game (glyph map of the sprite font, button text colours, text shading).
std::unique_ptr<World12ExtendedScreen> make_almanac_screen(const std::filesystem::path& data,std::shared_ptr<const AlmanacState>,const LeImage& rules);
std::unique_ptr<World12ExtendedScreen> make_job_offers_screen(const std::filesystem::path& data,std::shared_ptr<const JobOffersState>,const LeImage& rules);
}
