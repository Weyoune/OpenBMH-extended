# OpenBMH-Extended 1.0 Beta status

The current milestone is `1.0.0-beta.4`: the shared world on the code base of
OpenBMH Classic 1.0.13. A version string alone is not a published release.
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

What Beta 4 was checked with:

- `world12_soak` plays whole shared careers without a window. Careers with one, two
  and four managers ran three seasons without a stop, with a German and with an
  English first manager, with and without the national coach role; single seasons
  ran with only Germany and with only England. Every season change was saved and
  loaded back unchanged. The managers of these runs decline every offer, renew no
  contract and book nothing: the runs prove the course of the seasons, not the
  management screens.
- With Classic data the build reaches the same state at every desk as Classic 1.0.13
  in nine reference careers of 110 turns.
- The window program was driven by script from the title to the sixth kick-off of a
  shared career. The remaining screens of the table above were taken over from the
  earlier Betas and were not checked again for Beta 4; the open issues list what
  players reported there.

These runs need the data package and are not part of the public CI.

See [release notes](../release/extended-1.0/RELEASE_NOTES.md) for installation,
save compatibility and Beta limitations. See [the implementation contract](IMPLEMENTATION.md)
for the shared-country design. Individual milestone documents in this directory
retain their historical validation boundaries; this file is the current overview.

The 32-bit financial-storage limitation is not represented as solved by the
separate dividend arithmetic fix. National-only play uses a real independent
coach account without a fabricated hidden club and is deliberately unranked.
