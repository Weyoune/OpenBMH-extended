// SPDX-License-Identifier: GPL-3.0-only
#include "ui/world12_table.hpp"
#include "game/world12/rescheduling.hpp"
#include "formats/executable_layout.hpp"
#include "ui/setup.hpp"
#include "ui/manager.hpp"
#include <algorithm>
namespace bmh::ui {
using namespace bmh::world12;
namespace {
uint8_t rank_color(const Catalogue& catalogue,const League& league,int rank){
    if(league.country==Country::germany){
        constexpr int promoted[]={0,3,2,2,4};uint8_t color=rank<promoted[league.local_index]?0x51:0x37;
        if(int(league.clubs)-rank<=league.relegation)color=0xf7;
        if(league.local_index==0){if(rank==0)color=0x27;else if(rank<5)color=0x7a;}
        if((league.local_index==2||league.local_index==3)&&(rank==1||rank==2))color=0x27;
        return color;
    }
    uint8_t color=0x37;
    if(league.playable&&league.local_index>0){const auto& ladder=catalogue.rules(league.country).ladder;const auto promoted=ladder.at(league.local_index-1).relegation;if(rank<promoted)color=0x51;}
    if(int(league.clubs)-rank<=league.relegation)color=0xf7;
    if(league.playable&&league.local_index==0){if(rank==0)color=0x27;else if(rank<5)color=0x7a;}
    return color;
}
}
void normalize_world12_table_range(World12TableSelection& s,const CareerSnapshot& snapshot){
    const auto& league=snapshot.world.catalogue.league(s.league);const int played=visible_league_round(snapshot,s.league);
    if(s.first_round<0||s.first_round>=league.rounds())throw FormatError("world12: invalid first table round");
    if(s.end_round>league.rounds()||s.end_round<=s.first_round||s.end_round>played)s.end_round=played;
    if(s.end_round<=s.first_round)s.end_round=s.first_round+1;
}
World12TableSelection initial_world12_table_selection(const CareerSnapshot& snapshot,uint8_t seat,uint16_t word,uint8_t normal){
    const auto manager=std::find_if(snapshot.world.managers.begin(),snapshot.world.managers.end(),[&](const auto& m){return m.seat==seat;});
    if(manager==snapshot.world.managers.end()||normal>1)throw FormatError("world12: no table manager/default points mode");
    World12TableSelection out;out.league=snapshot.world.clubs.at(manager->club).league;
    out.group=snapshot.world.clubs.at(manager->club).country==Country::germany?TableGroup::germany:TableGroup::england;
    out.word=uint16_t(word&0xff7f);out.normal_mode=normal;out.aggregation=4;
    if((word&4)==0)out.aggregation|=1;if((word&2)==0)out.aggregation|=2;
    out.end_round=visible_league_round(snapshot,out.league);normalize_world12_table_range(out,snapshot);return out;
}
void apply_world12_table_control(World12TableSelection& s,int control,const Catalogue& catalogue){
    switch(control){
    case 0:s.aggregation=7;s.word=uint16_t((s.word&0xff0)|1);break;
    case 1:s.aggregation=1;s.word=uint16_t((s.word&0xff0)|2);break;
    case 2:s.aggregation=2;s.word=uint16_t((s.word&0xff0)|4);break;
    case 4:case 5:case 6:s.word^=uint16_t(0x10u<<(control-4));if((s.word&0x70)==0)s.word|=0x70;break;
    case 7:s.normal_mode^=1;break;
    case 8:s.first_round=0;s.end_round=catalogue.league(s.league).rounds();s.word=uint16_t((s.word&0xcff)|0x100);break;
    default:throw FormatError("world12: table control requires a separate presenter");
    }
    s.word&=0xf7f;
}
World12TableState world12_table_state(const CareerSnapshot& snapshot,const World12TableIdentity& owner,
    const World12TableSelection& selection,const LeImage& rules,const TextDb& texts){
    snapshot.validate();const auto& world=snapshot.world;const auto& catalogue=world.catalogue;
    const auto manager=std::find_if(world.managers.begin(),world.managers.end(),[&](const auto& m){return m.seat==owner.manager;});
    if(!owner.scope||manager==world.managers.end()||manager->club!=owner.club||uint8_t(selection.group)>2||selection.normal_mode>1||
       selection.first_round<0||selection.end_round<=selection.first_round||selection.end_round>catalogue.league(selection.league).rounds()||
       (selection.aggregation<1||selection.aggregation>7))throw FormatError("world12: invalid table owner/query");
    World12TableState out;out.identity=owner;out.group=selection.group;out.leagues=catalogue.table_leagues(selection.group);
    if(std::find(out.leagues.begin(),out.leagues.end(),selection.league)==out.leagues.end())throw FormatError("world12: table league is outside its country group");
    for(size_t group=0;group<3;++group)out.groups[group]=!catalogue.table_leagues(TableGroup(group)).empty();
    const auto& league=catalogue.league(selection.league);auto& table=out.table;
    table.manager_slot=owner.manager;table.league_id=int(selection.league.value);table.league_name=league.name;
    table.header_text=original_to_utf8(texts[(layout::LeagueTable_OriginalToUtf8_TextPointer2-layout::TextPointerTable)/4]);
    table.rounds_played=visible_league_round(snapshot,selection.league);table.raw_round_byte=uint8_t(table.rounds_played);
    table.display_word=selection.word;table.source_controls_projection=true;table.source_aggregation_flags=selection.aggregation;
    table.source_normal_mode=selection.normal_mode;table.points_mode=selection.normal_mode+14-int((selection.word>>3)&14);
    table.source_show_previous_rank=false; // Supplied by the owning runtime projection.
    (void)rules;
    table.secondary_leagues=true;
    const auto raw=texts.raw(82,9);table.retained_range_label_source_bytes.assign(raw.begin(),raw.end());
    const auto put_number=[&](size_t at,int value){table.retained_range_label_source_bytes.at(at)=char('0'+value/10);table.retained_range_label_source_bytes.at(at+1)=char('0'+value%10);};
    put_number(4,selection.first_round+1);put_number(7,selection.end_round);
    TableQuery query;query.first_round=selection.first_round+1;query.last_round=selection.end_round;
    query.three_points=selection.normal_mode!=0;query.venue=(selection.aggregation&3)==1?TableVenue::home:(selection.aggregation&3)==2?TableVenue::away:TableVenue::all;
    query.suppressed_parts=uint8_t(7^((selection.word>>4)&7));
    if(selection.aggregation&4)for(const auto& m:world.managers)if(m.match_state&&world.clubs.at(m.club).league==selection.league&&m.match_state->point_deduction>>5==3)query.point_deductions[m.club]=m.match_state->point_deduction&31;
    for(const auto& row:compute_standings(world,snapshot.results,selection.league,table.rounds_played,query)){
        LeagueTableRow value;value.rank=row.rank;value.previous_rank=row.previous_rank;value.name=original_to_utf8(world.clubs.at(row.club).name);
        value.runtime_team_id=int(table.rows.size());value.club_identity_key=int(table.rows.size());value.name_color=0x5f;
        for(const auto& m:world.managers)if(m.club==row.club)value.name_color=m.seat==owner.manager?0x7a:0x75;
        value.rank_color=rank_color(catalogue,league,row.rank-1);value.games=row.games;value.wins=row.wins;value.draws=row.draws;value.losses=row.defeats;
        value.goals_for=row.goals_for;value.goals_against=row.goals_against;value.points=row.points;value.form_bits=row.form;
        out.clubs.push_back(row.club);table.rows.push_back(std::move(value));
    }
    return out;
}
}
