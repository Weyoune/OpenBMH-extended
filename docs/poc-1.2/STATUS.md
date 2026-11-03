# OpenBMH-Extended 1.0 Beta status

Published Beta 1 is `1.0.0-beta.1`; the next implementation milestone is
`1.0.0-beta.3` on `main`. A version string alone is not a published release.
The old `world12` and `dual-classic-2` names identify the shared data model,
not a different public product version. Classic remains independent.

## Supported scope

| Area | Current behavior |
| --- | --- |
| World | Shared German/English career, stable wide club/player/crest IDs |
| Human managers | One to four managers; running entry with stable existing clubs/results, including mixed countries |
| Countries | Independent Germany/England/national-play controls; English Premier League fallback when England is disabled |
| Leagues | Germany five times 18; England 20/20/20/20/10, four visible selectable divisions; Spain/France/Italy each 18 |
| Competitions | Separate domestic cups, shared European cups, national competitions, indoor tournaments and promotion playoffs |
| Management | Original lineup, tactics, training, camps, bank, facilities, environment, advertising, contracts and transfers |
| Friendlies | Actual club/national opponent picker, cost and human consent, dated booking, attendance, match and saved result |
| Match UI | Existing Classic renderers, pre-match country grouping, winner screens, live overviews, settings, player cards and genuine intervention screens |
| Progression | Saved daily/weekly/monthly services, actual match clocks, awards, promotion and annual continuation |
| Statistics | Actual season and persistent title/player histories; unknown older facts remain unknown |
| Saves | Typed FBM career snapshots, both RNG streams and owned match/attendance receipts |
| Runtime data | Self-contained OBD; refreshed English squads and Original FLTD camps; no original executable or launcher required |
| Accounting | Recorded current values plus saved current/real/future planning, Original controls and warning acknowledgement |
| Postponement | Original Verlegen calendar; individual early/deferred home fixtures, undo, real replays and saved receipts |
| Joining a running game | Original options/club picker; staged atomic publication, existing loans retained |
| Tools | Earlier obdeddi/console remain Classic-profile tools, separate from the shared runtime |

## Verification and limitations

Public CI builds and runs the available synthetic tests on Windows x64,
macOS ARM64 and Linux x64. Tests requiring original private content are
explicitly skipped there. Every release includes a revision-bound QA report and
checksums; a development build alone is not a release verification.

Private validation covers actual supplied runtime data, native desktop input,
match interventions, friendly booking and play, strict save/load continuation,
random streams and multi-year national careers. The Windows compiler's
allocation-free typed publication checks also run on the supplied Windows server.
The private screenshot/save evidence is not included in public source archives.
Final mixed-club season checks and package checks are recorded with the release.

See [release notes](../release/extended-1.0/RELEASE_NOTES.md) for installation,
save compatibility and Beta limitations. See [the implementation contract](IMPLEMENTATION.md)
for the shared-country design. Individual milestone documents in this directory
retain their historical validation boundaries; this file is the current overview.

The 32-bit financial-storage limitation is not represented as solved by the
separate dividend arithmetic fix. National-only play uses a real independent
coach account without a fabricated hidden club and is deliberately unranked.
