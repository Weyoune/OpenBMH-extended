// SPDX-License-Identifier: GPL-3.0-only
#include "world12_fixture.hpp"
#include "fixture_tables.hpp"
#include "game/world12/presentation.hpp"
#include "game/world12/codec.hpp"
#include "game/world12/club_presentation.hpp"
#include "game/schedule.hpp"
#include <algorithm>
#include <iostream>
using namespace bmh;using namespace bmh::world12;
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main()try{
    set_fixture_tables(bmh::test::authored_fixture_tables());CareerSnapshot s;s.world=bmh::test::world12::complete_world();s.results=SeasonResults::empty(s.world.catalogue);s.calendar.emplace();s.calendar->schedule.days=366;
    for(const auto& l:s.world.catalogue.leagues)for(uint8_t r=1;r<=l.rounds();++r)s.calendar->schedule.leagues.push_back({uint16_t(r*7),l.id,r});s.calendar->next_day=7;s.date=7;s.validate();const auto before=s;const auto ticket=*prepare_day(s);const auto labels=matchday_competition_labels(s,ticket);
    check(std::count(labels.begin(),labels.end(),"")==1,"country separator missing or repeated");const auto gap=std::find(labels.begin(),labels.end(),"");check(gap!=labels.begin()&&std::next(gap)!=labels.end()&&*std::next(gap)=="Premier League","English block does not follow the separator");check(std::find(labels.begin(),labels.end(),"National League")==labels.end(),"hidden fifth English league leaked into pre-match screen");check(s==before,"pre-match labels changed career or RNG");
    auto world=s.world;const auto club=world.membership[5].clubs[0];world.clubs.at(club).presentation_category=1;const auto bytes=encode_world(world);check(decode_world(bytes)==world,"radio choice lost in saved world");auto malformed=world;malformed.clubs.at(club).presentation_category=3;bool refused=false;try{malformed.validate();}catch(...){refused=true;}check(refused,"invalid presentation category accepted");
    auto old=cli::parse_json(std::string(bytes.begin(),bytes.end()));old.object()->at("version")=43;const auto text=cli::dump_json(old);refused=false;try{decode_world(Bytes(text.begin(),text.end()));}catch(...){refused=true;}check(refused,"old schema silently discarded radio choice");
    auto choices=s.world;for(uint8_t seat=0;seat<4;++seat)choices.managers.push_back({seat,"Viewer "+std::to_string(seat),choices.membership[0].clubs[seat],{0},seat});const auto extra=choices.membership[5].clubs[0];const auto morale=choices.clubs.at(extra).morale;check(change_club_category(choices,extra,2)==0,"fifth watched game bypassed Original limit");check(change_club_category(choices,choices.managers[0].club,1)==1&&change_club_category(choices,extra,2)==2,"radio option did not free a watched slot");check(choices.clubs.at(extra).morale==morale&&decode_world(encode_world(choices))==choices,"presentation choice changed morale or failed persistence");
    const auto rules=LeImage::from_bytes(layout::GermanDataSegmentBegin,Bytes(0x6000));check(newly_completed_titles(s,s,rules,rules).empty(),"unchanged fixtures manufactured a title");
    std::cout<<"Pre-match country grouping/separator/hidden league, immutable projection, radio save/old-schema rejection and no synthetic title passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
