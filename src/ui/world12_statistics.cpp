// SPDX-License-Identifier: GPL-3.0-only
#include "ui/world12_statistics.hpp"
#include "game/world12/desktop_projection.hpp"
#include "game/world12/prediction.hpp"
#include "game/world12/standings.hpp"
#include "game/messages.hpp"
#include "ui/manager.hpp"
#include <algorithm>
#include <set>
namespace bmh::ui {
using namespace bmh::world12;
namespace {
uint8_t color(const World& world,ClubId club,uint8_t actor){
    for(const auto& manager:world.managers)if(manager.club==club)return manager.seat==actor?0x7a:0x75;
    return 0x5f;
}
StatisticsRankingState rankings(const CareerSnapshot& snapshot,const World12ReadonlyIdentity& owner,
    const World12StatisticsSelection& selection,const LeImage& source,const Image& frame){
    if(!snapshot.world.annual_title_archive)throw FormatError("world12: historical rankings require the actual saved title archive");
    const auto& archive=*snapshot.world.annual_title_archive;
    StatisticsRankingState out;out.manager_slot=owner.manager;out.category=selection.category;
    out.first_season=selection.first_season;out.history_first_season=int(archive.first_retained_season);out.current_season=int(snapshot.season)-out.history_first_season;
    if(out.current_season<0||out.current_season>10000)throw FormatError("world12: title archive has no bounded current history window");
    out.season_number_base=int(source.u16(layout::Achievements_FormatError_RuntimeField2))+out.history_first_season;
    out.underlay=frame;out.season_known.assign(size_t(out.current_season)+1,false);
    for(uint32_t category=0;category<9;++category){
        const auto id=source.u8(layout::StatisticsRankings_FormatError_RuntimeField+category);
        if(id>=9)throw FormatError("world12: invalid actual Source title category");
        out.categories[category]={id,source.u8(layout::StatisticsRankings_FormatError_RuntimeField2+category)!=0};
        const auto size=size_t(out.current_season)+1;out.title_holders[category].assign(size,0xffff);
        out.club_names[category].resize(size);out.club_name_colors[category].assign(size,0x5f);out.title_present[category].assign(size,false);
    }
    const auto index=[&](uint32_t season){if(season<archive.first_retained_season||season>snapshot.season)throw FormatError("world12: title archive row is outside its retained range");return size_t(season-archive.first_retained_season);};
    for(const auto& season:archive.seasons){const auto row=index(season.season);out.season_known[row]=true;
        for(const auto& title:season.titles)if(title.source_category){if((title.kind==AnnualTitleKind::league||title.kind==AnnualTitleKind::domestic)&&title.country!=snapshot.world.clubs.at(owner.club).country)continue;const auto c=*title.source_category;
            if(c>=9||out.title_present[c][row])throw FormatError("world12: repeated Source historical trophy category");
            out.title_present[c][row]=true;out.club_names[c][row]=utf8_to_original(snapshot.world.clubs.at(title.winner).name);out.club_name_colors[c][row]=color(snapshot.world,title.winner,owner.manager);
        }
    }
    for(const auto& [season,labels]:archive.unresolved_source_labels){if(season<archive.first_retained_season||season>snapshot.season)continue;const auto row=index(season);out.season_known[row]=true;
        for(size_t c=0;c<9;++c)if(!out.title_present[c][row]&&!labels[c].empty()){out.club_names[c][row]=labels[c];out.title_present[c][row]=true;}
    }
    return out;
}
}
World12StatisticsState world12_statistics_state(const CareerSnapshot& snapshot,const World12ReadonlyIdentity& owner,
    World12StatisticsSelection selection,const GameState& reference,const LeImage& source,const Image& frame){
    snapshot.validate();const auto& world=snapshot.world;
    const auto actor=std::find_if(world.managers.begin(),world.managers.end(),[&](const auto& manager){return manager.seat==owner.manager;});
    if(owner.menu!=23||actor==world.managers.end()||actor->club!=owner.club||owner.date!=snapshot.date||!snapshot.desktop||snapshot.desktop->active_seat!=owner.manager||selection.page< -1||selection.page>8||frame.width!=320||frame.height!=240)
        throw FormatError("world12: statistics page lacks its canonical active desktop owner");
    World12StatisticsState out;out.selection=selection;out.menu.owner={};out.menu.owner.kind=DeskRequestKind::open_menu;out.menu.owner.menu_kind=23;out.menu.owner.manager_slot=owner.manager;out.menu.owner.revision=owner.revision;out.menu.underlay=frame;
    out.menu.transfer_history_count=snapshot.transfers?int(snapshot.transfers->history[owner.manager].size()):0;
    out.menu.gradient_text=reference.memory.u8(layout::TextGradientDisabled)==0&&source.u8(layout::TextGradientMode)==2;
    if(world.economy&&world.economy->presentation_options)out.menu.gradient_text=(*world.economy->presentation_options)[layout::TextGradientDisabled-layout::LeaguePresentationOptions]==0&&source.u8(layout::TextGradientMode)==2;
    if(selection.page<0)return out;
    if(selection.page==7){out.rankings=rankings(snapshot,owner,selection,source,frame);return out;}
    if(selection.page==1){
        if(!snapshot.transfers)throw FormatError("world12: transfer history is unknown in this saved career");
        TransferHistoryProjection page;page.manager_slot=owner.manager;
        for(const auto& row:snapshot.transfers->history[owner.manager]){const auto player=world.players.find(row.player);if(player==world.players.end())throw FormatError("world12: transfer history lost its actual global player identity");
            ui::TransferHistoryRow record;record.source_date_counter=uint16_t((row.source_word>>2)&0xffff);record.source_kind=uint8_t(row.source_word&3);record.source_fee=(row.source_word>>19)*10000u;record.saved_player_name=utf8_to_original(player->second.attributes.name);record.special_counterparty=!row.other.valid();if(row.other.valid())record.counterparty_club_name=utf8_to_original(world.clubs.at(row.other).name);page.rows.push_back(std::move(record));}
        out.transfers=std::move(page);
    }
    const auto league=world.clubs.at(owner.club).league;const auto& definition=world.catalogue.league(league);
    if(!definition.active||!definition.playable)throw FormatError("world12: statistics require the actual club league");
    const auto& members=world.membership[league.value].clubs;
    auto projection=project_manager_desktop(snapshot,reference,members);auto& game=projection.state;
    uint8_t local=0;while(local<projection.human_managers()&&projection.global_manager(local)!=owner.manager)++local;
    if(local>=projection.human_managers())throw FormatError("world12: statistics lost its canonical manager mapping");
    if(selection.page==1){out.formatting=std::move(game);return out;}
    if(selection.page==0){
        if(selection.prediction_mode!=1&&selection.prediction_mode!=2&&selection.prediction_mode!=4&&selection.prediction_mode!=8)throw FormatError("world12: unsupported Source opportunity mode");
        std::map<ClubId,uint8_t> deductions;for(const auto& manager:world.managers)if(manager.match_state&&world.clubs.at(manager.club).league==league&&manager.match_state->point_deduction>>5==3)deductions[manager.club]=manager.match_state->point_deduction&31;const auto prediction=predict_league(snapshot,owner.club,selection.prediction_mode,deductions);
        OpportunitiesState page;page.identity={int(owner.manager),int(projection.local_club(owner.club)),int(game.club_ids.at(projection.local_club(owner.club))),snapshot.date,owner.revision,owner.scope,owner.sequence};page.mode=selection.prediction_mode;page.league_size=definition.clubs;page.ranks=prediction.result.ranks;page.underlay=frame;page.gradient_text=out.menu.gradient_text;
        page.current_rank=prediction.result.best_rank;
        out.opportunities=std::move(page);return out;
    }
    if(selection.page==2){
        if(!actor->career||!actor->career->season_services||!actor->match_state||!actor->match_state->attendance||!world.economy||!world.economy->managers[owner.manager].monthly)
            throw FormatError("world12: analytics require actual season/attendance/monthly histories");
        auto page=analytics_from_game(game,source,local,frame);page.manager_slot=owner.manager;page.owner=out.menu.owner;page.series[0].absolute_max=definition.clubs;out.analytics=std::move(page);return out;
    }
    if(selection.page==5){
        if(!actor->finance||!world.economy||!world.markets||!world.economy->managers[owner.manager].monthly)throw FormatError("world12: financial totals require the actual account, markets and monthly owner");
        auto page=finance_totals_from_game(game,local);page.manager_slot=owner.manager;out.finance=std::move(page);return out;
    }
    if(selection.page==8){if(!actor->match_state)throw FormatError("world12: minute statistics are unknown");auto page=minute_statistics_from_game(game,local);page.manager_slot=owner.manager;out.minutes=std::move(page);return out;}
    if(selection.page==4){
        if(!world.national_campaigns)throw FormatError("world12: World-Cup statistics require the actual saved national history owner");
        std::array<ClubId,62> nations;for(const auto& [id,club]:world.clubs)if(club.kind==ClubKind::national_team){const size_t country=size_t(club.country);if(country>=nations.size()||nations[country].valid())throw FormatError("world12: ambiguous actual national identity");nations[country]=id;}
        for(auto id:nations)if(!id.valid())throw FormatError("world12: World-Cup statistics require all actual 62 national identities");
        GameState view=reference;view.club_names={0};view.club_ids.resize(size_t(kNoTeam)+62);
        view.memory.add_block(layout::Placements,72);
        for(uint32_t i=0;i<72;++i)view.memory.set8(layout::Placements+i,0);
        for(size_t row=0;row<std::min<size_t>(18,world.national_campaigns->honours.size());++row)for(size_t place=0;place<4;++place){const auto id=world.national_campaigns->honours[row][place];const auto found=std::find(nations.begin(),nations.end(),id);if(found==nations.end())throw FormatError("world12: historical podium has no actual national identity");view.memory.set8(layout::Placements+uint32_t(row*4+place),uint8_t((found-nations.begin())|128));}
        for(size_t c=0;c<62;++c){const auto name=utf8_to_original(world.clubs.at(nations[c]).name);if(view.club_names.size()+name.size()+1>65535)throw FormatError("world12: national name pool exceeds Source width");view.club_ids[size_t(kNoTeam)+c]=uint16_t(view.club_names.size());view.club_names.insert(view.club_names.end(),name.begin(),name.end());view.club_names.push_back(0);}
        auto page=world_cup_statistics_from_game(view,source,WorldCupStatisticsOwner::desk_read_only);
        for(size_t c=0;c<62;++c){const auto& crest=world.crests.at(world.clubs.at(nations[c]).crest);page.badge_atlases[c]=crest.path;page.badge_cells[c]=crest.cell;out.clubs.push_back(nations[c]);}
        out.world_cup=std::move(page);return out;
    }
    if(selection.page==3){
        static constexpr Country countries[]={Country::germany,Country::spain,Country::england,Country::france,Country::italy};
        if(selection.nation<0||selection.nation>=5)throw FormatError("world12: invalid all-time country selector");
        AllTimeTableState page;page.owner=out.menu.owner;page.underlay=frame;page.nation=selection.nation;page.relative=selection.relative;page.mask=uint8_t((1u<<selection.nation)|(selection.relative?0x20:0));page.has_data=true;page.club_names.clear();page.club_name_colors.clear();
        const auto country=countries[selection.nation];std::vector<int> physical;
        for(const auto& members:world.membership){const auto& league=world.catalogue.league(members.league);if(!league.active||league.country!=country)continue;
            for(auto club:members.clubs){const auto& facts=world.clubs.at(club);const auto& bytes=facts.season_stats.counters;const auto word=[&](size_t at){return uint16_t(bytes[at]|uint16_t(bytes[at+1])<<8);};physical.push_back(int(page.records.size()));out.clubs.push_back(club);page.records.push_back({word(30),word(32),word(34),word(36),word(38),word(40)});page.club_names.push_back(utf8_to_original(facts.name));page.club_name_colors.push_back(color(world,club,owner.manager));}
        }
        // Country captions remain meaningful when international scope removes national-team entities.
        static constexpr const char* country_names[]={"Deutschland","Spanien","England","Frankreich","Italien"};
        for(size_t c=0;c<5;++c){page.nation_names[c]=country_names[c];const auto nation=std::find_if(world.clubs.begin(),world.clubs.end(),[&](const auto& row){return row.second.kind==ClubKind::national_team&&row.second.country==countries[c];});if(nation!=world.clubs.end())page.nation_names[c]=nation->second.name;}
        const auto top=std::find_if(world.catalogue.leagues.begin(),world.catalogue.leagues.end(),[&](const auto& league){return league.active&&league.country==country&&league.local_index==0;});if(top==world.catalogue.leagues.end())throw FormatError("world12: selected all-time country is disabled");
        populate_alltime_rows(page,physical,uint32_t(top->rounds()));out.alltime=std::move(page);return out;
    }
    if(selection.page==6){
        if(!actor->match_state)throw FormatError("world12: manager streak records are unknown");
        const auto& club=world.clubs.at(owner.club);SeriesRecordsProjection page;page.manager_slot=owner.manager;page.runtime_team=int(projection.local_club(owner.club));page.league=int(league.value);page.explicit_league_size=definition.clubs;page.league_row=int(std::find(members.begin(),members.end(),owner.club)-members.begin());page.rounds=snapshot.league_rounds[league.value];page.club_name=utf8_to_original(club.name);page.gradient_text=out.menu.gradient_text;
        std::copy_n(club.season_stats.counters.begin(),21,page.current_streaks.begin());page.manager_best_streaks=actor->match_state->best_streaks;
        for(size_t col=0;col<3;++col){page.source_flags[col]=source.u8(layout::TableFlags+uint32_t(col));page.series_indices[col]=source.u8(layout::SeriesSelector+uint32_t(col));if(page.series_indices[col]>=3)throw FormatError("world12: invalid Source series selector");TableQuery query;query.venue=(page.source_flags[col]&3)==1?TableVenue::home:(page.source_flags[col]&3)==2?TableVenue::away:TableVenue::all;query.three_points=false;
            auto& table=page.tables[col];table.size=definition.clubs;for(const auto& row:compute_standings(world,snapshot.results,league,page.rounds,query)){const auto index=size_t(std::find(members.begin(),members.end(),row.club)-members.begin());table.wins[index]=row.wins;table.defeats[index]=row.defeats;table.goals_for[index]=row.goals_for;table.goals_against[index]=row.goals_against;table.points[index]=int8_t(row.points);table.rank_word[index]=uint16_t(row.games|((row.previous_rank-1)<<6)|((row.rank-1)<<11));table.form[index]=row.form;}
        }
        for(size_t i=0;i<8;++i){auto& record=page.records[i];record.score_byte=club.season_stats.counters[21+i];record.saved_date=club.season_stats.dates[i];if(record.score_byte){if(!club.season_stats.opponents[i].valid())throw FormatError("world12: nonempty record has no actual canonical opponent");record.opponent_name=utf8_to_original(world.clubs.at(club.season_stats.opponents[i]).name);}}
        out.series=std::move(page);return out;
    }
    throw FormatError("world12: unsupported statistics projection");
}
}
