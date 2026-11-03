// SPDX-License-Identifier: GPL-3.0-only
#include "game/world12/world.hpp"
#include "game/world12/national.hpp"
#include <set>
#include <algorithm>
#include <stdexcept>
namespace bmh::world12 {
void validate_manager_tactics(const ManagerTactics& tactics) {
    if(tactics.backing)for(const auto& row:tactics.backing->scalar_rows)
        for(size_t at:{2u,3u,4u,5u,0x3fu,0x41u,0x42u,0x44u,0x45u,0x46u,0x47u})
            if(row[at])throw std::invalid_argument("world12: duplicate scalar/reference tactical encoding");
    if(tactics.automatic_formation>8)throw std::invalid_argument("world12: invalid automatic formation selector");
    for(size_t i=0;i<tactics.rows.size();++i) {
        const auto& row=tactics.rows[i];
        if((row.partner!=255 && row.partner>=24) ||
            (i<11 && (row.depth>19 || row.lane>8)))
            throw std::invalid_argument("world12: invalid tactical position or partner row");
    }
}
void World::validate(bool complete)const {
    validate_annual_title_archive(*this);
    for (const auto& [identity, counters] : player_statistics)
        if (!clubs.contains(identity.first) || !players.contains(identity.second))
            throw std::invalid_argument("world12: unknown global statistics identity");
    if(economy){for(const auto& m:economy->managers)if(m.monthly&&m.monthly->history_count>12)throw std::invalid_argument("world12: invalid monthly history cursor");if(!settings||economy->week>11||economy->messages.size()>40||economy->recipient_mask&~0x8000000fu)throw std::invalid_argument("world12: invalid career economy");
        if(economy->pools) {if(!economy->pools->appointment&&std::any_of(economy->pools->managers.begin(),economy->pools->managers.end(),[](const auto& c){return c.stake!=0;}))throw std::invalid_argument("world12: pools stakes have no appointment owner");for(const auto& c:economy->pools->managers)if((c.stake&&(c.stake<10000||c.stake>400000))||c.correct>11||std::any_of(c.tips.begin(),c.tips.end(),[](auto v){return v>2;}))throw std::invalid_argument("world12: invalid saved pools coupon");if(std::any_of(economy->pools->previous_outcomes.begin(),economy->pools->previous_outcomes.end(),[](auto v){return v>2;}))throw std::invalid_argument("world12: invalid previous pools outcomes");}
        for(const auto& m:economy->managers)if(m.bank_robbery_month==0xffffffffu)throw std::invalid_argument("world12: invalid bank robbery month sentinel");
        for(const auto& m:economy->managers)if(m.weekly_count>52)throw std::invalid_argument("world12: invalid weekly history cursor");
        for(const auto& m:economy->messages)if((m.recipients&~0x8000000fu)||(m.read_by&~15u))throw std::invalid_argument("world12: invalid message recipients");}
    if(markets && (!players_initialized || markets->first_default_name>1))throw std::invalid_argument("world12: invalid career market state");
    if(settings && (!free_players || settings->mode<1 || settings->mode>3 || settings->difficulty>9 || settings->duration>5 || settings->planned_managers<1 || settings->planned_managers>4 || managers.size()>settings->planned_managers))throw std::invalid_argument("world12: invalid career settings");
    for(const auto& manager:managers)if(manager.career) {
        if(manager.career->camp_focus&&std::any_of(manager.career->camp_focus->begin(),manager.career->camp_focus->end(),[](auto value){return value>15;}))throw std::invalid_argument("world12: camp focus outside 0..15");
        if(!settings)throw std::invalid_argument("world12: initial manager state requires career settings");
        if(manager.career->season_services){const auto& season=*manager.career->season_services;if(season.strength_history_count>38)throw std::invalid_argument("world12: invalid seasonal history cursor");if(manager.match_state&&manager.match_state->designated_takers)for(size_t b=8;b<12;++b)if(season.advertising_contracts[16][b])throw std::invalid_argument("world12: packed taker reference in sponsor backing");
            for(size_t i:{0u,14u,15u})for(size_t b:{0u,1u,2u,3u,5u})if(season.advertising_contracts[i][b])throw std::invalid_argument("world12: duplicate initial sponsor scalar encoding");}
        std::set<PlayerId> attempts;for(auto player:manager.career->poach_attempts)if(player.valid()&&(!players.contains(player)||!attempts.insert(player).second))throw std::invalid_argument("world12: invalid poaching history identity");
        for(const auto& youth:manager.career->environment.youth)if(youth.position>11||youth.league>2)throw std::invalid_argument("world12: invalid youth team state");
        for(auto value:manager.career->environment.monthly)if(value<0)throw std::invalid_argument("world12: invalid negative environment budget");
    }
    catalogue.validate();if(trend_rotation>2)throw std::invalid_argument("world12: invalid development trend rotation");std::set<ClubId> league_clubs;std::set<PlayerId> rostered;
    for(size_t i=0;i<membership.size();++i){const auto& m=membership[i];const auto& league=catalogue.leagues[i];if(m.league!=league.id)throw std::invalid_argument("world12: league membership identity mismatch");if(complete&&m.clubs.size()!=(league.active?league.clubs:0))throw std::invalid_argument("world12: incomplete league membership");if(m.clubs.size()>(league.active?league.clubs:0))throw std::invalid_argument("world12: league capacity exceeded");
        for(auto id:m.clubs){const auto found=clubs.find(id);if(found==clubs.end()||!league_clubs.insert(id).second)throw std::invalid_argument("world12: missing/duplicate league club");const auto& club=found->second;if(club.kind!=ClubKind::club||club.country!=league.country||club.league!=league.id)throw std::invalid_argument("world12: club assigned to the wrong country/league");}
    }
    for(const auto&[id,record]:players){const auto& p=record.attributes;if(!id.valid()||id!=record.id||p.name.empty()||p.name.size()>255||p.form()>99||p.technique()>99||p.condition()>99||p.nationality()+((record.bonus&128)?32:0)>=62)throw std::invalid_argument("world12: invalid player");}
    for(const auto&[id,club]:clubs){if((club.kind==ClubKind::club&&!catalogue.scope.club_enabled(club.country,club.league))||(club.kind==ClubKind::national_team&&!catalogue.scope.international))throw std::invalid_argument("world12: club/national team belongs to a disabled game scope");if(!id.valid()||id!=club.id||club.name.empty()||club.name.size()>255||uint8_t(club.country)>=62||!club.crest.valid()||!crests.contains(club.crest)||uint8_t(club.kind)>1)throw std::invalid_argument("world12: invalid club");for(auto opponent:club.season_stats.opponents)if(opponent.valid()&&(!clubs.contains(opponent)||clubs.at(opponent).kind!=ClubKind::club))throw std::invalid_argument("world12: missing/non-club statistics opponent");if(club.morale>63)throw std::invalid_argument("world12: invalid club morale");if(club.league.valid()&&!league_clubs.contains(id))throw std::invalid_argument("world12: club league absent from membership");unsigned occupied=0;bool empty=false;for(const auto& slot:club.roster){if(slot.flags&~uint16_t(15))throw std::invalid_argument("world12: unknown roster flags");if(slot.empty()){if(slot.flags)throw std::invalid_argument("world12: flags on an empty roster slot");empty=true;continue;}if(empty||!players.contains(slot.player)||!rostered.insert(slot.player).second)throw std::invalid_argument("world12: missing, duplicate or non-compact roster player");++occupied;}if(complete&&club.league.valid()&&occupied<11)throw std::invalid_argument("world12: league club below minimum squad size");if(!club.league.valid()&&occupied)throw std::invalid_argument("world12: non-league club has a league roster");}
    if(amateur_slots)for(size_t country_index=0;country_index<2;++country_index) {
        const auto country=country_index==0?Country::germany:Country::england;
        std::set<ClubId> expected,actual;
        for(const auto& [id,club]:clubs)if(club.kind==ClubKind::club&&club.country==country&&!club.league.valid())expected.insert(id);
        for(auto id:(*amateur_slots)[country_index])if(!actual.insert(id).second)throw std::invalid_argument("world12: repeated canonical amateur slot");
        if(actual!=expected)throw std::invalid_argument("world12: canonical amateur slots omit or misidentify clubs");
    }
    for(const auto&[id,crest]:crests){const auto& path=crest.path;if(!id.valid()||crest.cell>=8||path.empty()||path.find("..")!=std::string::npos||path.find(':')!=std::string::npos||path[0]=='/'||path.find('\\')!=std::string::npos)throw std::invalid_argument("world12: invalid crest resource");}
    if(free_players) {
        if(!markets)throw std::invalid_argument("world12: free-player phase requires career markets");
        const auto validate_pool=[&](const auto& pool){for(auto player:pool){if(!player.valid())continue;if(!players.contains(player)||!rostered.insert(player).second)throw std::invalid_argument("world12: missing or repeated free player");}};
        validate_pool(free_players->domestic);validate_pool(free_players->foreign);
    }
    validate_european_campaigns(*this);
    validate_indoor_season(*this);
    if(initial_competitions){
        if(!settings)throw std::invalid_argument("world12: initial competitions lack career settings");
        for(const auto& manager:managers)if(!manager.career||!manager.career->season_records||!manager.tactics||!manager.finance||!manager.match_state)throw std::invalid_argument("world12: prepared competitions lack complete manager state");
        for(const auto* titles:{&initial_competitions->champions,&initial_competitions->domestic_holders}){std::optional<Country> previous;for(const auto& [country,id]:*titles){if((previous&&country<=*previous)||!catalogue.selectable(country)||!clubs.contains(id)||clubs.at(id).country!=country||clubs.at(id).kind!=ClubKind::club)throw std::invalid_argument("world12: invalid domestic title identity/order");previous=country;}}
        if(initial_competitions->manager_services_complete)for(const auto& manager:managers)if(!manager.career->season_services)throw std::invalid_argument("world12: completed initial services lack manager records");
        const size_t sizes[]={64,24,32};std::set<ClubId> participants;
        for(size_t k=0;k<3;++k){const auto& field=initial_competitions->fields[k];if(field.size()!=sizes[k])throw std::invalid_argument("world12: incomplete European field");
            for(auto id:field)if(!clubs.contains(id)||clubs.at(id).kind!=ClubKind::club||!participants.insert(id).second)throw std::invalid_argument("world12: invalid/repeated European club");
            std::set<ClubId> qualified;for(auto id:initial_competitions->qualified[k])if(!qualified.insert(id).second||std::find(field.begin(),field.end(),id)==field.end())throw std::invalid_argument("world12: repeated/missing European qualifier");}
        for(auto id:initial_competitions->holders)if(!participants.contains(id))throw std::invalid_argument("world12: European holder absent from fields");
        if(initial_competitions->national_field.size()!=(catalogue.scope.international?4u:0u))throw std::invalid_argument("world12: invalid initial national field size");
        std::set<ClubId> nations;for(auto id:initial_competitions->national_field)if(!clubs.contains(id)||clubs.at(id).kind!=ClubKind::national_team||!nations.insert(id).second)throw std::invalid_argument("world12: invalid initial national identity");
    }
    if(transfer_market){
        for(auto id:transfer_market->last_entry40_bidders)if(id.valid()&&(!transfer_market->entry40_history_known||!clubs.contains(id)||clubs.at(id).kind!=ClubKind::club))throw std::invalid_argument("world12: unqualified special bid history");
        if(!initial_competitions||!initial_competitions->manager_services_complete||transfer_market->list_toggle>1)throw std::invalid_argument("world12: invalid initial market state");
        std::set<PlayerId> listed;
        for(const auto& row:transfer_market->rows)if(row){if(!players.contains(row->player)||!listed.insert(row->player).second)throw std::invalid_argument("world12: missing or duplicate market player");
            if(row->seller.valid()){if(!clubs.contains(row->seller)||clubs.at(row->seller).kind!=ClubKind::club)throw std::invalid_argument("world12: missing market seller");bool found=false;for(const auto& slot:clubs.at(row->seller).roster)found|=slot.player==row->player;if(!found)throw std::invalid_argument("world12: market player is not at seller");}
            else {bool free=false;for(auto id:free_players->domestic)free|=id==row->player;for(auto id:free_players->foreign)free|=id==row->player;if(!free||row->human_seller)throw std::invalid_argument("world12: market free agent is unavailable");}
            for(const auto& bid:row->bids)if(bid.bidder.valid()&&(!clubs.contains(bid.bidder)||clubs.at(bid.bidder).kind!=ClubKind::club))throw std::invalid_argument("world12: missing market bidder");
        }
    }
    if(loans.size()>200)throw std::invalid_argument("world12: loan ledger exceeds capacity");
    std::set<uint16_t> loan_slots;
    std::set<PlayerId> loaned;for(const auto& loan:loans){if(loan.source_slot&&(*loan.source_slot>=200||!loan_slots.insert(*loan.source_slot).second))throw std::invalid_argument("world12: invalid loan ledger slot");if(!players.contains(loan.player)||!clubs.contains(loan.owner)||!clubs.contains(loan.borrower)||loan.owner==loan.borrower||!loan.months||!loaned.insert(loan.player).second)throw std::invalid_argument("world12: invalid loan");bool at_borrower=false;for(const auto& slot:clubs.at(loan.borrower).roster)at_borrower|=slot.player==loan.player;if(!at_borrower)throw std::invalid_argument("world12: loan player is not at borrower");}
    if(managers.size()>4)throw std::invalid_argument("world12: too many managers");std::set<uint8_t> seats,portraits;std::set<ClubId> owned;
    for(const auto& manager:managers){if(manager.match_state){const auto& m=*manager.match_state;bool ended=false;
for(size_t i=0;i<12;i+=2){const auto a=m.recent_fixtures[i],b=m.recent_fixtures[i+1];if(a.valid()!=b.valid()||(a.valid()&&(ended||a==b||!clubs.contains(a)||!clubs.contains(b))))throw std::invalid_argument("world12: invalid recent manager fixture");if(!a.valid())ended=true;}
for(const auto& [club,scores]:m.head_to_head)if(!clubs.contains(club))throw std::invalid_argument("world12: missing manager opponent history club");
for(auto player:m.takers)if(player.valid()&&(!m.designated_takers||!players.contains(player)))throw std::invalid_argument("world12: invalid manager designated player");}if(manager.tactics){validate_manager_tactics(*manager.tactics);if(manager.tactics->backing){
for(const auto* refs:{&manager.tactics->backing->marks,&manager.tactics->backing->marked_by})for(auto id:*refs)if(id.valid()&&!players.contains(id))throw std::invalid_argument("world12: missing tactical player reference");}}const auto it=clubs.find(manager.club);if(manager.seat>=4||manager.name.empty()||manager.name.size()>20||manager.portrait>=12||!seats.insert(manager.seat).second||!portraits.insert(manager.portrait).second||!owned.insert(manager.club).second||it==clubs.end()||!it->second.league.valid()||!catalogue.selectable(it->second.country)||catalogue.league(manager.start_league).country!=it->second.country)throw std::invalid_argument("world12: invalid manager selection");}
    for(const auto& manager:managers)if(manager.match_state)validate_manager_attendance(*this,*manager.match_state);
    validate_national_squads(*this);
    validate_national_campaigns(*this);
}
std::vector<ClubId> World::selectable_clubs(Country country,LeagueId id)const {if(!catalogue.selectable(country)||catalogue.league(id).country!=country)throw std::invalid_argument("world12: unavailable manager country/league");return membership.at(id.value).clubs;}
Country World::manager_country(uint8_t seat)const {for(const auto& manager:managers)if(manager.seat==seat)return clubs.at(manager.club).country;throw std::out_of_range("world12: manager seat is not occupied");}
Country World::national_coach_country(uint8_t seat)const {if(!catalogue.scope.international||managers.size()!=1||managers[0].seat!=seat)throw std::invalid_argument("world12: original single-manager national coach restriction");return manager_country(seat);}
}
