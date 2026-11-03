// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/match_projection.hpp"
#include "game/world12/save.hpp"
#include "game/desk_actions.hpp"
namespace bmh::world12 {
struct FriendlyOffer {
    CareerSnapshot source_binding;
    uint64_t request=0;uint8_t actor=0;ClubId own,opponent;bool national=false,home=false;
    AiMatchProjection projection;
    FriendlyBookingQuote club_quote;
    std::optional<NationalFriendlyQuote> national_quote;
    WatcomRandom random{1};
};
void install_friendly_workspace(AiMatchProjection&,const CareerSnapshot&);
FriendlyOffer quote_friendly_booking(const CareerSnapshot&,uint8_t canonical_actor,ClubId opponent,bool home,uint64_t request,const GameState& reference);
struct FriendlyBookingStage {CareerSnapshot snapshot;FriendlyBookingResult result;uint64_t game_draws=0,watcom_draws=0;};
FriendlyBookingStage calculate_friendly_booking(const CareerSnapshot&,const FriendlyOffer&,uint16_t day,std::optional<uint8_t> approving_actor,std::optional<uint8_t> consenting_actor);
}
