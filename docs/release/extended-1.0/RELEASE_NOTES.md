# OpenBMH-Extended 1.0 Beta

The first shared German/English career release retains the Classic interface
and match calculations. It is a separate game line from
[OpenBMH Classic](https://github.com/Weyoune/openbmh).

## Included

- Start a career with one to four managers, including German and English clubs
  in the same saved world. Menus remain German.
- Enable Germany, England and international national-team play independently.
  European club competitions belong to club play.
- Germany retains five 18-team leagues. England uses 20/20/20/20/10 teams;
  four English divisions are selectable by human managers. The fifth is
  simulated. Spain, France and Italy each have an 18-team first division.
- With England disabled, its Premier League remains AI-only in the international
  tables. Lower English divisions and English domestic cups are absent.
- Separate domestic cups and shared European competitions; Super-Cup and Charity
  Shield appear on one page with clearly separated results.
- Management, training, transfers and loans, advertising, facilities, banking,
  match interventions, actual weather and crowds, and persistent statistics use
  the shared career state. Friendlies work for clubs and national teams.
- Saved careers retain global player/club identities, both random streams,
  active competitions, prepared match conditions and seasonal history.
- New high-resolution Extended start artwork; recent Classic input, dialog,
  localization and desktop corrections are included.

## Beta 3 corrections

- Current English squads from the supplied FC source, with import provenance.
- Football Limited training-camp map and country-specific booking values.
- Player cards, team statistics and saved display/radio choices in club information.
- Original Europe artwork, pre-match lists and competition winner screens.
- National draws restricted to relevant human national participants; career music restored.
- The preceding Beta 2 milestone adds match postponement, a new human manager
  during a running career, and interactive future balance planning.

## Installation and data

Extract the complete archive for your operating system. Run `OpenBMH.exe` on
Windows or `OpenBMH` on Linux/macOS. The program requires compatible Extended
runtime data; public executable downloads do not contain original game content.

A supplied, finalized `COMMUNITY.OBD` beside the executable is detected
without a launcher or original DOS executable. An explicit location also works:

```sh
./OpenBMH --data "path/to/extended-data" --save-directory "path/to/saves"
```

The Extended data profile is `dual-classic-2`, with a validated World schema and
runtime manifest. Development packages marked `playable:false` require the
explicit developer flag and are not release data. Keep the import report with
the dataset: added clubs, rosters, ratings and replacement crests include
identified fictional adaptations, not a verified complete current-season census.

This release uses its own typed FBM career snapshots. Classic, historical 1.1
and original DOS saves are not migrated into shared careers. Back up your data
and keep the different games' save directories separate.

## Beta limitations

- The earlier native `obdeddi` editor and console operate on Classic-profile
  data. They do not edit or run the authoritative shared `WORLD12.JSON` career.
  The runtime download therefore contains the graphical game only.
- Some original-width financial fields remain 32-bit. The dividend intermediate
  overflow correction is included; this is not a general arbitrary-balance fix.
- National-only careers deliberately have a real coach account without an
  invented hidden club. They are unranked; they do not produce a club highscore.

## Verification

Each published archive is tied to the same successful CI revision. The attached
`QA-REPORT.json` contains the Windows x64, macOS ARM64 and Linux x64 test totals,
source revision and archive hashes. Tests requiring original private content are
reported as skipped in public CI rather than counted as passing.

Private checks additionally exercise real runtime data, actual match clocks,
friendlies, save/load, desktop transitions and seasonal progression. Screenshots
and saved-game evidence stay in the private QA repository. CI platform builds
and native function tests do not claim an interactive macOS playthrough.

This remains a Beta: retain saves that reproduce a problem and report the version,
platform, selected countries and steps in the Extended issue tracker. Do not
upload original data or private saves to a public issue.
