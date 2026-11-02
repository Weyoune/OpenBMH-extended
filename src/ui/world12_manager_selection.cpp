// SPDX-License-Identifier: GPL-3.0-only
#include "ui/world12_manager_selection.hpp"
#include <stdexcept>
namespace bmh::ui {
SelectionContext world12_manager_selection_context(const bmh::world12::World& world,uint64_t revision) {
    using namespace bmh::world12;world.validate();
    if(!world.catalogue.scope.club_play())throw std::invalid_argument("world12: national-only manager selection needs its national owner");
    auto view=std::make_shared<ManagerWorldSelection>();view->scope=world.catalogue.scope;view->revision=revision;
    for(auto country:{Country::germany,Country::england}) {
        if(!world.catalogue.selectable(country))continue;
        auto& groups=country==Country::germany?view->germany:view->england;
        const auto add=[&](size_t group,const ClubRecord& club){const auto& crest=world.crests.at(club.crest);groups[group].clubs.push_back({club.id,club.name,crest.path,crest.cell});};
        for(const auto id:world.catalogue.player_leagues(country)){const auto local=world.catalogue.league(id).local_index;groups[local].label=world.catalogue.league(id).name;
            for(auto club:world.membership[id.value].clubs)add(local,world.clubs.at(club));}
        groups[5].label="Zusatzteams A";groups[6].label="Zusatzteams B";size_t extra=0;
        for(const auto& [id,club]:world.clubs)if(club.kind==ClubKind::club&&club.country==country&&!club.league.valid()) {
            if(extra>=40)throw std::invalid_argument("world12: additional country teams exceed Original picker pages");add(extra<20?5:6,club);++extra;}
    }
    SelectionContext result;result.revision=revision;result.source="world12";result.world=std::move(view);return result;
}
}
