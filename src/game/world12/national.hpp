// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/world.hpp"
#include "game/state.hpp"
#include "game/random.hpp"

namespace bmh::world12 {
struct NationalCandidate {
    PlayerId player;
    ClubId club;
    uint8_t row=0;
    uint16_t flags=0;
    bool operator==(const NationalCandidate&)const=default;
};
// Home league squads first, then expatriates in canonical league/row order.
// Listed players remain eligible at their owning club;
// free pools are outside the Original nomination scan.
std::array<NationalCandidate,20> national_nomination(const World&,Country);
struct NationalNominationOutcome {
    ClubId team;
    std::optional<uint8_t> coach;
    uint64_t watcom_draws=0;
};
// Global selection and shared Original board/lineup calculations. The event
// owner separately presents a coached desk and consumes the appointment.
NationalNominationOutcome nominate_national_squad(World&,Country,uint32_t date,
    const GameState& reference,WatcomRandom&);
void validate_national_squads(const World&);
}
