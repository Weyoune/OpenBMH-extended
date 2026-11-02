// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/world.hpp"
#include "game/world12/cup_draw.hpp"
#include "game/cup.hpp"
#include "formats/le.hpp"
#include "cli/json.hpp"

namespace bmh::test::world12 {
// Older-schema fixtures must actually omit fields their writer did not know.
inline void omit_recent_manager_records(bmh::cli::Json& root) {
    root.object()->erase("amateur_slots");
    root.object()->erase("european_campaigns");
    auto& managers=std::get<bmh::cli::Json::Array>(root.object()->at("managers").value);
    for(auto& manager:managers) {
        auto& match=manager.object()->at("match_state");
        if(match.object())match.object()->erase("attendance");
        auto& career=manager.object()->at("career");if(career.object()){const auto services=career.object()->find("season_services");if(services!=career.object()->end()&&services->second.object())services->second.object()->erase("promotion_tail_flags");}if(career.object()){career.object()->erase("cup_titles");career.object()->erase("strengths_display_word");career.object()->erase("pairings_display_byte");career.object()->erase("table_display_word");career.object()->erase("other_games_view");career.object()->erase("camp_focus");career.object()->erase("stadium_curve_flags");}
    }
    const auto economy=root.object()->find("economy");
    if(economy!=root.object()->end()&&economy->second.object())economy->second.object()->erase("presentation_options");
    if(economy!=root.object()->end()&&economy->second.object())
        for(auto& manager:std::get<bmh::cli::Json::Array>(economy->second.object()->at("managers").value))manager.object()->erase("depreciation");
}
inline void omit_recent_manager_records(bmh::world12::World& world) {
    world.amateur_slots.reset();
    for(auto& manager:world.managers)if(manager.career&&manager.career->season_services)manager.career->season_services->promotion_tail_flags.reset();
    world.european_campaigns.reset();
    for(auto& manager:world.managers)if(manager.match_state)manager.match_state->attendance.reset();
    for(auto& manager:world.managers)if(manager.career){manager.career->cup_titles.reset();manager.career->strengths_display_word.reset();manager.career->pairings_display_byte.reset();manager.career->table_display_word.reset();manager.career->other_games_view.reset();manager.career->camp_focus.reset();manager.career->stadium_curve_flags.reset();}
    if(world.economy)world.economy->presentation_options.reset();
    if(world.economy)for(auto& manager:world.economy->managers)manager.depreciation.reset();
}
inline bmh::world12::World complete_world() {
    using namespace bmh::world12;
    World world;uint32_t club_id=100000,player_id=200000;
    for(const auto& league:world.catalogue.leagues) {
        auto& membership=world.membership[league.id.value];membership.league=league.id;
        for(int c=0;c<league.clubs;++c) {
            ClubRecord club;club.id={club_id++};club.name="Club "+std::to_string(club.id.value);
            club.country=league.country;club.league=league.id;club.crest={club.id.value};
            world.crests[club.crest]={"W/TEST.VGA",0};
            for(int p=0;p<11;++p) {
                PlayerRecord player;player.id={player_id++};player.attributes={"Player "+std::to_string(player.id.value),50u|(60u<<7)|(65u<<14),0,0};
                world.players[player.id]=player;club.roster[size_t(p)]={player.id,1};
            }
            membership.clubs.push_back(club.id);world.clubs[club.id]=club;
        }
    }
    world.validate();return world;
}
inline bmh::world12::CountryCupRanking membership_ranking(const bmh::world12::World& world,bmh::world12::Country country) {
    bmh::world12::CountryCupRanking ranking;
    for(uint8_t local=0;local<5;++local)ranking[local]=world.membership.at(world.catalogue.starting_league(country,local).value).clubs;
    return ranking;
}
inline LeImage country_rules(bool english) {
    auto image=LeImage::from_bytes(layout::LeagueSizes,Bytes(0x1000));
    const uint8_t size=english?20:18;image.patch(layout::LeagueSizes,&size,1);
    const auto stage=[&](uint8_t entry,uint8_t type,uint8_t teams,uint8_t rounds,uint8_t flags,uint8_t next,uint8_t seeds){
        const auto at=cup_entry_address(entry);
        const uint8_t previous=entry==28?0:entry==26?25:entry==27?26:255;
        image.patch(at+0x15,&previous,1);
        image.patch(at+0x14,&flags,1);image.patch(at+0x16,&next,1);image.patch(at+0x18,&type,1);
        image.patch(at+0x1a,&teams,1);image.patch(at+0x1b,&rounds,1);image.patch(at+0x20,&seeds,1);
    };
    stage(23,8,2,1,12,0,0);
    if(!english)stage(0,0,64,6,46,0,0);
    else {stage(0,0,48,2,46,28,20);stage(28,0,32,5,46,0,0);
        stage(25,11,24,1,46,26,20);stage(26,11,32,4,63,27,0);stage(27,11,2,1,44,0,0);}
    const auto european=[&](uint8_t entry,uint8_t type,uint8_t teams,uint8_t rounds,uint8_t flags,uint8_t next,uint8_t previous=255,uint8_t parallel=0,uint8_t groups=1,uint8_t group=0,uint8_t quota=1,uint8_t seeds=0){
        stage(entry,type,teams,rounds,flags,next,seeds);const auto at=cup_entry_address(entry);image.patch(at+0x15,&previous,1);image.patch(at+0x17,&parallel,1);const uint8_t packed=uint8_t(groups|(group<<4));image.patch(at+0x1e,&packed,1);image.patch(at+0x1f,&quota,1);
    };
    european(15,1,64,6,191,0);
    european(16,2,16,1,191,31,255,0,1,0,8,8);
    european(18,2,8,2,191,19,31,0,1,0,2);
    european(19,2,2,1,140,0,18);
    european(20,3,32,4,191,21,255,0,1,0,2);
    european(21,3,2,1,140,0,20);
    for(uint8_t group=0;group<4;++group)european(uint8_t(31+group),2,4,6,227,18,16,group==3?0:uint8_t(32+group),4,group,8);
    return image;
}
}
