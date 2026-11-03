// SPDX-License-Identifier: GPL-3.0-only
#include "app/world12_live_scope.hpp"
#include "app/live_substitution_source.hpp"
#include "app/world12_pitch_source.hpp"
#include "app/preparation_actions.hpp"
#include "app/career_session.hpp"
#include "ui/club_player_detail.hpp"
#include "game/match.hpp"
#include "game/lineup.hpp"
#include "game/weekly.hpp"
#include "game/national_identity.hpp"
#include <algorithm>
namespace bmh::app::world12 {
namespace {
LiveEditOutcome failure(const std::string& text){
    constexpr std::string_view prefix="source_notice:";
    if(text.starts_with(prefix)){try{return {false,uint32_t(std::stoul(text.substr(prefix.size()),nullptr,16)),{}};}catch(...){}}
    return {false,0,text};
}
}
LiveScope::LiveScope(bmh::world12::AiMatchProjection& p,MatchEngine& e,const LeImage& rules,int minute,int team,
    int mode,uint64_t& nonce,const std::optional<LiveMatchEvent>& event)
    :projection_(p),engine_(e),rules_(rules),minute_(minute),team_(team),manager_(p.state.manager_of(team)),mode_(mode),nonce_(nonce),
     event_(event),before_(p.state),game_before_(e.game),random_before_(e.rand),groups_before_(e.groups),group_storage_(e.groups.data()) {
    if(&e.state!=&p.state||nonce_==UINT64_MAX||manager_<0||size_t(manager_)>=p.human_managers()||
       (mode_!=2&&mode_!=8)||minute_<0||minute_>125||(!event_&&minute_<=2)||(event_&&mode_!=2))throw FormatError("world12: live menu has no actual human minute owner");
    for(size_t g=0;g<e.groups.size();++g)for(int m=0;m<e.groups[g].matches();++m){const auto& group=e.groups[g];
        if((group.kickoff_void_mask>>m&1)||(group.void_mask()>>m&1))continue;
        const int home=group.fixtures.at(size_t(m)*2),away=group.fixtures.at(size_t(m)*2+1);
        if(std::find(p.state.live_matches.begin(),p.state.live_matches.end(),home)==p.state.live_matches.end())continue;
        if(team==home||team==away)opponent_=team==home?away:home;
        if(event_&&event_->group_index==g&&event_->match_index==m){
            if(event_->team!=team||event_->opponent!=opponent_||event_->manager_slot!=manager_||event_->minute!=minute_||
               event_->side<0||event_->side>1||group.fixtures.at(size_t(m)*2+size_t(event_->side))!=team)
                throw FormatError("world12: live event identity differs from its fixture");
        }
    }
    if(opponent_<0)throw FormatError("world12: live menu club is not in a watched fixture");
    if(event_){
        if(event_->group_index>=e.groups.size()||event_->match_index<0||event_->match_index>=e.groups[event_->group_index].matches()||
           event_->player<0||size_t(event_->player)>=p.state.players.size()||(event_->kind!=1&&event_->kind!=2))throw FormatError("world12: invalid live card/injury boundary");
        const auto& player=p.state.players.at(size_t(event_->player));bool found=false;
        const auto range=squad_range(p.state,team);for(size_t row=0;row<range.count;++row){const auto slot=p.state.squad_slots.at(range.first+row);if((slot&0xfff)!=event_->player||(slot&0x8000))continue;
            found=event_->kind==1?(player.w368&0x07000000u)!=0:(player.w35c>>27)!=0;}
        if(!found)throw FormatError("world12: live event has no changed Source player");
    }
    auto entered=p.state;if(mode_==2)prepare_lineup_entry(entered,team_);entered.memory.set8(layout::ActiveManager,uint8_t(manager_));
    p.state=std::move(entered);revision_=++nonce_;
}
LiveScope::~LiveScope(){*active_=false;if(!closed_){try{abort();}catch(...){}}}
void LiveScope::validate()const{
    if(closed_||nonce_!=revision_||&engine_.state!=&projection_.state||engine_.groups.data()!=group_storage_||engine_.groups.size()!=groups_before_.size())throw FormatError("world12: retired or changed live menu owner");
    for(size_t i=0;i<groups_before_.size();++i)if(engine_.groups[i].desc!=groups_before_[i].desc||engine_.groups[i].fixtures!=groups_before_[i].fixtures)throw FormatError("world12: live menu changed a retained fixture");
    if(projection_.state.manager_of(team_)!=manager_||projection_.state.memory.u8(layout::ActiveManager)!=manager_)throw FormatError("world12: live menu manager changed");
}
void LiveScope::validate_actor(int manager,int team,uint64_t revision)const{validate();if(manager!=manager_||team!=team_||revision!=revision_||nonce_==UINT64_MAX)throw FormatError("world12: stale live row/action");}
ui::LineupState LiveScope::lineup()const{validate();auto out=ui::lineup_from_game(projection_.state,rules_,revision_);out.in_match=true;out.entry_prepared=true;return out;}
ui::PitchExtras LiveScope::pitch()const{validate();return ui::pitch_extras_from_game(projection_.state,rules_,revision_,opponent_);}
ui::TacticsState LiveScope::tactics()const{validate();return ui::tactics_from_game(projection_.state,rules_,revision_);}
bmh::app::ClubModalServices LiveScope::player_services(const bmh::world12::World& world){
    validate();bmh::app::ClubModalServices services;const auto active=active_;
    const auto guard=[this,active]{if(!*active)throw FormatError("world12: retired live player-card capability");validate();};
    services.state=[this,guard]() -> GameState& {guard();return projection_.state;};
    services.runtime_byte=[this,guard](uint32_t at){guard();return projection_.state.memory.has(at)?projection_.state.memory.u8(at):rules_.u8(at);};
    services.set_runtime_byte=[this,guard](uint32_t at,uint8_t value){
        guard();if(at!=layout::CareerSession_DispatchPoachUi_ExecutableField&&at!=layout::CareerSession_InitializeClubModalRuntime_ExecutableField)
            throw FormatError("world12: live player card requested another runtime byte");
        projection_.state.memory.add_block(at,1);projection_.state.memory.set8(at,value);
    };
    services.redraw_player=[this,guard](const ClubPlayerDetailSelection& selection){
        guard();if(selection.runtime_team!=team_||selection.origin!=ClubPlayerDetailOrigin::squad||selection.human_loan_index!=-1)
            throw FormatError("world12: live card has another squad owner");
        auto game=engine_.game;auto random=engine_.rand;
        auto redraw=club_player_detail_redraw(projection_.state,selection,rules_,game,random);
        engine_.game=game;engine_.rand=random;return redraw;
    };
    services.decorate_player_projection=[this,guard,&world](ui::ClubPlayerDetailProjection& page){
        guard();for(auto& club:page.clubs)if(club){const auto id=projection_.global_club(uint32_t(club->runtime_team));
            const auto& crest=world.crests.at(world.clubs.at(id).crest);club->badge_atlas=crest.path;club->badge_selector=crest.cell;
        }
        const auto& player=world.players.at(projection_.global_player(uint32_t(page.redraw.context.selection.player_id)));
        const auto country=uint8_t(player.attributes.nationality()+((player.bonus&0x80)?32:0));
        const auto nation=std::find_if(world.clubs.begin(),world.clubs.end(),[&](const auto& pair){return pair.second.kind==bmh::world12::ClubKind::national_team&&uint8_t(pair.second.country)==country;});
        if(nation!=world.clubs.end())page.original_nationality_name=nation->second.name;
        // A running game admits no transfer or nomination transaction. This
        // card displays the actual current squad's board and Source redraw.
        page.redraw.context.active_offer=-1;page.redraw.context.national_action_available=false;page.displayed_offer_amount.reset();
    };
    return services;
}
LiveEditOutcome LiveScope::apply(const ui::LineupRequest& request){
    try{validate_actor(request.manager_slot,request.runtime_club,request.revision);if(mode_!=2)throw FormatError("world12: lineup action outside Source lineup scope");
        const auto& g=projection_.state;const bool foreign=g.projected_foreign_rule.value_or(rules_.u8(layout::ForeignPlayerLimitEnabled)!=0);
        std::string error;if(!apply_live_substitution_source(projection_.state,request,minute_,rules_.u8(layout::CareerSession_DispatchLiveSubstitution_ExecutableField),foreign,match_foreign_player_limit(g,team_),[&](int player){return nationality(g,player)!=match_team_region(g,team_);},error))return failure(error);
        revision_=++nonce_;return {true,0,{}};
    }catch(const std::exception& error){return failure(error.what());}
}
LiveEditOutcome LiveScope::apply(const ui::PitchRequest& request){
    if(request.kind==ui::PitchRequestKind::swap_players){ui::LineupRequest row;row.kind=ui::LineupRequestKind::swap_players;row.manager_slot=request.manager_slot;row.runtime_club=request.runtime_club;row.revision=request.revision;row.first_row=request.row;row.first_player=request.player_id;row.first_slot=request.slot;row.second_row=request.second_row;row.second_player=request.second_player;row.second_slot=request.second_slot;return apply(row);}
    try{validate_actor(request.manager_slot,request.runtime_club,request.revision);if(mode_!=2||request.kind==ui::PitchRequestKind::set_automatic_weight||request.kind==ui::PitchRequestKind::apply_formation||request.kind==ui::PitchRequestKind::save_formation)throw FormatError("world12: formation automation is not a live Source control");
        auto candidate=projection_.state;apply_world12_pitch_action(candidate,request,candidate.projected_foreign_rule.value_or(false));projection_.state=std::move(candidate);revision_=++nonce_;return {true,0,{}};
    }catch(const std::exception& error){return failure(error.what());}
}
LiveEditOutcome LiveScope::apply(const ui::TacticsRequest& request){
    try{validate_actor(request.manager_slot,request.runtime_club,request.revision);if(mode_!=8||minute_<=2)throw FormatError("world12: tactic action outside its live Source scope");
        auto candidate=projection_.state;auto revision=revision_;std::string error;if(!apply_tactics_request_source(candidate,rules_,request,revision,error))return failure(error);
        projection_.state=std::move(candidate);revision_=++nonce_;return {true,0,{}};
    }catch(const std::exception& error){return failure(error.what());}
}
void LiveScope::finish(){validate();if(nonce_==UINT64_MAX)throw FormatError("world12: live nonce exhausted");
    if(!event_){MinuteReport report;engine_.rerate_team(team_,report);if(report.unsupported){abort();throw FormatError(report.reason);}}
    // EventImpl owns the event-origin rerate at its existing tail.
    // The Source menu leaves its selected manager active; abort alone restores
    // the previous actor. EventImpl retains its own post-menu rerate boundary.
    ++nonce_;closed_=true;*active_=false;
}
void LiveScope::abort(){if(closed_)return;
    if(engine_.groups.data()!=group_storage_||engine_.groups.size()!=groups_before_.size())throw FormatError("world12: live group storage was invalidated");
    projection_.state=std::move(before_);for(size_t i=0;i<groups_before_.size();++i)engine_.groups[i]=std::move(groups_before_[i]);engine_.game=game_before_;engine_.rand=random_before_;if(nonce_!=UINT64_MAX)++nonce_;closed_=true;*active_=false;
}
}
