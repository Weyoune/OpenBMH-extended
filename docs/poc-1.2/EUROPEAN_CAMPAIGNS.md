# Shared European club campaigns

The European campaign owner uses an independent `EuropeanCompetition` identity.
European dates never masquerade as a country's `DomesticCup`, indoor tournament
or national-team appointment. Germany and England share one campaign for each
European competition.

## Supplied Source programmes

The descriptors are read from the supplied German/shared rules, following their
successor and parallel links. Unreachable template entry 17 is not a played
Champions League stage.

| Competition | Source entries | Programme | Calendar appointments | Matches |
| --- | --- | --- | --- | --- |
| UEFA Cup | 15 | 64 clubs; six two-legged knockout rounds, including the final | 12 | 126 |
| Champions League | 16 → 31–34 → 18 → 19 | 16 qualifying clubs plus eight seeds; four groups of four; quarterfinals and semifinals over two legs; one final | 13 | 77 |
| Cup Winners' Cup | 20 → 21 | 32 clubs; four two-legged rounds; one final | 9 | 61 |

The shared calendar bits are `0x40`, `0x80` and `0x100`. The actual supplied
calendar has exactly these 12/13/9 appointments. Source fixture tables remain
external data; the four-club round schedule has no generated production fallback.
Initial campaign installation consumes the already drawn 64/24/32 fields and
adds no RNG calls. The Champions League's eight reserved seeds retain the
Source's backwards-read order.

## Canonical state and results

World schema 35 stores each campaign's original entrants, reachable scalar
stage descriptors, all drawn team slots, both Source score arrays, played-leg
cursors, reserved seeds, revision and winner/runner-up. All clubs use global
32-bit identities. Unplayed slots remain unknown and score arrays retain the
unplayed sentinel. An older world leaves the entire campaign owner absent;
a later European date cannot manufacture the missing history.

`prepare_european_groups` exposes current first/return-leg or parallel group
fixtures and their precise source/revision ownership. `current_european_members`
and `european_club_status` distinguish surviving clubs, eliminated entrants,
previous holders and completed winners. Elimination does not erase participation
or earlier results. `DayTicket.european` and `PreparedGroup.european` retain that
identity through calendar and weather preparation. Prepared European events use
snapshot `OBMH12SH`, including explicit pools presence and desk phase; older
snapshot formats remain readable.

`commit_european_results` rebuilds a bounded, identity-mapped Core workspace.
The existing `advance_cup`, `cup_tie` and group-table kernels own aggregate and
away-goal decisions, Source draws, group qualification and linked stages.
Unresolved knockout ties, incomplete parallel results, changed rules, stale
revisions, cycles and malformed score/cursor storage reject the transaction.
The manager season-round byte and two cup-title counts are projected through the
same Core transition. Schema 35 retains those native title words explicitly;
unknown older manager history remains unknown and cannot be reset by a result.

`play_competition_day` validates the complete league/domestic/European batch,
runs actual Core scoring and after-match calculations, and publishes results,
draws, player/manager changes, both seeds, date and one session revision together.
European groups retain separate Source descriptors on one shared minute clock,
including parallel Champions League groups. Synchronous live hooks are passed
through unchanged. The owner never chooses headless decline for its caller.
A cancelled presenter or changed session owner preserves the accepted career.

## Rating-only opponents

`AiMatchProjection::create_for_european` keeps real league squads in the existing
bounded roster projection. Actual non-league foreign clubs occupy the Original
rating-only physical slots and supply their stored strength byte to Core.
Original rating draws, no-player event handling and morale changes therefore
remain in use. No placeholder players or substitute results are generated.
National teams and unsupported non-league squad layouts remain separate owners.
The scheduled desk and gate-income owners use the same explicit projection.

## Validation scope and remaining gates

Public tests use authored clubs, players, fixture schedules and result inputs.
The campaign test follows all 264 Source matches, checks the 236 subsequent-draw
Watcom calls, and roundtrips every result boundary. The match test calculates
all 264 matches with the real Core engine for AI-only and human-manager cases,
including rating-only opponents, then saves and reloads each date. Explicit
headless decline is a test policy, not a production default.

These bounded programmes are not three played career seasons or native GUI
acceptance. National/indoor appointments, annual qualification and complete
season transition ownership remain separate gates. Presentation of the saved
European campaign in competition windows and final native live/calendar QA
must use this canonical state; an initial field alone is not a current bracket.
