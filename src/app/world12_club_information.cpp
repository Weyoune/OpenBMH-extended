// SPDX-License-Identifier: GPL-3.0-only
#include "app/world12_club_information.hpp"
#include "formats/executable_layout.hpp"
#include "game/world12/desktop_projection.hpp"
#include <memory>
namespace bmh::app::world12 {
void inspect_club_information(bmh::world12::CareerSnapshot& candidate,const GameState& reference,
    const LeImage& rules,const TextDb& texts,bmh::world12::ClubId club,
    const ClubInformationPresenter& presenter,const Image& backdrop) {
    if(!presenter)throw FormatError("world12: club information requires its visible presenter");
    const auto before=candidate;
    const auto& target=candidate.world.clubs.at(club);
    const bool rostered=target.league.valid();
    auto projection=bmh::world12::project_manager_desktop(candidate,reference,rostered?std::vector{club}:std::vector<bmh::world12::ClubId>{});
    if(!rostered)projection.include_club_references(candidate.world,{club});
    struct Context {
        bool active=true,drawn=false,rostered=false;
        int team=-1;
        std::string name;
        bmh::world12::AiMatchProjection projection;
        LeImage rules;TextDb texts;
    };
    auto context=std::make_shared<Context>();context->rostered=rostered;
    context->team=int(projection.local_club_reference(club));context->name=target.name;
    context->projection=std::move(projection);context->rules=rules;context->texts=texts;
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
        page.player_details_available=false;page.statistics_available=false;page.category_available=false;page.view=view;
        context->projection.state=std::move(next);context->drawn=true;return page;
    };
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
    result.validate();candidate=std::move(result);
}
}
