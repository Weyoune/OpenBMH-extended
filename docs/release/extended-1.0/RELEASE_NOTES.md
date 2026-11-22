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

## Beta 5: Almanach and Wechsel

Two additional icons stand on the desk of the shared world, below the lowest icon row. They are
additions of OpenBMH-Extended; the original game has neither. Both can be moved with the right
mouse button like the other icons, but not put into folders or onto the shortcut bar.

**Almanach** is the reference book of the career, in the look of the statistics screens:

- Saison: champion and runner-up of every league, the cups, the European cups, the indoor
  Masters and the national tournament, for every completed season.
- Tabelle: the final table of every league and season.
- Torjäger: the five best scorers of every league and season.
- Ehrentafel: titles counted per club and competition.
- Ewige Tabelle: the all-time table of the highest division of each country.
- Manager: club, league and final place of every human manager, season by season, and his titles.

Final tables, scorers and the clubs of the managers are recorded from Beta 5 on. A career that
was started earlier shows the title holders of its earlier seasons and "no record" for the rest.

**Wechsel** shows job offers. This is a new rule:

- At the change of season every human club manager is judged by his final place against the
  strength of his squad. A manager who clearly did better than his squad promised, a champion,
  a promoted manager or a cup winner receives up to three offers, up to two of them from the
  next higher division. A season as expected brings one or two offers of comparable clubs. A
  manager far below expectation, relegated or starting with a point deduction receives none.
- Offers come only from clubs without a human manager. With both countries in the game an offer
  can come from the other country. No club is offered to two managers.
- An offer can only be accepted at the first desk of the new season. It is a new start: the
  funds, stadium, sponsors and squad contracts of the new club, as for a manager who joins a
  running career. The manager keeps his name, picture, desk, mailbox and his career record
  (places, titles, awards, series), so the highscore counts the whole career. Cash, savings,
  credits, shares, the old stadium and sponsors are given up. The old club is led by the
  computer from then on.
- A change is refused while credits between human managers are open.
- There is no dismissal.

How it was checked: a test without game data covers the recording, the saved form (a world
in the format of Beta 4 loads; an actual save file of Beta 4 was not tried), the offer rule and the change of club. `world12_soak` with the new
scope letter `j` opened the almanac at every first desk and accepted every first offer: careers
with one and four managers over three seasons ran to the end, with four changes of club in the
four-manager career. The window program was driven by script through both screens.

Known limits: the page of record results is not part of the almanac yet; the mailbox does not
announce new offers (the Wechsel screen shows them); a table longer than the window is scrolled
by a click into its upper or lower half.

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
