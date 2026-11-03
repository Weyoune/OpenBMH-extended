// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "world12_fixture.hpp"
#include "game/world12/indoor.hpp"
#include "game/world12/manager_tactics.hpp"
#include "game/world12/economy.hpp"
#include "game/career.hpp"
#include "formats/data_profile.hpp"
namespace bmh::test::world12 {
using namespace bmh;using namespace bmh::world12;
inline LeImage annual_indoor_rules(){auto result=LeImage::from_bytes(layout::GermanDataSegmentBegin,Bytes(0x21000));const auto compact=bmh::test::world12::country_rules(false);for(uint32_t i=0;i<compact.size();++i){const auto value=compact.u8(compact.base()+i);result.patch(compact.base()+i,&value,1);}const auto profile=DataProfile::international_11();for(size_t i=0;i<9;++i)result.patch(layout::LeagueSizes+uint32_t(i),&profile.leagues[i],1);return result;}
inline GameState annual_indoor_reference(const LeImage& source){Bytes forms(0x160);const uint8_t outdoor_x[]={0,5,5,5,5,10,10,10,10,17,17},outdoor_y[]={4,1,3,5,7,1,3,5,7,3,5};const uint8_t indoor_x[]={0,5,5,5,5,11,11,11,11,17,18},indoor_y[]={4,2,3,6,7,2,3,6,7,4,4};for(size_t f=0;f<8;++f)for(size_t row=0;row<11;++row){forms[f*22+row]=outdoor_x[row];forms[f*22+11+row]=outdoor_y[row];forms[0xb0+f*22+row]=indoor_x[row];forms[0xb0+f*22+11+row]=indoor_y[row];}return startup_reference_state(source,DataProfile::international_11().counts,Bytes(0x360),forms);}
inline CareerSnapshot annual_indoor_fixture(const GameState& reference,const LeImage& source,bool human){CareerSnapshot s;auto& w=s.world;w=bmh::test::world12::complete_world();w.catalogue.scope.international=false;w.players_initialized=true;w.markets.emplace();w.free_players.emplace();w.settings=CareerSettings{3,6,5,1,false};w.economy.emplace();w.economy->presentation_options.emplace();w.economy->presentation_options->fill(1);(*w.economy->presentation_options)[16*9+4]=0;uint32_t next=2000000;
    for(auto& [id,club]:w.clubs)for(size_t row=0;row<24;++row){if(club.roster[row].empty()){PlayerRecord p;p.id={next++};p.attributes.name="Fixture reserve";w.players[p.id]=p;club.roster[row]={p.id,uint16_t(row<17?2:0)};}auto& p=w.players.at(club.roster[row].player).attributes;p.w35c=60u|(80u<<7)|(85u<<14);p.w368=uint32_t(club.country)<<27;p.w364=uint32_t(row==0||row==11?0:row<5?1:row<9?2:3);}
    if(human){const auto club=w.membership[0].clubs[0];w.managers={{3,"Hall manager",club,{0},3}};WatcomRandom random(713);initialize_manager_contracts(w,3,reference,{3,false,false,false,6},random);w.managers[0].career.emplace();w.managers[0].career->season_records.emplace();w.managers[0].career->cup_titles.emplace();w.economy->managers[3].monthly.emplace();w.economy->managers[3].depreciation=0;}
    InitialCompetitionFields fields;auto club=w.clubs.begin();const size_t counts[]={64,24,32};for(size_t k=0;k<3;++k){for(size_t i=0;i<counts[k];++i)fields.fields[k].push_back((club++)->first);fields.holders[k]=fields.fields[k][0];}fields.champions={{Country::germany,w.membership[0].clubs[0]},{Country::england,w.membership[5].clubs[0]}};fields.domestic_holders={{Country::germany,w.membership[0].clubs[7]},{Country::england,w.membership[5].clubs[7]}};w.initial_competitions=fields;w.indoor=initial_indoor_season(w,source,reference);s.results=SeasonResults::empty(w.catalogue);s.game_seed=31;s.watcom_seed=713;s.validate();return s;}
}
