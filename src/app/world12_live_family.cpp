// SPDX-License-Identifier: GPL-3.0-only
#include "app/world12_live_family.hpp"
#include "game/world12/economy.hpp"
#include "game/world12/indoor.hpp"
#include "game/world12/fixtures.hpp"
#include "game/matchday.hpp"
#include <algorithm>
#include <set>
namespace bmh::app::world12 {
using namespace bmh::world12;
LeagueId live_group_league(const World& world,const AiMatchProjection& projection,const MatchGroup& group){
    if(group.desc.size()<0xd0||group.desc[0]!=0||group.fixtures.empty()||group.fixtures.size()%2)
        throw FormatError("world12: live group has no league fixture identity");
    const auto league=world.clubs.at(projection.global_club(uint32_t(group.fixtures.front()))).league;
    if(!league.valid()||!world.catalogue.league(league).active)throw FormatError("world12: live group has no active canonical league");
    for(auto team:group.fixtures)if(world.clubs.at(projection.global_club(uint32_t(team))).league!=league)
        throw FormatError("world12: live group mixes canonical leagues");
    return league;
}
CareerSnapshot live_family_snapshot(const CareerSnapshot& staged,const AiMatchProjection& projection,const MatchEngine& engine) {
    staged.validate();
    if(&engine.state!=&projection.state)throw FormatError("world12: live overview has another match workspace");
    auto display=staged;
    projection.commit_match_state(display.world);
    if(display.world.economy)capture_career_economy(display.world,projection);
    projection.commit_desktop_preferences(display);
    // These transient display scores are not a completed matchday. Keep the
    // real schedule separately in the UI selection, never relax saved clocks.
    display.calendar.reset();display.progress.reset();display.prepared_event.reset();
    if(display.desktop){display.desktop->initial_event.reset();display.desktop->autosave_pending=false;}
    if(display.world.indoor)display.world.indoor->date_pass.reset();
    std::set<LeagueId> seen;
    for(const auto& group:engine.groups) {
        if(group.desc.size()<0xd0)throw FormatError("world12: incomplete live group descriptor");
        if(group.desc[0]!=0)continue;
        const auto league=live_group_league(staged.world,projection,group);
        const auto& rules=display.world.catalogue.league(league);
        if(!rules.active||!seen.insert(league).second||group.fixtures.size()%2)
            throw FormatError("world12: repeated or inactive live league");
        const auto round=int(staged.league_rounds.at(league.value))+1;
        if(round>rules.rounds())throw FormatError("world12: live round exceeds its season");
        const auto fixtures=league_fixtures(staged.world,league,round);
        if(fixtures.size()!=size_t(group.matches()))throw FormatError("world12: live fixtures do not cover their league round");
        for(size_t match=0;match<fixtures.size();++match){
            const Fixture actual{projection.global_club(uint32_t(group.fixtures.at(match*2))),projection.global_club(uint32_t(group.fixtures.at(match*2+1)))};
            int home=0,away=0;
            if(actual!=fixtures[match]||!match_score(group,int(match),home,away)||home<0||home>15||away<0||away>15)
                throw FormatError("world12: live score identity is unknown or changed");
            auto& score=display.results.result(display.world.catalogue,league,round,int(match));
            if(score.played)throw FormatError("world12: live overview would overwrite a committed round");
            score={true,uint8_t(home),uint8_t(away)};
        }
        display.league_rounds.at(league.value)=uint8_t(round);
    }
    display.validate();return display;
}
LiveFamilyScope::LiveFamilyScope(const Session& source,const CareerSnapshot& staged,AiMatchProjection& projection,
    MatchEngine& engine,uint8_t seat,uint64_t& nonce,uint8_t child,std::optional<LeagueId> league)
    :source_(source),projection_(projection),engine_(engine),seat_(seat),nonce_(nonce),source_revision_(source.revision()),
     game_seed_(engine.game.seed()),watcom_seed_(engine.rand.seed()),game_calls_(engine.game.calls()),watcom_calls_(engine.rand.calls()),
     groups_(engine.groups.data()),group_count_(engine.groups.size()) {
    if(nonce>=UINT64_MAX-2||child>2)throw FormatError("world12: live overview generation exhausted");
    auto display=live_family_snapshot(staged,projection,engine);
    for(const auto& group:engine.groups)if(group.desc[0]==0)live_leagues_.push_back(live_group_league(staged.world,projection,group));
    const auto manager=std::find_if(display.world.managers.begin(),display.world.managers.end(),[&](const auto& m){return m.seat==seat;});
    if(manager==display.world.managers.end()||!manager->career||!display.desktop)
        throw FormatError("world12: live overview has no canonical human viewer");
    display.desktop->active_seat=seat;
    viewer_=std::make_unique<Session>(source);viewer_->state_=std::move(display);viewer_->owner_={source.scope()^(uint64_t(1)<<63),nonce};
    if(!viewer_->owner_.scope)viewer_->owner_.scope=1;
    viewer_->preparation_menu_.reset();viewer_->table_owner_.reset();viewer_->table_generation_=0;
    viewer_->open_table_owned(seat,child);
    if(league&&*league!=viewer_->table_selection_.league){
        const auto& catalogue=viewer_->state_.world.catalogue;std::optional<TableGroup> target;
        for(int group=0;group<3;++group){const auto ids=catalogue.table_leagues(TableGroup(group));
            if(std::find(ids.begin(),ids.end(),*league)!=ids.end()){target=TableGroup(group);break;}
        }
        if(!target)throw FormatError("world12: live overview league is hidden");
        for(int step=0;viewer_->table_selection_.group!=*target&&step<3;++step){
            ui::World12TableRequest change;change.identity=*viewer_->table_owner_;change.action=ui::World12TableAction::next_group;
            viewer_->apply_tables(change,{}, {},{});
        }
        ui::World12TableRequest request;request.identity=*viewer_->table_owner_;request.action=ui::World12TableAction::select_league;request.league=*league;
        viewer_->apply_tables(request,{}, {},{});
    }
    viewer_->table_selection_.display_calendar=staged.calendar;
    select_current_round();
    revision_=viewer_->revision();nonce_=revision_;validate();
}
LiveFamilyScope::~LiveFamilyScope(){if(!closed_){try{abort();}catch(...){}}}
void LiveFamilyScope::validate()const {
    if(closed_||dispatching_||!viewer_||nonce_!=revision_||source_.revision()!=source_revision_||&engine_.state!=&projection_.state||
       engine_.groups.data()!=groups_||engine_.groups.size()!=group_count_||engine_.game.seed()!=game_seed_||engine_.rand.seed()!=watcom_seed_||
       engine_.game.calls()!=game_calls_||engine_.rand.calls()!=watcom_calls_)
        throw FormatError("world12: retired or retimed live overview owner");
}
ui::World12TableState LiveFamilyScope::view()const{
    validate();auto page=viewer_->tables_view();
    if(page.child==0&&std::find(live_leagues_.begin(),live_leagues_.end(),viewer_->table_selection_.league)!=live_leagues_.end()){
        const auto& texts=*viewer_->resources_.texts;
        auto prefix=ui::original_to_utf8(texts[(layout::CareerSession_FormatError_TextPointer2-layout::TextPointerTable)/4]);
        auto suffix=ui::original_to_utf8(texts[(layout::CareerSession_FormatError_TextPointer4-layout::TextPointerTable)/4]);
        while(!prefix.empty()&&prefix.back()==' ')prefix.pop_back();while(!suffix.empty()&&suffix.front()==' ')suffix.erase(suffix.begin());
        page.subtitle=prefix+" "+std::to_string(page.table.rounds_played)+". "+suffix;
    }
    return page;
}
void LiveFamilyScope::select_current_round(){
    auto& selection=viewer_->table_selection_;
    if(selection.child==1&&std::find(live_leagues_.begin(),live_leagues_.end(),selection.league)!=live_leagues_.end()){
        selection.pairings.round=std::max(0,int(viewer_->state_.league_rounds.at(selection.league.value))-1);
        selection.pairings.title_round=selection.pairings.round;
    }
}
bool LiveFamilyScope::apply(const ui::World12TableRequest& request,const std::filesystem::path& data,
    const ClubInformationPresenter& club,const TableRangePresenter& range) {
    validate();if(nonce_>=UINT64_MAX-1)throw FormatError("world12: live overview nonce exhausted");
    dispatching_=true;
    bool closed=false;
    try{closed=viewer_->apply_tables(request,data,club,range);}catch(...){dispatching_=false;throw;}
    dispatching_=false;
    validate();
    // OpenBMH deviates from the original: club-inspection cleanup in a live
    // read-only league overview stays in its display overlay. Only an
    // acknowledged close publishes UI words; lineup edits have their own scope.
    if(closed){
        const auto& managers=viewer_->snapshot().world.managers;
        const auto manager=std::find_if(managers.begin(),managers.end(),[&](const auto& m){return m.seat==seat_;});
        if(manager==managers.end()||!manager->career)throw FormatError("world12: live overview lost its human viewer");
        const auto& career=*manager->career;
        projection_.retain_live_preferences(seat_,career.table_display_word,career.strengths_display_word,career.pairings_display_byte);
        ++nonce_;closed_=true;return true;
    }
    if(request.action==ui::World12TableAction::navigate||request.action==ui::World12TableAction::select_league||
       request.action==ui::World12TableAction::next_group||(request.action==ui::World12TableAction::pairings_control&&
       request.value==int(LeaguePairingsAction::reset)))select_current_round();
    revision_=viewer_->revision();nonce_=revision_;return false;
}
void LiveFamilyScope::abort(){if(closed_)return;validate();if(nonce_!=UINT64_MAX)++nonce_;closed_=true;}
}
