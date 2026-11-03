# 1.2 AI post-match processing

`simulate_and_settle_ai_league_match` adds the existing `process_group` path to actual scoring: club records/streaks, morale and player development/card/absence handling. The scoring-only API remains available and does not silently acquire these effects.

`simulate_and_settle_ai_league_round` runs a complete scheduled league round as one Original match group on a shared minute clock. After every eligible fixture reaches full time, the group's post-match processing runs with the Source kickoff exclusions. It does not simulate and settle individual fixtures one after another. RNG counts belong to the whole round; per-fixture counters are not invented. Conditions share one date/month, referee table and presentation policy, while each fixture retains its supplied weather/referee/attendance.

## Persistent ownership

World schema version 4 stores the global signed development balance and trend rotation, together with each club's season statistics. The statistics representation separates the first 42 counter/record-score bytes, eight stable opponent club IDs and eight original 16-bit dates. Versions 1–3 retain initial defaults for these new fields.

The Original club table stores record opponents as 16-bit **club-name-pool offsets**, not stable club IDs. Projection builds a private offset map, including historical opponents that are not playing in the current group. Commit translates those offsets back to global IDs, including ID 0. Unknown offsets reject the entire transaction. No packed local reference is persisted.

The global development stream is copied from the typed world, not from an unrelated reference career. Stale balance/rotation rejects publication alongside the existing player/club/manager/RNG checks. Original signed 32-bit development arithmetic now uses explicit wrapping instead of C++ signed-overflow behavior. Ordinary legacy results remain unchanged.

Club-table bookkeeping consults explicit league classes for projected teams. It retains the Original's first-league/foreign-league totals restriction; mapping a lower-division club into local slot 0 must not make it a first-division club. Legacy careers keep their positional path.

Kick-off played-marker clearing is shared between the legacy driver and the projected scoring path. The bounded operation clears its projected participants; a future complete matchday owner still owns the global preparation barrier across all day's groups.

## Short squads and resumable kickoff

Regular projected matches share the Core kickoff operation used by the native
matchday driver. It performs man marking, checks both fit squads, charges each
human forfeiting side, increments the existing discipline high byte and restores
both squads. If both sides are short, the Source's later side determines the
award. The shared result writer sets the kickoff void mask and score; the minute
clock and post-match effects skip that fixture, retaining an end minute of zero.

The scheduled attendance transaction performs this operation before calculating
or booking crowds. A forfeited fixture has zero attendance and no gate draw or
income. A saved per-fixture `kickoff_disposition` records playable, home-forfeit
or away-forfeit, so the subsequent match clock never repeats marking or fines.
Development stream M appends that receipt byte after each fixture's attendance;
255 retains an older unknown receipt. The World schema and existing discipline
representation do not change. Old snapshots before attendance can proceed; an
older snapshot with already-booked attendance and no kickoff receipt cannot
safely resume the match through the Session owner and explicitly rejects.

Initialized runtime squads may contain zero through24 actual compact rows: Source
monthly contract expiry and retirement can release the last player. The master
minimum remains eleven, and the data loader rejects an already initialized
`WORLD12.JSON` as a new-game master. Player identities, compactness, empty-slot
flags, capacity and loan ownership remain strict. A short or empty runtime team
keeps its real lineup/market views and receives the same Source kickoff forfeit;
no replacement player or contract renewal is invented.

## Checks

- Synthetic home-, away- and both-short rounds compare the complete projected
  state, fines, restoration, exclusions and exact RNG against actual Core
  `kick_off` plus its clock and settlement. Saved attendance checks cover receipt
  roundtrip, replay rejection and no repeated fine or discipline increment.
- Five league contexts compare the combined scoring/settlement API against scoring followed by direct Original `process_group`, including the complete resulting world and both RNG seeds/call counts.
- Four full-round scenarios cover nine-, ten- and five-fixture rounds, deterministic repeats, original fixture identities, post-settlement snapshots and whole-round rollback for inconsistent late input.
- Direct Original positional club-table comparisons retain lower-division totals/streak behavior. Historical opponents outside the current match, wide IDs, unknown-offset rejection and signed development wrap are covered.
- Edge tests check a fifth-yellow suspension, one-match injury recovery, league appearance counting and cleared booking/played flags.
- Real-data `world12_ai_validate ... --settle` runs seven supplied-world fixtures and verifies their post-match state snapshots. Weather and attendance are explicit probe inputs.

## Remaining boundaries

Human tactics/finance/manager statistics, optional after-match presentation/awards, generated weather/referee/attendance selection and full-day/weekly/monthly ownership remain outside this API. Results colliding with the legacy unplayed sentinel explicitly reject settlement rather than silently skipping it.

The API does not advance a career date, update league round cursors/results, or provide a resumable complete GUI career. It supplies settled AI group state and outcomes for that future owner. Snapshot checks prove state preservation, not a complete played season or a 1.2 release.
