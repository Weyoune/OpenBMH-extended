// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "game/world12/save.hpp"
namespace bmh::world12 {
struct ScheduledLeagueRound {
    LeagueAppointment appointment;
    std::vector<Fixture> matches;
    bool operator==(const ScheduledLeagueRound&)const=default;
};
struct ScheduledCupLeg {
    DomesticCup cup=DomesticCup::german_cup;
    uint8_t source_entry=0,leg=0;
    uint64_t revision=0;
    std::vector<Fixture> matches;
    bool operator==(const ScheduledCupLeg&)const=default;
};
struct DayTicket {
    uint16_t day=0;uint64_t revision=0;
    std::vector<ScheduledLeagueRound> leagues;
    std::vector<OtherAppointment> others;
    std::vector<ScheduledCupLeg> cups;
    std::vector<EuropeanGroup> european;
    bool operator==(const DayTicket&)const=default;
};
struct CompletedLeagueRound {LeagueId league;uint8_t round=0;std::vector<LeagueResult> results;};
// Scores come from the match layer. This coordinator never manufactures match
// outcomes or consumes either gameplay RNG to satisfy a calendar transition.
// Runtime admission never removes source appointments from the saved calendar.
// German indoor dates honor the saved option; English 0x4002 is League Cup.
std::vector<OtherAppointment> admitted_other_appointments(const CareerSnapshot&,uint16_t day);
std::optional<DayTicket> prepare_day(const CareerSnapshot&);
void commit_league_day(CareerSnapshot&,const DayTicket&,const std::vector<CompletedLeagueRound>&);
struct DayPresentation {std::vector<CupDrawNotice> cup_draws;uint64_t watcom_draws=0;};
DayPresentation commit_day(CareerSnapshot&,const DayTicket&,const std::vector<CompletedLeagueRound>&,
    const std::vector<CupLegResult>&,const std::vector<EuropeanResults>& european={},const LeImage* shared_rules=nullptr);
}
