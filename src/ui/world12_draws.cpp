// SPDX-License-Identifier: GPL-3.0-only
#include "ui/world12_draws.hpp"
#include "ui/manager.hpp"
#include "game/cup.hpp"
#include <algorithm>
#include <array>
namespace bmh::ui {
using namespace bmh::world12;
namespace {
std::string english_domestic_round(const KnockoutStage& stage,uint8_t round,
                                 const LeImage& rules,const TextDb& texts){
    // English +1c indexes its own TEXT table. German menus instead use the
    // actual linked knockout programme: FA 2+5 rounds, League Cup 1+4+1.
    std::array<bool,kCupEntries> seen{};
    int total=0,first=-1;
    uint8_t entry=stage.cup==DomesticCup::fa_cup?0:25;
    for(;;){
        if(entry>=kCupEntries||seen[entry])throw FormatError("world12: cyclic English draw round programme");
        seen[entry]=true;const auto at=cup_entry_address(entry);
        if(!rules.contains(at+0x27)||!(rules.u8(at+0x14)&4)||rules.u16(at+0x18)!=(stage.cup==DomesticCup::fa_cup?0:11))throw FormatError("world12: English draw round programme has no matching knockout rules");
        const auto rounds=rules.u8(at+0x1b),next=rules.u8(at+0x16);
        if(!rounds||rounds>7)throw FormatError("world12: invalid English draw round count");
        if(entry==stage.source_entry){
            if(rounds!=stage.planned_rounds||next!=stage.next_entry)throw FormatError("world12: English draw round metadata changed");
            first=total;
        }
        total+=rounds;if(!next)break;entry=next;
    }
    if(first<0||!round||round>stage.planned_rounds)throw FormatError("world12: English draw round is outside its Source programme");
    const int ordinal=first+round,remaining=total-ordinal+1;
    const auto base=size_t((layout::CupDraw_FormatError_ExecutableField4-layout::TextPointerTable)/4);
    // German TEXTD has 1./2. Runde and the final four named knockout rounds.
    const auto index=base+size_t(remaining<=4?7-remaining:std::min(ordinal,2));
    if(index>=texts.strings.size())throw FormatError("world12: missing German domestic round text");
    auto label=original_to_utf8(texts[index]);
    if(remaining>4&&ordinal>2){
        const auto first_round=base+1;
        if(first_round>=texts.strings.size())throw FormatError("world12: missing German ordinal round text");
        label=original_to_utf8(texts[first_round]);
        if(label.empty()||label.front()!='1')throw FormatError("world12: German ordinal round template is unavailable");
        label.replace(0,1,std::to_string(ordinal));
    }
    return label;
}
}
World12CupDrawView world12_cup_draw_notice(const CareerSnapshot& state,const CupDrawNotice& notice,
    const GameState& reference,const LeImage& rules,const TextDb& texts){
    state.world.validate();validate_cup_programmes(state.world,state.cups);
    const auto stage=std::find_if(state.cups.begin(),state.cups.end(),[&](const auto& value){return value.cup==notice.cup&&value.source_entry==notice.source_entry;});
    if(stage==state.cups.end()||!notice.round||notice.round>stage->rounds.size()||notice.fixtures!=stage->rounds[size_t(notice.round-1)].fixtures||notice.fixtures.empty()||notice.fixtures.size()>32)
        throw FormatError("world12: cup draw notice has no matching saved actual draw");
    const auto entry=cup_entry_address(notice.source_entry);if(!rules.contains(entry+0x27)||rules.u8(entry+0x14)!=stage->flags)throw FormatError("world12: cup draw Source programme changed");
    GameState game=reference;if(state.world.economy&&state.world.economy->presentation_options){game.memory.add_block(layout::LeaguePresentationOptions,180);for(uint32_t i=0;i<180;++i)game.memory.set8(layout::LeaguePresentationOptions+i,(*state.world.economy->presentation_options)[i]);}
    game.manager_teams.clear();game.manager_records.assign(state.world.managers.size()*0xb0c,0);game.club_names={0};
    std::vector<ClubId> ids;for(const auto& fixture:notice.fixtures)for(auto club:{fixture.home,fixture.away}){if(std::find(ids.begin(),ids.end(),club)!=ids.end())throw FormatError("world12: actual cup draw repeats a participant");ids.push_back(club);}
    const auto participants=ids.size();for(const auto& manager:state.world.managers)if(std::find(ids.begin(),ids.end(),manager.club)==ids.end())ids.push_back(manager.club);
    game.club_ids.resize(ids.size());game.club_heap_78.assign(ids.size()*2,0);
    for(size_t i=0;i<ids.size();++i){const auto& club=state.world.clubs.at(ids[i]);const auto name=utf8_to_original(club.name);if(game.club_names.size()+name.size()+1>65535)throw FormatError("world12: cup draw actual name pool exceeds Source field");game.club_ids[i]=uint16_t(game.club_names.size());game.club_names.insert(game.club_names.end(),name.begin(),name.end());game.club_names.push_back(0);game.club_heap_78[i*2]=state.world.crests.at(club.crest).cell;}
    game.memory.add_block(entry,0x28);for(uint32_t byte=0;byte<0x28;++byte)game.memory.set8(entry+byte,rules.u8(entry+byte));
    game.memory.add_block(layout::CompetitionSelection,4);game.memory.set32(layout::CompetitionSelection,(state.world.catalogue.scope.club_play()?1u:0u)|(state.world.catalogue.scope.international?2u:0u));
    for(auto address:{layout::ActiveManager,layout::ManagerCount,layout::DeskPosition})game.memory.add_block(address,1);game.memory.set8(layout::ManagerCount,uint8_t(state.world.managers.size()));game.memory.set8(layout::ActiveManager,0);game.memory.set8(layout::DeskPosition,255);
    for(size_t i=0;i<state.world.managers.size();++i){const auto& manager=state.world.managers[i];game.manager_teams.push_back(int(std::find(ids.begin(),ids.end(),manager.club)-ids.begin()));const auto name=utf8_to_original(manager.name);std::copy_n(name.begin(),std::min<size_t>(20,name.size()),game.manager_records.begin()+i*0xb0c+0xa8);if(state.desktop&&state.desktop->active_seat==manager.seat){game.memory.set8(layout::ActiveManager,uint8_t(i));game.memory.set8(layout::DeskPosition,0);}}
    SeasonCupDrawEvent event;event.phase=SeasonCupDrawPhase::draw;event.entry_address=entry;event.competition=int(rules.u16(entry+0x18));event.round=notice.round-1;event.flags=stage->flags;event.count=participants;
    for(int round=0;round<event.round;++round)event.offset+=int(stage->rounds.front().fixtures.size()*2)>>round;
    for(size_t i=0;i<participants;++i)event.participants.push_back(uint16_t(i));
    World12CupDrawView out;out.projection=cup_draw_from_game(game,event,rules,texts);
    for(size_t i=0;i<participants;++i){const auto id=ids[i];const auto& club=state.world.clubs.at(id);const auto& crest=state.world.crests.at(club.crest);auto& row=out.projection.clubs.at(i);row.badge_atlas=crest.path;row.badge_selector=crest.cell;row.league_tier=club.league.valid()?state.world.catalogue.league(club.league).local_index:255;row.country=0;out.clubs[uint16_t(i)]=id;}
    if(notice.cup==DomesticCup::fa_cup||notice.cup==DomesticCup::league_cup)out.projection.heading=english_domestic_round(*stage,notice.round,rules,texts);
    if(notice.cup==DomesticCup::fa_cup)out.projection.footer="FA CUP";
    if(notice.cup==DomesticCup::league_cup)out.projection.footer="KOKA KOLA CUP";
    return out;
}
}
