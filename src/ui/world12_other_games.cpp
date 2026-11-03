// SPDX-License-Identifier: GPL-3.0-only
#include "ui/world12_other_games.hpp"
#include "ui/manager.hpp"
#include "game/world12/group_table.hpp"
#include "game/cup.hpp"
#include "game/matchday.hpp"
#include "game/driver.hpp"
#include "game/schedule.hpp"
#include <algorithm>
#include <stdexcept>
#include <set>

namespace bmh::ui {
namespace {
using namespace bmh::world12;
constexpr const char* labels[]={"DFB-POKAL","UEFA-CUP","CHAMP. LEAGUE","POKALSIEGER","WM/EM-QUALI.","SUPER-CUP","FA CUP","KOKA KOLA CUP"};
bool enabled(const World& world,int page) {
    switch(World12OtherPage(page)) {
    case World12OtherPage::german_cup:return world.catalogue.scope.germany;
    case World12OtherPage::fa_cup:case World12OtherPage::league_cup:return world.catalogue.scope.england;
    case World12OtherPage::national_qualifiers:return world.catalogue.scope.international;
    default:return page>=0&&page<8&&world.catalogue.scope.club_play();
    }
}
void validate_owner(const World& world,const World12OtherGamesIdentity& owner) {
    if(!owner.scope || owner.manager_slot>3 || !owner.club.valid())throw FormatError("other games: invalid typed owner");
    const auto manager=std::find_if(world.managers.begin(),world.managers.end(),[&](const auto& m){return m.seat==owner.manager_slot;});
    const bool club=manager!=world.managers.end()&&manager->club==owner.club;
    const bool national=world.national_coaches&&std::any_of(world.national_coaches->begin(),world.national_coaches->end(),[&](const auto& record){return record.active&&record.coach==owner.manager_slot&&record.team==owner.club;});
    if(!club&&!national)throw FormatError("other games: stale typed manager owner");
}
std::vector<const KnockoutStage*> stages(const CareerSnapshot& snapshot,DomesticCup cup) {
    std::vector<const KnockoutStage*> out;
    for(const auto& stage:snapshot.cups)if(stage.cup==cup)out.push_back(&stage);
    return out;
}
void add_fixture(OtherGamesState& state,const World& world,const bmh::world12::Fixture& fixture,
    std::optional<uint16_t> score,std::optional<uint16_t> other,int source_match,int baseline=-1) {
    OtherGamesRow row;row.kind=OtherGamesRowKind::match;
    const int token=int(state.projection.rows.size())*2;
    row.home_team=token;row.away_team=token+1;row.home_name=utf8_to_original(world.clubs.at(fixture.home).name);row.away_name=utf8_to_original(world.clubs.at(fixture.away).name);
    state.world12_teams[token]=fixture.home;state.world12_teams[token+1]=fixture.away;
    row.source_match=source_match;row.played=score.has_value();row.source_result=score.value_or(0xcccc);
    row.home_goals=score?int(*score>>5&31):0;row.away_goals=score?int(*score&31):0;
    row.source_second_leg=other.value_or(0xcccc);row.second_leg_visible=other.has_value();row.second_leg_array_present=other.has_value();if(other){row.second_leg_home_goals=*other&31;row.second_leg_away_goals=*other>>5&31;}
    const auto metadata=[&](ClubId id){
        const auto& club=world.clubs.at(id);OtherGamesClubMetadata result;
        const auto squad=std::find_if(world.national_squads.begin(),world.national_squads.end(),[&](const auto& value){return value.team==id;});
        if(!club.league.valid()&&squad==world.national_squads.end()){result.foreign_strength=club.rating;return result;}
        GameState view;view.squad_slots.assign(24,0xfff);view.team_morale={club.morale};
        const auto& roster=squad==world.national_squads.end()?club.roster:squad->roster;
        for(const auto& slot:roster)if(!slot.empty()){
            const auto index=view.players.size();view.players.push_back(world.players.at(slot.player).attributes);view.squad_slots[index]=uint16_t(index);
        }
        result.has_squad_values=true;result.averages=other_games_club_averages(view,0);return result;
    };
    row.home_metadata=metadata(fixture.home);row.away_metadata=metadata(fixture.away);
    OtherGamesDisplayRow display;display.name_colors={0x5f,0x5f};display.countries={0xff,0xff};
    const auto result=[](uint16_t value){return std::to_string(value>>5&31)+":"+std::to_string(value&31);};
    display.primary_result=score?result(*score):"-";if(other)display.second_leg_result=std::to_string(*other&31)+":"+std::to_string(*other>>5&31);
    for(size_t side=0;side<2;++side) {
        const auto club_id=side?fixture.away:fixture.home;const auto& club=world.clubs.at(club_id);
        display.league_tiers[side]=club.league.valid()?world.catalogue.league(club.league).local_index:255;
        if(!club.league.valid()||!world.catalogue.league(club.league).playable)display.countries[side]=uint8_t(club.country);
        for(const auto& manager:world.managers)if(manager.club==club_id)display.name_colors[side]=manager.seat==state.world12_identity->manager_slot?0x7a:0x75;if(world.national_coaches)for(const auto& coach:*world.national_coaches)if(coach.active&&coach.team==club_id)display.name_colors[side]=coach.coach==state.world12_identity->manager_slot?0x7a:0x75;
    }
    state.projection.rows.push_back(std::move(row));state.rows.push_back(std::move(display));
    if(baseline>=0)state.fixture_baselines.push_back(baseline);
}
}
World12OtherSelection default_world12_other_selection(const bmh::world12::World& world) {
    if(world.catalogue.scope.germany)return {};
    World12OtherSelection selection;selection.page=world.catalogue.scope.england?World12OtherPage::fa_cup:World12OtherPage::national_qualifiers;return selection;
}
OtherGamesState world12_european_games_state(const bmh::world12::CareerSnapshot& snapshot,
    const World12OtherGamesIdentity& owner,World12OtherSelection selection,const std::vector<World12CompetitionOverlay>& overlays){
    const auto& world=snapshot.world;world.validate();validate_owner(world,owner);const int page=int(selection.page);
    if(page<1||page>3||!enabled(world,page))throw FormatError("other games: European page lacks its actual country scope");
    OtherGamesState state;state.world12_identity=owner;auto& p=state.projection;p.competition=page;p.descriptor_competition=page;p.title=labels[page];p.rounds=1;p.knockout=true;
    for(int slot=0;slot<8;++slot){const bool active=enabled(world,slot);p.selectors.push_back({slot,active?slot:-1,active?labels[slot]:"",active});state.selector_labels[size_t(slot)]=active?labels[slot]:"";}
    if(!world.european_campaigns){if(!overlays.empty())throw FormatError("other games: live European score has no actual campaign owner");p.fixtures_available=false;state.unavailable_message="Europapokal-Verlauf unbekannt";return state;}
    const auto& campaign=(*world.european_campaigns)[size_t(page-1)];
    const auto active=std::find_if(campaign.stages.begin(),campaign.stages.end(),[&](const auto& stage){return stage.rules.entry==campaign.active_entry;});if(active==campaign.stages.end())throw FormatError("other games: European active stage is missing");
    const int selected=selection.stage<0?int(active-campaign.stages.begin()):selection.stage;if(selected<0||size_t(selected)>=campaign.stages.size())throw FormatError("other games: European stage selector is stale");
    auto stage=campaign.stages[size_t(selected)];const auto& rules=stage.rules;const int legs=(rules.flags&1)+1;
    std::vector<EuropeanGroup> current;if(!campaign.complete)current=prepare_european_groups(campaign);
    std::set<uint8_t> overlaid;for(const auto& overlay:overlays){if(!overlaid.insert(overlay.source_entry).second)throw FormatError("other games: repeated live European score owner");const auto group=std::find_if(current.begin(),current.end(),[&](const auto& value){return value.source_entry==overlay.source_entry;});
        if(group==current.end()||overlay.revision!=campaign.revision||overlay.played!=group->played||overlay.matches!=group->matches||overlay.scores.size()!=group->matches.size())throw FormatError("other games: live European score overlay has another actual owner");
        if(overlay.source_entry!=rules.entry)continue;size_t offset=0;if(rules.knockout())for(int round=0;round<stage.played/legs;++round)offset+=size_t(rules.teams)>>(round+1);else offset=size_t(stage.played)*rules.teams/2;
        for(size_t match=0;match<overlay.scores.size();++match){if(overlay.scores[match]==0xcccc||(overlay.scores[match]>>10)>2||(!rules.knockout()&&(overlay.scores[match]>>10)))throw FormatError("other games: invalid actual live European score");stage.scores[0].at(offset+match)=overlay.scores[match];}
    }
    p.entry_index=rules.entry;p.entry_address=0;p.linked_page=selected;p.flags=rules.flags;p.knockout=rules.knockout();p.two_legs=(rules.flags&1)!=0;p.rounds=rules.rounds;p.current_round=p.knockout?stage.played/legs:stage.played;p.round=selection.round<0?std::min(p.current_round,p.rounds-1):selection.round;
    if(p.round<0||p.round>=p.rounds)throw FormatError("other games: European round selector is stale");p.group_index=rules.group_index;p.group_count=rules.group_count;
    p.round_heading=p.knockout?std::to_string(p.round+1)+". RUNDE":"GRUPPE "+std::to_string(rules.group_index+1);p.round_arrows_visible=p.rounds>1;
    for(size_t i=0;i<campaign.stages.size();++i){const auto& linked=campaign.stages[i].rules;p.linked_pages.push_back({int(i),linked.entry,page,linked.group_index,linked.group_count});}
    const bool started=!stage.teams.empty()&&stage.teams.front().valid();p.source_participant_count=p.knockout?rules.teams>>p.round:rules.teams;p.display_mode_toggle_available=p.source_participant_count>16;p.display_mode=p.display_mode_toggle_available?selection.mode:OtherGamesDisplayMode::detail;
    if(selection.participant_offset<0||selection.participant_offset%16||selection.participant_offset>=std::max(1,p.source_participant_count))throw FormatError("other games: European participant cursor is invalid");state.source_participant_offset=selection.participant_offset;p.participant_offset=p.display_mode==OtherGamesDisplayMode::detail?selection.participant_offset:0;
    if(!started||(p.knockout&&p.round>p.current_round)){p.fixtures_available=false;state.unavailable_message="Auslosung noch nicht erfolgt";return state;}
    std::vector<bmh::world12::Fixture> fixtures;size_t score_offset=0;
    if(p.knockout){size_t start=0;for(int round=0;round<p.round;++round)start+=size_t(rules.teams)>>round;score_offset=start/2;for(int i=0;i<p.source_participant_count;i+=2)fixtures.push_back({stage.teams.at(start+size_t(i)),stage.teams.at(start+size_t(i+1))});}
    else {score_offset=size_t(p.round)*rules.teams/2;for(const auto& fixture:bmh::generate_fixtures(rules.teams,p.round+1,rules.rounds/legs,0))fixtures.push_back({stage.teams.at(size_t(fixture.home)),stage.teams.at(size_t(fixture.away))});}
    const size_t first=p.knockout&&p.display_mode==OtherGamesDisplayMode::detail?size_t(selection.participant_offset/2):0,last=std::min(fixtures.size(),p.knockout&&p.display_mode==OtherGamesDisplayMode::detail?first+8:fixtures.size());
    for(size_t match=first;match<last;++match){const auto primary=stage.scores[0].at(score_offset+match);const auto secondary=stage.scores[1].at(score_offset+match);const bool show_second=p.knockout&&p.two_legs&&(stage.played%2||p.round<stage.played/2);
        add_fixture(state,world,fixtures[match],primary==0xcccc?std::nullopt:std::optional(primary),show_second&&secondary!=0xcccc?std::optional(secondary):std::nullopt,int(match));
        auto& row=p.rows.back();row.second_leg_array_present=p.knockout&&p.two_legs;row.source_second_leg=secondary;row.second_leg_visible=show_second;
        if(primary!=0xcccc&&p.knockout){MatchGroup tie;tie.desc.assign(0xd0,0);tie.desc[1]=rules.flags;tie.cup_results={primary};if(p.two_legs)tie.cup_other_leg={secondary};row.source_winner=bmh::cup_tie(tie,0);}
    }
    if(!p.knockout){const auto standings=project_source_group_table(stage.teams,rules.flags,rules.rounds,stage.played,stage.scores[0]);p.group_tables.push_back(standings.table);state.previous_rank_visible=stage.played>1;
        for(size_t i=0;i<stage.teams.size();++i){const auto club=stage.teams[i];const int token=1000+int(i);state.world12_teams[token]=club;OtherGamesTableRow row;row.runtime_team=token;row.rank=standings.table.rank(int(i))+1;row.previous_rank=standings.table.previous_rank(int(i))+1;row.name=utf8_to_original(world.clubs.at(club).name);row.name_color=0x6a;row.rank_color=0x7a;for(const auto& manager:world.managers)if(manager.club==club)row.name_color=manager.seat==owner.manager_slot?0x7a:0x75;row.form=standings.table.form[i];row.games=standings.table.games(int(i));row.wins=standings.table.wins[i];row.defeats=standings.table.defeats[i];row.draws=row.games-row.wins-row.defeats;row.goals_for=standings.table.goals_for[i];row.goals_against=standings.table.goals_against[i];row.points=standings.table.points[i];state.table_rows.push_back(std::move(row));}
    }
    return state;
}
OtherGamesState world12_national_games_state(const bmh::world12::CareerSnapshot& snapshot,
    const World12OtherGamesIdentity& owner,const TextDb& texts,uint8_t source_entry,int requested_round,const std::vector<World12CompetitionOverlay>& overlays){
    World12OtherSelection selection;selection.page=World12OtherPage::national_qualifiers;selection.round=requested_round;
    const auto& world=snapshot.world;world.validate();validate_owner(world,owner);const int page=4;
    if(!world.catalogue.scope.international)throw FormatError("other games: national page lacks its actual country scope");
    OtherGamesState state;state.world12_identity=owner;auto& p=state.projection;p.competition=page;p.descriptor_competition=page;p.title=labels[page];p.rounds=1;p.knockout=true;
    for(int slot=0;slot<8;++slot){const bool active=enabled(world,slot);p.selectors.push_back({slot,active?slot:-1,active?labels[slot]:"",active});state.selector_labels[size_t(slot)]=active?labels[slot]:"";}
    if(!world.national_campaigns){if(!overlays.empty())throw FormatError("other games: live national score has no actual campaign owner");p.fixtures_available=false;state.unavailable_message="Nationaler Verlauf unbekannt";return state;}
    if(!world.national_campaigns->seasons.contains(snapshot.season))throw FormatError("other games: current national campaign is unknown");
    const auto& campaign=world.national_campaigns->seasons.at(snapshot.season);p.competition=p.descriptor_competition=campaign.competition;p.title=texts[(layout::CupDraw_FormatError_ExecutableField6-layout::TextPointerTable)/4+campaign.competition];
    if(source_entry!=255){const auto found=std::find_if(campaign.stages.begin(),campaign.stages.end(),[&](const auto& stage){return stage.rules.entry==source_entry;});if(found==campaign.stages.end())throw FormatError("other games: national source stage is not owned");selection.stage=int(found-campaign.stages.begin());}
    p.selectors[4].competition=campaign.competition;
    const auto active=std::find_if(campaign.stages.begin(),campaign.stages.end(),[&](const auto& stage){return stage.rules.entry==campaign.active_entry;});if(active==campaign.stages.end())throw FormatError("other games: national active stage is missing");
    const int selected=selection.stage<0?int(active-campaign.stages.begin()):selection.stage;if(selected<0||size_t(selected)>=campaign.stages.size())throw FormatError("other games: national stage selector is stale");
    auto stage=campaign.stages[size_t(selected)];const auto& rules=stage.rules;const int legs=(rules.flags&1)+1;
    std::vector<NationalGroup> current;if(!campaign.complete)current=prepare_national_groups(campaign);
    std::set<uint8_t> overlaid;for(const auto& overlay:overlays){if(!overlaid.insert(overlay.source_entry).second)throw FormatError("other games: repeated live national score owner");const auto group=std::find_if(current.begin(),current.end(),[&](const auto& value){return value.source_entry==overlay.source_entry;});
        if(group==current.end()||overlay.revision!=campaign.revision||overlay.played!=group->played||overlay.matches!=group->matches||overlay.scores.size()!=group->matches.size())throw FormatError("other games: live national score overlay has another actual owner");
        if(overlay.source_entry!=rules.entry)continue;size_t offset=0;if(rules.knockout())for(int round=0;round<stage.played/legs;++round)offset+=size_t(rules.teams)>>(round+1);else offset=size_t(stage.played)*rules.teams/2;
        for(size_t match=0;match<overlay.scores.size();++match){if(overlay.scores[match]==0xcccc||(overlay.scores[match]>>10)>2||(!(rules.flags&8)&&(overlay.scores[match]>>10)))throw FormatError("other games: invalid actual live national score");stage.scores[0].at(offset+match)=overlay.scores[match];}
    }
    p.entry_index=rules.entry;p.entry_address=0;p.linked_page=selected;p.flags=rules.flags;p.knockout=rules.knockout();p.two_legs=(rules.flags&1)!=0;p.rounds=rules.rounds;p.current_round=p.knockout?stage.played/legs:stage.played;p.round=selection.round<0?std::min(p.current_round,p.rounds-1):selection.round;
    if(p.round<0||p.round>=p.rounds)throw FormatError("other games: national round selector is stale");p.group_index=rules.group_index;p.group_count=rules.groups;
    p.round_heading=p.knockout?std::to_string(p.round+1)+". RUNDE":"GRUPPE "+std::to_string(rules.group_index+1);p.round_arrows_visible=p.rounds>1;
    for(size_t i=0;i<campaign.stages.size();++i){const auto& linked=campaign.stages[i].rules;p.linked_pages.push_back({int(i),linked.entry,page,linked.group_index,linked.groups});}
    const bool started=!stage.teams.empty()&&stage.teams.front().valid();p.source_participant_count=p.knockout?rules.teams>>p.round:rules.teams;p.display_mode_toggle_available=p.source_participant_count>16;p.display_mode=p.display_mode_toggle_available?selection.mode:OtherGamesDisplayMode::detail;
    if(selection.participant_offset<0||selection.participant_offset%16||selection.participant_offset>=std::max(1,p.source_participant_count))throw FormatError("other games: national participant cursor is invalid");state.source_participant_offset=selection.participant_offset;p.participant_offset=p.display_mode==OtherGamesDisplayMode::detail?selection.participant_offset:0;
    if(!started||(p.knockout&&p.round>p.current_round)){p.fixtures_available=false;state.unavailable_message="Auslosung noch nicht erfolgt";return state;}
    std::vector<bmh::world12::Fixture> fixtures;size_t score_offset=0;
    if(p.knockout){size_t start=0;for(int round=0;round<p.round;++round)start+=size_t(rules.teams)>>round;score_offset=start/2;for(int i=0;i<p.source_participant_count;i+=2)fixtures.push_back({stage.teams.at(start+size_t(i)),stage.teams.at(start+size_t(i+1))});}
    else {score_offset=size_t(p.round)*rules.teams/2;for(const auto& fixture:bmh::generate_fixtures(std::max(4,int(rules.teams)),p.round+1,rules.rounds/legs,0))fixtures.push_back({stage.teams.at(size_t(fixture.home)),stage.teams.at(size_t(fixture.away))});}
    const size_t first=p.knockout&&p.display_mode==OtherGamesDisplayMode::detail?size_t(selection.participant_offset/2):0,last=std::min(fixtures.size(),p.knockout&&p.display_mode==OtherGamesDisplayMode::detail?first+8:fixtures.size());
    for(size_t match=first;match<last;++match){const auto primary=stage.scores[0].at(score_offset+match);const auto secondary=stage.scores[1].at(score_offset+match);const bool show_second=p.knockout&&p.two_legs&&(stage.played%2||p.round<stage.played/2);
        add_fixture(state,world,fixtures[match],primary==0xcccc?std::nullopt:std::optional(primary),show_second&&secondary!=0xcccc?std::optional(secondary):std::nullopt,int(match));
        auto& row=p.rows.back();row.second_leg_array_present=p.knockout&&p.two_legs;row.source_second_leg=secondary;row.second_leg_visible=show_second;
        if(primary!=0xcccc&&p.knockout){MatchGroup tie;tie.desc.assign(0xd0,0);tie.desc[1]=rules.flags;tie.cup_results={primary};if(p.two_legs)tie.cup_other_leg={secondary};row.source_winner=bmh::cup_tie(tie,0);}
    }
    if(!p.knockout&&rules.teams>2){const auto standings=project_source_group_table(stage.teams,rules.flags,rules.rounds,stage.played,stage.scores[0]);p.group_tables.push_back(standings.table);state.previous_rank_visible=stage.played>1;
        for(size_t i=0;i<stage.teams.size();++i){const auto club=stage.teams[i];const int token=1000+int(i);state.world12_teams[token]=club;OtherGamesTableRow row;row.runtime_team=token;row.rank=standings.table.rank(int(i))+1;row.previous_rank=standings.table.previous_rank(int(i))+1;row.name=utf8_to_original(world.clubs.at(club).name);row.name_color=0x6a;row.rank_color=0x7a;for(const auto& manager:world.managers)if(manager.club==club)row.name_color=manager.seat==owner.manager_slot?0x7a:0x75;row.form=standings.table.form[i];row.games=standings.table.games(int(i));row.wins=standings.table.wins[i];row.defeats=standings.table.defeats[i];row.draws=row.games-row.wins-row.defeats;row.goals_for=standings.table.goals_for[i];row.goals_against=standings.table.goals_against[i];row.points=standings.table.points[i];state.table_rows.push_back(std::move(row));}
    }
    return state;
}
OtherGamesState world12_regional_games_state(const bmh::world12::CareerSnapshot& snapshot,
    const World12OtherGamesIdentity& owner,const TextDb& texts,int requested_round,const std::optional<World12CompetitionOverlay>& overlay){
    snapshot.validate();const auto& world=snapshot.world;validate_owner(world,owner);
    if(!world.regional_playoff||!world.catalogue.scope.germany)throw FormatError("other games: actual regional playoff owner is unknown or disabled");
    const auto& campaign=*world.regional_playoff;if(campaign.season!=snapshot.season||!campaign.activation_day)throw FormatError("other games: regional playoff has not actually activated");
    OtherGamesState state;state.world12_identity=owner;auto& p=state.projection;
    p.competition=p.descriptor_competition=13;p.entry_index=35;p.entry_address=0;p.title=texts[(layout::CupDraw_FormatError_ExecutableField6-layout::TextPointerTable)/4+13];p.flags=campaign.rules.flags;p.knockout=false;p.rounds=6;p.current_round=campaign.played;p.round=requested_round<0?std::min<int>(5,campaign.played):requested_round;
    if(p.round<0||p.round>=6)throw FormatError("other games: regional round selector is stale");
    p.round_heading=std::to_string(p.round+1)+". RUNDE";p.round_arrows_visible=true;p.source_participant_count=4;p.display_mode=OtherGamesDisplayMode::detail;
    for(int slot=0;slot<8;++slot){p.selectors.push_back({slot,slot==0?13:-1,slot==0?p.title:"",slot==0});state.selector_labels[size_t(slot)]=slot==0?p.title:"";}
    std::vector<ClubId> teams(campaign.entrants.begin(),campaign.entrants.end());std::vector<uint16_t> scores(campaign.scores.begin(),campaign.scores.end());
    if(overlay){const auto actual=prepare_regional_playoff_group(snapshot);if(!actual||overlay->source_entry!=35||overlay->revision!=campaign.revision||overlay->played!=campaign.played||overlay->matches!=actual->matches||overlay->scores.size()!=2)throw FormatError("other games: stale or mismatched actual regional overlay");for(size_t m=0;m<2;++m){if(overlay->scores[m]==0xcccc||overlay->scores[m]>>10)throw FormatError("other games: invalid actual regional score");scores.at(size_t(campaign.played)*2+m)=overlay->scores[m];}}
    const auto fixtures=bmh::generate_fixtures(4,p.round+1,3,0);for(size_t m=0;m<fixtures.size();++m){const auto& fixture=fixtures[m];const auto score=scores.at(size_t(p.round)*2+m);add_fixture(state,world,{teams.at(fixture.home),teams.at(fixture.away)},score==0xcccc?std::nullopt:std::optional(score),{},int(m));}
    const auto table=project_source_group_table(teams,campaign.rules.flags,6,campaign.played,scores);p.group_tables.push_back(table.table);state.previous_rank_visible=campaign.played>1;
    for(size_t i=0;i<4;++i){const auto club=teams[i];const int token=1000+int(i);state.world12_teams[token]=club;OtherGamesTableRow row;row.runtime_team=token;row.rank=table.table.rank(int(i))+1;row.previous_rank=table.table.previous_rank(int(i))+1;row.name=utf8_to_original(world.clubs.at(club).name);row.name_color=0x6a;row.rank_color=0x7a;for(const auto& manager:world.managers)if(manager.club==club)row.name_color=manager.seat==owner.manager_slot?0x7a:0x75;row.form=table.table.form[i];row.games=table.table.games(int(i));row.wins=table.table.wins[i];row.defeats=table.table.defeats[i];row.draws=row.games-row.wins-row.defeats;row.goals_for=table.table.goals_for[i];row.goals_against=table.table.goals_against[i];row.points=table.table.points[i];state.table_rows.push_back(std::move(row));}
    return state;
}
OtherGamesState world12_indoor_games_state(const bmh::world12::CareerSnapshot& snapshot,
    const World12OtherGamesIdentity& owner,uint8_t ordinal,uint8_t entry,int requested_round,const std::optional<World12CompetitionOverlay>& overlay){
    const auto& world=snapshot.world;world.validate();validate_owner(world,owner);
    if(!world.indoor||!world.catalogue.scope.germany)throw FormatError("other games: actual indoor season is unknown or disabled");
    const auto& indoor=*world.indoor;const auto tournament=std::find_if(indoor.tournaments.begin(),indoor.tournaments.end(),[&](const auto& value){return value.ordinal==ordinal;});
    if(tournament==indoor.tournaments.end()||entry<25||entry>29)throw FormatError("other games: indoor tournament/stage is not actually drawn");
    const auto& rules=indoor.rules[size_t(entry-25)];const auto& t=*tournament;const auto played=t.played[size_t(entry-25)];const bool knockout=(rules.flags&4)!=0;
    const auto round=requested_round<0?std::min<int>(played,rules.rounds-1):requested_round;if(round<0||round>=rules.rounds)throw FormatError("other games: indoor displayed round is stale");
    std::vector<ClubId> teams;for(size_t i=0;i<rules.teams;++i)teams.push_back(t.teams.at(rules.team_offset+i));std::vector<uint16_t> scores;for(size_t i=0;i<size_t(rules.rounds)*rules.teams/2;++i)scores.push_back(t.scores.at(rules.result_offset+i));
    if(overlay){if(overlay->revision!=indoor.revision||overlay->source_entry!=entry||overlay->played!=played||t.complete||indoor.tournaments.back().ordinal!=ordinal)throw FormatError("other games: stale live indoor score owner");const auto groups=prepare_indoor_groups(indoor);const auto actual=std::find_if(groups.begin(),groups.end(),[&](const auto& group){return group.source_entry==entry;});if(actual==groups.end()||actual->matches!=overlay->matches||overlay->scores.size()!=actual->matches.size())throw FormatError("other games: live indoor fixtures differ from actual current pass");for(size_t m=0;m<overlay->scores.size();++m){if(overlay->scores[m]==0xcccc||(overlay->scores[m]>>10)>((rules.flags&8)?2:0))throw FormatError("other games: invalid actual indoor live score");scores.at(size_t(played)*rules.teams/2+m)=overlay->scores[m];}}
    OtherGamesState state;state.world12_identity=owner;auto& p=state.projection;p.competition=p.descriptor_competition=11;p.entry_index=entry;p.entry_address=0;p.title=utf8_to_original(t.name);p.flags=rules.flags;p.knockout=knockout;p.round=round;p.current_round=played;p.rounds=rules.rounds;p.group_index=rules.group_index;p.group_count=rules.groups;p.display_mode=OtherGamesDisplayMode::detail;p.round_arrows_visible=rules.rounds>1;p.source_participant_count=rules.teams;p.round_heading=entry>=27?(entry==27?"HALBFINALE":entry==28?"FINALE":"SPIEL UM PLATZ3"):"GRUPPE "+std::to_string(rules.group_index+1);
    for(int slot=0;slot<8;++slot){p.selectors.push_back({slot,slot==0?11:-1,slot==0?"HALLEN-TURNIER":"",slot==0});state.selector_labels[size_t(slot)]=slot==0?"HALLEN-TURNIER":"";}
    for(size_t i=0;i<indoor.rules.size();++i){const auto& r=indoor.rules[i];p.linked_pages.push_back({int(i),r.entry,11,r.group_index,r.groups});}
    if(std::any_of(teams.begin(),teams.end(),[](auto club){return !club.valid();})){p.fixtures_available=false;state.unavailable_message="Noch keine Paarung";return state;}
    std::vector<bmh::world12::Fixture> fixtures;if(knockout)for(size_t i=0;i<teams.size();i+=2)fixtures.push_back({teams[i],teams[i+1]});else for(const auto& fixture:bmh::generate_fixtures(std::max(4,int(rules.teams)),round+1,rules.rounds/((rules.flags&1)+1),0))fixtures.push_back({teams[size_t(fixture.home)],teams[size_t(fixture.away)]});
    for(size_t m=0;m<fixtures.size();++m){const auto score=scores.at(size_t(round)*rules.teams/2+m);add_fixture(state,world,fixtures[m],score==0xcccc?std::nullopt:std::optional(score),{},int(m));}
    if(!knockout&&rules.teams>2){const auto table=project_source_group_table(teams,rules.flags,rules.rounds,played,scores);p.group_tables.push_back(table.table);state.previous_rank_visible=played>1;for(size_t i=0;i<teams.size();++i){const auto club=teams[i];const int token=1000+int(i);state.world12_teams[token]=club;OtherGamesTableRow row;row.runtime_team=token;row.rank=table.table.rank(int(i))+1;row.previous_rank=table.table.previous_rank(int(i))+1;row.name=utf8_to_original(world.clubs.at(club).name);row.name_color=0x6a;row.rank_color=0x7a;for(const auto& manager:world.managers)if(manager.club==club)row.name_color=manager.seat==owner.manager_slot?0x7a:0x75;row.form=table.table.form[i];row.games=table.table.games(int(i));row.wins=table.table.wins[i];row.defeats=table.table.defeats[i];row.draws=row.games-row.wins-row.defeats;row.goals_for=table.table.goals_for[i];row.goals_against=table.table.goals_against[i];row.points=table.table.points[i];state.table_rows.push_back(std::move(row));}}
    return state;
}
OtherGamesState world12_other_games_state(const bmh::world12::CareerSnapshot& snapshot,
    const World12OtherGamesIdentity& owner,World12OtherSelection selection,const std::vector<World12CompetitionOverlay>& overlays,const TextDb* texts) {
    if(selection.page==World12OtherPage::national_qualifiers){if(!texts)throw FormatError("other games: actual national programme needs its Source text context");uint8_t entry=255;if(selection.stage>=0){if(!snapshot.world.national_campaigns||!snapshot.world.national_campaigns->seasons.contains(snapshot.season)||size_t(selection.stage)>=snapshot.world.national_campaigns->seasons.at(snapshot.season).stages.size())throw FormatError("other games: stale national stage selector");entry=snapshot.world.national_campaigns->seasons.at(snapshot.season).stages[size_t(selection.stage)].rules.entry;}return world12_national_games_state(snapshot,owner,*texts,entry,selection.round,overlays);}
    if(int(selection.page)>=1&&int(selection.page)<=3)return world12_european_games_state(snapshot,owner,selection,overlays);
    const auto& world=snapshot.world;world.validate();validate_cup_programmes(world,snapshot.cups);validate_owner(world,owner);
    std::map<const KnockoutStage*,const World12CompetitionOverlay*> current_scores;
    for(const auto& overlay:overlays){const KnockoutStage* matched=nullptr;
        for(const auto& stage:snapshot.cups){if(stage.complete()||stage.source_entry!=overlay.source_entry||stage.revision!=overlay.revision)continue;const auto legs=stage.two_legs()?2u:1u;const auto played=(stage.rounds.size()-1)*legs+stage.next_leg()-1;
            if(played!=overlay.played||stage.next_fixtures()!=overlay.matches)continue;if(matched)throw FormatError("other games: ambiguous actual domestic score owner");matched=&stage;}
        if(!matched||overlay.scores.size()!=overlay.matches.size()||!current_scores.emplace(matched,&overlay).second)throw FormatError("other games: stale, repeated or mismatched actual domestic score owner");
        for(auto score:overlay.scores)if(score==0xcccc||(score>>10)>2||(matched->two_legs()&&matched->next_leg()==1&&(score>>10)))throw FormatError("other games: invalid actual domestic live score");
    }
    const auto displayed_score=[&](const KnockoutStage& stage,size_t round,size_t match,bool second)->std::optional<uint16_t>{
        if(const auto found=current_scores.find(&stage);found!=current_scores.end()&&round+1==stage.rounds.size()&&second==(stage.next_leg()==2))return found->second->scores.at(match);
        const auto& saved=second?stage.rounds.at(round).second_leg:stage.rounds.at(round).first_leg;if(saved.empty())return {};return saved.at(match);
    };
    const int page=int(selection.page);
    if(page<0||page>=8||!enabled(world,page))throw FormatError("other games: disabled typed competition page");
    OtherGamesState state;state.world12_identity=owner;auto& p=state.projection;
    p.competition=page;p.descriptor_competition=page;p.entry_index=0;p.round=0;p.rounds=1;p.knockout=true;
    p.display_mode=selection.mode;p.title=labels[page];p.round_heading="1. RUNDE";
    for(int slot=0;slot<8;++slot) {
        const bool active=enabled(world,slot);
        p.selectors.push_back({slot,active?slot:-1,active?labels[slot]:"",active});
        state.selector_labels[size_t(slot)]=active?labels[slot]:"";
    }
    if(selection.page==World12OtherPage::supercups) {
        p.display_mode=OtherGamesDisplayMode::detail;p.title="SUPER-CUP";p.round_heading="FINALSPIELE";
        int block=0;const int blocks=int(world.catalogue.scope.germany)+int(world.catalogue.scope.england);
        if(blocks==1){p.round_heading="FINALE";if(world.catalogue.scope.england)p.title="CHARITY SHIELD";}
        for(const auto cup:{DomesticCup::german_supercup,DomesticCup::english_supercup}) {
            if(!world.catalogue.scope.domestic_enabled(cup_country(cup)))continue;
            const int baseline=blocks==1?104:70+60*block++;
            OtherGamesState::Section section;
            section.title=cup==DomesticCup::german_supercup?"Deutschland - Super-Cup":"England - Charity Shield";
            section.first_row=p.rows.size();section.heading_baseline=baseline-16;section.fixture_baseline=baseline;
            const auto available=stages(snapshot,cup);
            if(available.empty())section.message="Noch keine Paarung";
            else {
                const auto& round=available.back()->rounds.front();
                if(round.fixtures.size()!=1)throw FormatError("other games: a supercup must contain one independent final");
                add_fixture(state,world,round.fixtures[0],displayed_score(*available.back(),available.back()->rounds.size()-1,0,false),{},0,baseline);section.row_count=1;
            }
            state.sections.push_back(std::move(section));
        }
        p.source_participant_count=int(p.rows.size())*2;return state;
    }
    std::optional<DomesticCup> cup;
    if(selection.page==World12OtherPage::german_cup)cup=DomesticCup::german_cup;
    if(selection.page==World12OtherPage::fa_cup)cup=DomesticCup::fa_cup;
    if(selection.page==World12OtherPage::league_cup)cup=DomesticCup::league_cup;
    const auto available=cup?stages(snapshot,*cup):std::vector<const KnockoutStage*>{};
    if(available.empty()) {p.fixtures_available=false;state.unavailable_message="Spielplan noch nicht initialisiert";return state;}
    const int stage_index=selection.stage<0?int(available.size())-1:selection.stage;
    if(stage_index<0||stage_index>=int(available.size()))throw FormatError("other games: unknown typed cup stage");
    const auto& stage=*available[size_t(stage_index)];
    p.entry_index=stage.source_entry;p.linked_page=stage_index;p.rounds=stage.planned_rounds;p.round=selection.round<0?int(stage.rounds.size())-1:selection.round;
    if(p.round<0||p.round>=p.rounds)throw FormatError("other games: invalid typed round");
    p.flags=stage.flags;p.current_round=int(stage.rounds.size())-1;p.round_arrows_visible=p.rounds>1;p.two_legs=stage.two_legs();
    p.round_heading=std::to_string(p.round+1)+". RUNDE";
    if(available.size()>1)for(size_t i=0;i<available.size();++i)p.linked_pages.push_back({int(i),available[i]->source_entry,page,0,1});
    p.source_participant_count=int(stage.rounds.front().fixtures.size()*2)>>p.round;
    p.display_mode_toggle_available=p.source_participant_count>16;
    // CupGroupOverview forces detail when this stage fits one detail window.
    if(!p.display_mode_toggle_available)p.display_mode=OtherGamesDisplayMode::detail;
    if(selection.participant_offset<0||selection.participant_offset%16 || selection.participant_offset>=std::max(1,p.source_participant_count))throw FormatError("other games: invalid typed participant window");
    state.source_participant_offset=selection.participant_offset;p.participant_offset=p.display_mode==OtherGamesDisplayMode::detail?selection.participant_offset:0;
    if(p.source_participant_count>16)state.selector_labels[size_t(page)]+=" "+std::to_string(selection.participant_offset/16+1);
    if(p.round>=int(stage.rounds.size())) {p.fixtures_available=false;state.unavailable_message="Auslosung noch nicht erfolgt";return state;}
    const auto& round=stage.rounds[size_t(p.round)];
    const size_t first=p.display_mode==OtherGamesDisplayMode::detail?size_t(selection.participant_offset/2):0;
    const size_t last=std::min(round.fixtures.size(),p.display_mode==OtherGamesDisplayMode::detail?first+8:round.fixtures.size());
    for(size_t m=first;m<last;++m)add_fixture(state,world,round.fixtures[m],displayed_score(stage,size_t(p.round),m,false),
        displayed_score(stage,size_t(p.round),m,true),int(m));
    return state;
}
World12OtherSelection navigate_world12_other_games(const bmh::world12::CareerSnapshot& snapshot,const World12OtherGamesIdentity& owner,
    World12OtherSelection selection,const OtherGamesRequest& request,const TextDb* texts) {
    if(!request.world12_identity || *request.world12_identity!=owner || request.identity!=OtherGamesIdentity{} || request.world12_team.valid())
        throw FormatError("other games: stale/mixed typed navigation request");
    const auto current=world12_other_games_state(snapshot,owner,selection,{},texts);
    if(request.action!=OtherGamesAction::select_competition&&request.action!=OtherGamesAction::select_linked_page&&request.value!=-1)
        throw FormatError("other games: unexpected typed navigation value");
    switch(request.action) {
    case OtherGamesAction::select_competition:
        if(current.projection.display_mode!=OtherGamesDisplayMode::detail)throw FormatError("other games: competition selector is not visible");
        if(request.value<0||request.value>=8||!enabled(snapshot.world,request.value))throw FormatError("other games: disabled selector");
        if(request.value==int(selection.page)&&current.projection.source_participant_count>16&&current.projection.fixtures_available){
            selection.participant_offset+=16;
            if(selection.participant_offset>=current.projection.source_participant_count)selection.participant_offset=0;
        }else {selection={};selection.page=World12OtherPage(request.value);}
        break;
    case OtherGamesAction::previous_round:case OtherGamesAction::next_round:
        if(current.projection.display_mode!=OtherGamesDisplayMode::detail||!current.projection.round_arrows_visible)throw FormatError("other games: round arrows are disabled");
        selection.round=(current.projection.round+current.projection.rounds+(request.action==OtherGamesAction::next_round?1:-1))%current.projection.rounds;
        selection.participant_offset=0;
        break;
    case OtherGamesAction::select_linked_page:
        if(current.projection.display_mode!=OtherGamesDisplayMode::detail||request.value<0||size_t(request.value)>=current.projection.linked_pages.size())throw FormatError("other games: stage selector is not visible");
        selection.stage=request.value;selection.round=-1;selection.participant_offset=0;break;
    case OtherGamesAction::toggle_display_mode:
        if(!current.projection.display_mode_toggle_available||!current.projection.fixtures_available)throw FormatError("other games: display mode control is disabled");
        selection.mode=selection.mode==OtherGamesDisplayMode::detail?OtherGamesDisplayMode::compact:OtherGamesDisplayMode::detail;break;
    default:throw FormatError("other games: action belongs to the typed window owner");
    }
    world12_other_games_state(snapshot,owner,selection,{},texts);return selection;
}
}
