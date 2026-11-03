// SPDX-License-Identifier: GPL-3.0-only
#include "app/world12_club_information.hpp"
#include "formats/executable_layout.hpp"
#include "game/world12/desktop_projection.hpp"
#include "app/career_session.hpp"
#include "ui/club_player_detail.hpp"
#include "game/world12/club_presentation.hpp"
#include "game/world12/standings.hpp"
#include "game/world12/rescheduling.hpp"
#include "ui/team_statistics.hpp"
#include <memory>
#include <algorithm>
namespace bmh::app::world12 {
void inspect_club_information(bmh::world12::CareerSnapshot& candidate,const GameState& reference,
    const LeImage& rules,const TextDb& texts,bmh::world12::ClubId club,
    const ClubInformationPresenter& presenter,const Image& backdrop,std::optional<uint8_t> viewer,const LeImage* english_rules) {
    if(!presenter)throw FormatError("world12: club information requires its visible presenter");
    const auto before=candidate;
    const auto& target=candidate.world.clubs.at(club);
    const bool rostered=target.league.valid();
    auto projection=[&]{
        if(candidate.desktop&&candidate.desktop->active_seat){std::vector<bmh::world12::ClubId> extra;if(rostered)extra.push_back(club);for(const auto& [id,c]:candidate.world.clubs)if(c.league.valid()&&c.presentation_category==2&&std::find(extra.begin(),extra.end(),id)==extra.end())extra.push_back(id);return bmh::world12::project_manager_desktop(candidate,reference,extra);}
        if(!candidate.world.settings)throw FormatError("world12: club information lacks its actual career settings");
        const auto& settings=*candidate.world.settings;
        const bmh::world12::HumanProjectionPolicy policy{settings.mode,settings.difficulty,settings.foreign_rule,true};
        if(target.kind==bmh::world12::ClubKind::national_team)return bmh::world12::AiMatchProjection::create_for_nationals(candidate,{club},reference,policy);
        std::vector<bmh::world12::ClubId> teams;for(const auto& manager:candidate.world.managers)if(candidate.world.clubs.at(manager.club).league.valid())teams.push_back(manager.club);
        if(rostered&&std::find(teams.begin(),teams.end(),club)==teams.end())teams.push_back(club);
        return bmh::world12::AiMatchProjection::create_for_humans(candidate,teams,reference,policy);
    }();
    if(!viewer&&candidate.desktop&&candidate.desktop->active_seat)viewer=*candidate.desktop->active_seat;
    if(!viewer&&candidate.progress)viewer=candidate.progress->last_manager;
    if(viewer)for(size_t n=0;n<projection.human_managers();++n)if(projection.global_manager(uint8_t(n))==*viewer)projection.state.memory.set8(layout::ActiveManager,uint8_t(n));
    if(!rostered&&target.kind!=bmh::world12::ClubKind::national_team)projection.include_club_references(candidate.world,{club});
    struct Context {
        bool active=true,drawn=false,rostered=false;
        int team=-1;
        std::string name;
        bmh::world12::AiMatchProjection projection;
        LeImage rules;std::optional<LeImage> english;TextDb texts;GameRandom game;WatcomRandom random;
        std::optional<uint8_t> statistics_flags;std::optional<bmh::world12::World> presentation;
    };
    auto context=std::make_shared<Context>();context->rostered=rostered;
    context->team=int(projection.local_club_reference(club));context->name=target.name;
    context->projection=std::move(projection);context->rules=rules;context->texts=texts;context->presentation=candidate.world;context->game=GameRandom(candidate.game_seed);context->random=WatcomRandom(candidate.watcom_seed);context->projection.state.texts=&context->texts;if(english_rules)context->english=*english_rules;
    struct Retire {std::shared_ptr<Context> value;~Retire(){value->active=false;}} retire{context};
    ClubInformationProvider provide=[context](ui::ClubInfoView view){
        if(!context->active)throw FormatError("world12: retired club-information presenter");
        if(view!=ui::ClubInfoView::data_a&&view!=ui::ClubInfoView::data_b)throw FormatError("world12: invalid club-information page");
        auto next=context->projection.state;ui::ClubInfoProjection page;
        if(context->rostered){
            const auto entry=prepare_club_info_entry(next,context->team);
            page=ui::club_info_from_game(next,context->team,entry,context->rules,context->texts);
        }else {
            page.runtime_team=context->team;page.original_club_name=context->name;
            page.gradient_text=next.memory.u8(layout::TextGradientDisabled)==0&&context->rules.u8(layout::TextGradientMode)==2;
            page.decorated_background=next.memory.u8(layout::PlainBackground)==0;
        }
        // Nested card/statistics actions and global category ownership are separate
        // adapters. They must not dispatch a bounded drawing ID as a world ID.
        page.player_details_available=context->rostered;page.statistics_available=context->rostered;page.category_available=true;page.category=uint8_t(next.team_morale.at(context->team)>>6);page.view=view;
        context->projection.state=std::move(next);context->drawn=true;return page;
    };
    auto services=std::make_shared<bmh::app::ClubModalServices>();
    const auto guard=[context]{if(!context->active)throw FormatError("world12: retired club-information capability");};
    services->state=[context,guard]() -> GameState& {guard();return context->projection.state;};
    services->runtime_byte=[context,guard](uint32_t at){guard();return context->projection.state.memory.has(at)?context->projection.state.memory.u8(at):context->rules.u8(at);};
    services->set_runtime_byte=[context,guard](uint32_t at,uint8_t value){guard();if(at!=layout::CareerSession_DispatchPoachUi_ExecutableField&&at!=layout::CareerSession_InitializeClubModalRuntime_ExecutableField)throw FormatError("world12: unrelated club modal byte");context->projection.state.memory.add_block(at,1);context->projection.state.memory.set8(at,value);};
    services->change_category=[context,club,guard](int team,int category){guard();if(team!=context->team)throw FormatError("world12: another club category owner");const auto applied=bmh::world12::change_club_category(*context->presentation,club,category);auto& value=context->projection.state.team_morale.at(size_t(team));value=uint8_t((value&63)|(applied<<6));return applied;};
    const auto source=std::make_shared<const bmh::world12::CareerSnapshot>(candidate);const std::shared_ptr<const bmh::world12::World> world(source,&source->world);
    services->redraw_player=[context,world,club,guard](const ClubPlayerDetailSelection& selection){guard();if(selection.runtime_team!=context->team||selection.origin!=ClubPlayerDetailOrigin::squad)throw FormatError("world12: another club/player card owner");const auto id=context->projection.global_player(uint32_t(selection.player_id));if(selection.human_loan_index<0){const auto& roster=world->clubs.at(club).roster;if(std::none_of(roster.begin(),roster.end(),[&](const auto& row){return row.player==id;}))throw FormatError("world12: player card left its actual roster");}else if(std::none_of(world->loans.begin(),world->loans.end(),[&](const auto& loan){return loan.player==id&&loan.owner==club;}))throw FormatError("world12: loan card left its actual owner");return club_player_detail_redraw(context->projection.state,selection,context->rules,context->game,context->random);};
    services->team_statistics=[context,source,world,club,guard](ui::TeamStatisticsMode mode){
        guard();if(!context->rostered)throw FormatError("world12: unrostered club has no league statistics");
        const auto& g=context->projection.state;auto page=ui::team_statistics_from_game(g,context->team,mode,context->rules,context->texts);const auto& c=world->clubs.at(club);const auto& league=world->catalogue.league(c.league);const auto& members=world->membership.at(c.league.value).clubs;
        page.original_team_name=ui::utf8_to_original(c.name);page.original_league_name=ui::utf8_to_original(league.name);page.league=league.local_index;page.league_row=int(std::find(members.begin(),members.end(),club)-members.begin());page.flag_index=uint8_t(c.country);page.source_has_position_history=false;page.league_rounds=bmh::world12::visible_league_round(*source,c.league);
        const auto& policy=c.country==bmh::world12::Country::england&&context->english?*context->english:context->rules;const auto rule_index=league.playable?league.local_index:c.country==bmh::world12::Country::spain?5:c.country==bmh::world12::Country::england?6:c.country==bmh::world12::Country::france?7:8;page.points_mode=policy.u8(layout::CareerSession_BeginFriendlyOpponentTable_RuntimeField+rule_index)&1;
        bmh::world12::TableQuery query;query.three_points=page.points_mode!=0;query.venue=mode==ui::TeamStatisticsMode::home?bmh::world12::TableVenue::home:mode==ui::TeamStatisticsMode::away?bmh::world12::TableVenue::away:bmh::world12::TableVenue::all;
        if(page.source_flags&4)for(const auto& m:world->managers)if(m.match_state&&world->clubs.at(m.club).league==c.league&&(m.match_state->point_deduction>>5)==3)query.point_deductions[m.club]=m.match_state->point_deduction&31;
        page.table={};page.table.league=page.league;page.table.size=league.clubs;
        for(const auto& row:bmh::world12::compute_standings(*world,source->results,c.league,page.league_rounds,query)){const auto at=size_t(std::find(members.begin(),members.end(),row.club)-members.begin());page.table.goals_for[at]=row.goals_for;page.table.goals_against[at]=row.goals_against;page.table.points[at]=int8_t(row.points);page.table.wins[at]=row.wins;page.table.defeats[at]=row.defeats;page.table.rank_word[at]=uint16_t(row.games|((row.previous_rank?row.previous_rank-1:0)<<6)|((row.rank-1)<<11));page.table.form[at]=row.form;}
        const auto& crest=world->crests.at(c.crest);page.badge_atlas=crest.path;page.badge_cell=crest.cell;page.source_badge_fallback=false;return page;
    };
    services->decorate_player_projection=[context,world,guard](ui::ClubPlayerDetailProjection& page){guard();for(auto& club:page.clubs)if(club){const auto id=context->projection.global_club(uint32_t(club->runtime_team));const auto& crest=world->crests.at(world->clubs.at(id).crest);club->badge_atlas=crest.path;club->badge_selector=crest.cell;}page.redraw.context.active_offer=-1;page.redraw.context.national_action_available=false;page.displayed_offer_amount.reset();};
    services->retain_player_statistics_flags=[context,guard](uint8_t value){guard();context->statistics_flags=value;};
    provide.modals=std::move(services);provide.runtime_team=context->team;
    if(!presenter(provide,backdrop))throw FormatError("world12: club information presentation interrupted");
    context->active=false;
    if(!context->drawn)throw FormatError("world12: club information was not presented");
    if(candidate!=before)throw FormatError("world12: club information candidate changed during presentation");
    auto result=candidate;
    if(rostered){
        auto& roster=result.world.clubs.at(club).roster;
        const auto range=squad_range(context->team);
        if(range.count!=roster.size())throw FormatError("world12: club information roster dimensions changed");
        for(size_t row=0;row<roster.size();++row){
            const auto raw=context->projection.state.squad_slots.at(range.first+row);
            if(roster[row].empty()){if(raw!=0xfff)throw FormatError("world12: club information changed an empty row");}
            else {
                if((raw&0xfff)==0xfff||context->projection.global_player(raw&0xfff)!=roster[row].player)throw FormatError("world12: club information changed a player identity");
                roster[row].flags=uint16_t(((raw&0x8000)?1:0)|((raw&0x4000)?2:0)|((raw&0x2000)?4:0)|((raw&0x1000)?8:0));
            }
        }
    }
    const auto& local=context->projection.state;
    for(const auto& [id,c]:context->presentation->clubs)result.world.clubs.at(id).presentation_category=c.presentation_category;
    if(context->statistics_flags){const auto manager=local.memory.u8(layout::ActiveManager);const auto seat=context->projection.global_manager(manager);for(auto& m:result.world.managers)if(m.seat==seat&&m.career)m.career->player_statistics_flags=*context->statistics_flags;}
    result.game_seed=context->game.seed();result.watcom_seed=context->random.seed();
    result.validate();candidate=std::move(result);
}
}
