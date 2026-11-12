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

## Beta 4: on the code base of Classic 1.0.13

Beta 4 merges everything OpenBMH Classic gained between 1.0.2 and 1.0.13 and adds the
first whole-career checks of the shared world.

From Classic:

- The rules follow the Diamond 3.7 programme: match events and injuries, start capital,
  licence limit, weekly processing, facility values, income per visitor, season tickets,
  the youth academy (two rounds a season, an older player of a computer club is replaced)
  and the season end. The credit annuity is exact.
- Transfer market: the search dialog behind Suchen and Vorschlag, the review of offers
  for an own listing, the question whether a listed player leaves the list, asking fees
  as in the original.
- The stadium keeps the limits of the edition (Diamond 3.7: 96,000 places).
- Settings: every unlocked switch can be changed. National coach desk, calendar,
  training bars, set piece editor and pointer corrections; buttons look pressed while held.
- `-debug` writes a log and a save after every match day into the save directory.
- The built-in numbers of the original programs and `openbmh.dat` (Classic data without
  the original executable).

Corrections of the shared world:

- A career no longer stops on the day of the national Super-Cups when the champion also
  won the cup in both countries.
- With several managers, a point deduction of one manager no longer stops the indoor
  tournament of a manager in another league.
- A second academy recruit in one season is accepted.
- The monthly transfer list draws a club player among the players in the squad, not
  among squad plus players away on loan.
- The bundled data package keeps its own runtime when no original executable is present.

How it was checked:

- A new program, `world12_soak`, plays whole careers of the shared world without a window.
  Careers with one, two and four managers over three seasons, with a German and with an
  English first manager, with and without the international competitions, and single
  seasons with only Germany or only England ran to the end; every season change was
  saved and loaded back unchanged. In these runs the managers decline every offer and
  renew no contract, and matches are played without presentation: they prove the course
  of the seasons, not every screen.
- With Classic data the program gives the same state at every desk as Classic 1.0.13 in
  nine reference careers of 110 turns.
- The window program was played by script from the title to the sixth kick-off of a
  shared career.

Known limits of the Beta: the window program is still one source file (Classic has split
it); the pressed picture of OK in the transfer confirmation is missing; saved games of
Beta 3 were not tested with Beta 4.

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
