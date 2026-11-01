// SPDX-License-Identifier: GPL-3.0-only
#include "game/world12/save.hpp"
#include "game/world12/codec.hpp"
#include <bit>
#include <algorithm>
#include <set>
namespace bmh::world12 {
void CareerSnapshot::validate()const{
    if(desktop){
        if(!world.settings||desktop->managers.size()!=world.managers.size()||desktop->managers.empty()||desktop->managers.size()>4||desktop->autosave_month>12||desktop->autosave_months>255)
            throw FormatError("world12: invalid saved desktop runtime");
        std::array<bool,4> seen{};
        for(const auto& desk:desktop->managers){
            if(desk.seat>=4||seen[desk.seat]||std::none_of(world.managers.begin(),world.managers.end(),[&](const auto& m){return m.seat==desk.seat;}))
                throw FormatError("world12: invalid saved desktop manager");
            seen[desk.seat]=true;
        }
        if(desktop->active_seat&&(*desktop->active_seat>=4||!seen[*desktop->active_seat]))throw FormatError("world12: invalid active desktop manager");
        if(desktop->autosave_pending&&!desktop->active_seat)throw FormatError("world12: pending autosave has no active desktop");
        if(desktop->initial_event){const auto& event=*desktop->initial_event;
            if(date||season||!calendar||calendar->next_day||event.matchday_number>10000||event.manager_order.size()!=world.managers.size()||event.manager_position>=event.manager_order.size()||!event.pools_open||!calendar->schedule.leagues_on(0).empty()||!calendar->schedule.others_on(0).empty())throw FormatError("world12: invalid initial desktop event");
            std::array<bool,4> ordered{};for(auto seat:event.manager_order){if(seat>=4||!seen[seat]||ordered[seat])throw FormatError("world12: invalid initial desk order");ordered[seat]=true;}
            if(desktop->active_seat&&*desktop->active_seat!=event.manager_order[event.manager_position])throw FormatError("world12: initial desk/order mismatch");
            if(world.transfer_market)for(size_t row=40;row<44;++row)if(world.transfer_market->rows[row]&&std::any_of(world.transfer_market->rows[row]->bids.begin(),world.transfer_market->rows[row]->bids.end(),[](const auto& bid){return bid.bidder.valid();})&&(!event.offer||event.offer->row!=row))throw FormatError("world12: pending initial offer has no decision owner");
            if(event.offer){const auto& offer=*event.offer;
                if(offer.seat!=event.manager_order[event.manager_position]||offer.row<40||offer.row>43||!world.transfer_market||!world.transfer_market->rows[offer.row]||!world.transfer_market->rows[offer.row]->bids[0].bidder.valid()||(!offer.salary_only&&offer.warned))throw FormatError("world12: invalid initial offer notice");
                const auto manager=std::find_if(world.managers.begin(),world.managers.end(),[&](const auto& m){return m.seat==offer.seat;});
                if(world.transfer_market->rows[offer.row]->seller!=manager->club||!world.transfer_market->rows[offer.row]->human_seller)throw FormatError("world12: initial offer seller mismatch");
            }
        }
    }
    if(transfers){
        for(size_t seat=0;seat<4;++seat){const auto& rows=transfers->history[seat];if(rows.size()>30)throw FormatError("world12: transfer history exceeds Source capacity");for(const auto& row:rows)if(!world.players.contains(row.player)||(row.other.valid()&&!world.clubs.contains(row.other)))throw FormatError("world12: unknown transfer history actor");}
        if(transfers->loans.size()>12)throw FormatError("world12: owner loan snapshots exceed Source capacity");std::set<PlayerId> players;std::set<uint8_t> slots;std::array<int,4> counts{};
        for(const auto& contract:transfers->loans){if(contract.source_slot&&(*contract.source_slot>=12||!slots.insert(*contract.source_slot).second))throw FormatError("world12: invalid owner loan slot");const auto loan=std::find_if(world.loans.begin(),world.loans.end(),[&](const auto& l){return l.player==contract.player;});const auto owner=std::find_if(world.managers.begin(),world.managers.end(),[&](const auto& m){return m.seat==contract.manager;});
            if(contract.manager>=4||!players.insert(contract.player).second||loan==world.loans.end()||owner==world.managers.end()||owner->club!=loan->owner||++counts[contract.manager]>3)throw FormatError("world12: invalid owner loan contract");
            for(auto id:{contract.mark,contract.marked_by})if(id.valid()&&!world.players.contains(id))throw FormatError("world12: unknown loan-board player reference");for(size_t b=0x44;b<0x48;++b)if(contract.board[b])throw FormatError("world12: packed reference in loan contract backing");}
    }
    world.validate();results.validate(world.catalogue);validate_cup_programmes(world,cups);for(size_t i=0;i<kLeagueSlots;++i)if(league_rounds[i]>world.catalogue.leagues[i].rounds())throw FormatError("world12: round cursor past season end");
    if(calendar){calendar->schedule.validate(world.catalogue);if(calendar->next_day>calendar->schedule.days)throw FormatError("world12: invalid calendar cursor");std::array<uint8_t,kLeagueSlots> expected{};for(const auto&a:calendar->schedule.leagues)if(a.day<calendar->next_day)expected[a.league.value]=a.round;if(expected!=league_rounds)throw FormatError("world12: saved rounds disagree with committed calendar days");for(uint32_t l=0;l<kLeagueSlots;++l)for(int r=1;r<=world.catalogue.leagues[l].rounds();++r)for(int m=0;m<world.catalogue.leagues[l].clubs/2;++m)if(results.result(world.catalogue,{l},r,m).played!=(r<=league_rounds[l]))throw FormatError("world12: saved scores disagree with committed rounds");}
}
Savegame make_save(const CareerSnapshot& snapshot){snapshot.validate();Savegame save;save.version="O1.2";save.key=0;save.checksum_ok=true;save.stream={'O','B','M','H','1','2','S','A'};
    const auto put=[&](uint64_t v,int width=4){for(int i=0;i<width;++i)save.stream.push_back(uint8_t(v>>(i*8)));};auto world=encode_world(snapshot.world);put(world.size());put(snapshot.date);put(snapshot.season);put(snapshot.watcom_seed);put(std::bit_cast<uint32_t>(snapshot.game_seed));save.stream.insert(save.stream.end(),world.begin(),world.end());for(auto round:snapshot.league_rounds)put(round,1);
    for(const auto&league:snapshot.results.leagues)for(const auto&r:league){put(r.played?1:0,1);put(r.home,1);put(r.away,1);}
    put(snapshot.calendar?1:0,1);if(snapshot.calendar){const auto&cursor=*snapshot.calendar;put(cursor.schedule.days,2);put(cursor.next_day,2);put(cursor.revision,8);put(cursor.schedule.leagues.size());put(cursor.schedule.others.size());for(const auto&a:cursor.schedule.leagues){put(a.day,2);put(a.league.value);put(a.round,1);}for(const auto&a:cursor.schedule.others){put(a.day,2);put(uint8_t(a.domain),1);put(a.source_entry,2);}}
    put(snapshot.cups.size(),2);
    for(const auto& stage:snapshot.cups){
        put(uint8_t(stage.cup),1);put(stage.source_entry,1);put(stage.flags,1);put(stage.planned_rounds,1);put(stage.next_entry,1);put(stage.seeded.size(),1);for(auto club:stage.seeded)put(club.value);put(stage.revision,8);put(stage.rounds.size(),1);
        for(const auto& round:stage.rounds){
            put(round.fixtures.size(),1);for(const auto& fixture:round.fixtures){put(fixture.home.value);put(fixture.away.value);}
            for(const auto* scores:{&round.first_leg,&round.second_leg}){put(scores->size(),1);for(auto score:*scores)put(score,2);}
        }
    }
    put(snapshot.desktop?1:0,1);
    if(snapshot.desktop){const auto& d=*snapshot.desktop;put(d.managers.size(),1);put(d.active_seat?*d.active_seat:255,1);put(d.autosave_month,2);put(d.autosave_months,2);put(d.autosave_pending?1:0,1);
        for(const auto& m:d.managers){put(m.seat,1);put(m.prestige,1);put(m.display_word,2);for(const auto& row:m.layout)for(auto b:row)put(b,1);}
        put(d.initial_event?1:0,1);if(d.initial_event){const auto& e=*d.initial_event;put(e.matchday_number,2);put(e.manager_position,1);put(e.manager_order.size(),1);for(auto seat:e.manager_order)put(seat,1);put(e.pools_open?1:0,1);put(e.offer?1:0,1);if(e.offer){put(e.offer->seat,1);put(e.offer->row,1);put(e.offer->salary_only?1:0,1);put(e.offer->warned?1:0,1);}}
    }
    put(snapshot.transfers?1:0,1);if(snapshot.transfers){const auto& t=*snapshot.transfers;for(auto balance:t.balance)put(std::bit_cast<uint32_t>(balance));for(const auto& rows:t.history){put(rows.size(),1);for(const auto& row:rows){put(row.player.value);put(row.other.value);put(row.source_word);}}put(t.loans.size(),1);for(const auto& loan:t.loans){put(loan.player.value);put(loan.manager,1);for(auto b:loan.board)put(b,1);put(loan.mark.value);put(loan.marked_by.value);put(loan.source_slot?*loan.source_slot:255,1);}}
    for(auto byte:save.stream)save.checksum=uint8_t(save.checksum+byte);return save;
}
CareerSnapshot read_save(const Savegame& save){
    if(save.version!="O1.2"||!save.checksum_ok)throw FormatError("world12: incompatible or damaged save");const auto&raw=save.stream;if(raw.size()<28||(std::string(raw.begin(),raw.begin()+8)!="OBMH12S4"&&std::string(raw.begin(),raw.begin()+8)!="OBMH12S5"&&std::string(raw.begin(),raw.begin()+8)!="OBMH12S6"&&std::string(raw.begin(),raw.begin()+8)!="OBMH12S7"&&std::string(raw.begin(),raw.begin()+8)!="OBMH12S8"&&std::string(raw.begin(),raw.begin()+8)!="OBMH12S9"&&std::string(raw.begin(),raw.begin()+8)!="OBMH12SA"))throw FormatError("world12: unknown development save schema");size_t at=8;
    const auto get=[&](int width=4){if(at+size_t(width)>raw.size())throw FormatError("world12: truncated save field");uint64_t value=0;for(int i=0;i<width;++i)value|=uint64_t(raw[at++])<<(8*i);return value;};
    auto size=get();if(size>32*1024*1024)throw FormatError("world12: world stream too large");CareerSnapshot state;state.date=uint32_t(get());state.season=uint32_t(get());state.watcom_seed=uint32_t(get());state.game_seed=std::bit_cast<int32_t>(uint32_t(get()));if(at+size>raw.size())throw FormatError("world12: truncated world stream");state.world=decode_world(Bytes(raw.begin()+at,raw.begin()+at+size));at+=size;for(auto&round:state.league_rounds)round=uint8_t(get(1));state.results=SeasonResults::empty(state.world.catalogue);
    for(auto&league:state.results.leagues)for(auto&r:league){auto played=get(1);if(played>1)throw FormatError("world12: invalid result flag");r={played!=0,uint8_t(get(1)),uint8_t(get(1))};}
    auto present=get(1);if(present>1)throw FormatError("world12: invalid calendar flag");if(present){CalendarCursor cursor;cursor.schedule.days=uint16_t(get(2));cursor.next_day=uint16_t(get(2));cursor.revision=get(8);auto leagues=get(),others=get();if(leagues>1000||others>4096)throw FormatError("world12: calendar entries exceed limits");for(uint64_t i=0;i<leagues;++i)cursor.schedule.leagues.push_back({uint16_t(get(2)),{uint32_t(get())},uint8_t(get(1))});for(uint64_t i=0;i<others;++i)cursor.schedule.others.push_back({uint16_t(get(2)),CalendarDomain(get(1)),uint16_t(get(2))});state.calendar=std::move(cursor);}
    const auto cup_count=get(2);if(cup_count>180)throw FormatError("world12: too many domestic cup owners");
    for(uint64_t c=0;c<cup_count;++c){
        KnockoutStage stage;stage.cup=DomesticCup(get(1));stage.source_entry=uint8_t(get(1));stage.flags=uint8_t(get(1));stage.planned_rounds=uint8_t(get(1));stage.next_entry=uint8_t(get(1));const auto seeds=get(1);if(seeds>128)throw FormatError("world12: seeded cup count exceeds limits");for(uint64_t i=0;i<seeds;++i)stage.seeded.push_back({uint32_t(get())});stage.revision=get(8);
        const auto rounds=get(1);if(rounds>7)throw FormatError("world12: cup round count exceeds limits");
        for(uint64_t r=0;r<rounds;++r){
            CupRound round;const auto matches=get(1);if(matches>64)throw FormatError("world12: cup fixture count exceeds limits");
            for(uint64_t m=0;m<matches;++m)round.fixtures.push_back({{uint32_t(get())},{uint32_t(get())}});
            for(auto* scores:{&round.first_leg,&round.second_leg}){const auto count=get(1);if(count>64)throw FormatError("world12: cup score count exceeds limits");for(uint64_t m=0;m<count;++m)scores->push_back(uint16_t(get(2)));}
            stage.rounds.push_back(std::move(round));
        }
        state.cups.push_back(std::move(stage));
    }
    if(raw[7]>='5'){
        const auto present=get(1);if(present>1)throw FormatError("world12: invalid desktop presence flag");
        if(present){DesktopRuntime desktop;const auto count=get(1),active=get(1);if(count<1||count>4)throw FormatError("world12: invalid desktop count");
            if(active!=255)desktop.active_seat=uint8_t(active);desktop.autosave_month=uint16_t(get(2));desktop.autosave_months=uint16_t(get(2));const auto pending=get(1);if(pending>1)throw FormatError("world12: invalid pending autosave flag");desktop.autosave_pending=pending!=0;
            for(uint64_t i=0;i<count;++i){ManagerDesktop m;m.seat=uint8_t(get(1));m.prestige=uint8_t(get(1));if(raw[7]>='7')m.display_word=uint16_t(get(2));for(auto& row:m.layout)for(auto& b:row)b=uint8_t(get(1));desktop.managers.push_back(m);}
            if(raw[7]=='6'||raw[7]>='7'){const auto present=get(1);if(present>1)throw FormatError("world12: invalid initial event flag");if(present){InitialDeskEvent event;event.matchday_number=uint16_t(get(2));if(raw[7]>='8')event.manager_position=uint8_t(get(1));const auto seats=get(1);if(seats<1||seats>4)throw FormatError("world12: invalid initial event order count");for(uint64_t i=0;i<seats;++i)event.manager_order.push_back(uint8_t(get(1)));const auto pools=get(1),offer=get(1);if(pools>1||offer>1)throw FormatError("world12: invalid initial event booleans");event.pools_open=pools!=0;if(offer){PendingDeskOffer notice;notice.seat=uint8_t(get(1));notice.row=uint8_t(get(1));const auto salary=get(1),warned=get(1);if(salary>1||warned>1)throw FormatError("world12: invalid offer booleans");notice.salary_only=salary!=0;notice.warned=warned!=0;event.offer=notice;}desktop.initial_event=std::move(event);}}
            state.desktop=std::move(desktop);}
    }
    if(raw[7]>='9'){const auto present=get(1);if(present>1)throw FormatError("world12: invalid transfer runtime flag");if(present){TransferRuntime t;for(auto& balance:t.balance)balance=std::bit_cast<int32_t>(uint32_t(get()));for(auto& rows:t.history){const auto count=get(1);if(count>30)throw FormatError("world12: invalid history count");for(uint64_t n=0;n<count;++n)rows.push_back({{uint32_t(get())},{uint32_t(get())},uint32_t(get())});}const auto count=get(1);if(count>12)throw FormatError("world12: invalid owner loan count");for(uint64_t n=0;n<count;++n){LoanContract loan;loan.player={uint32_t(get())};loan.manager=uint8_t(get(1));for(auto& b:loan.board)b=uint8_t(get(1));loan.mark={uint32_t(get())};loan.marked_by={uint32_t(get())};if(raw[7]=='A'){const auto slot=get(1);if(slot!=255)loan.source_slot=uint8_t(slot);}t.loans.push_back(loan);}state.transfers=std::move(t);}}
    if(at!=raw.size())throw FormatError("world12: trailing save bytes");state.validate();return state;
}
}
