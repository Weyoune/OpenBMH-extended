# Extended follow-up reports, 2026-11-03

Tracked as issues #36–43. Corrections reuse the Classic renderers and match
calculations. Original content and saved-game evidence stay private.

| Issue | Correction |
| --- | --- |
| 36 | Refresh English club membership and squads from locally supplied FC tables; retain the Extended league dimensions and document every structural adaptation. New data applies to new careers. |
| 37 | English managers use the Football Limited map, pictures, resort names, coordinates, quality and price tables. The interface language stays German. |
| 38 | Club information opens actual player cards and home/away/combined statistics, with canonical club identities and crests. Display/radio/off choices persist and respect the Original four-match limit. |
| 39 | The European league selector uses the Original Europe selector artwork. |
| 40 | Restore the Original pre-match screen, with one blank line between German and English competitions. The internal fifth English division stays hidden. |
| 41 | Restore the Original winner screens after actual completed domestic/European finals, championships and applicable indoor/national titles. Rendering never creates a result. |
| 42 | Show national qualification draws only for a human national participant, using the actual staged session identity. |
| 43 | Switch to the second Original music track after successful career start/load, respecting the saved music setting. |

## Persistence and transaction boundaries

World schema 44 adds nullable `ClubRecord.presentation_category`: 0 is off, 1
radio and 2 watched. Missing values keep the Original human/AI defaults. Older
worlds remain readable; a nonnull category cannot be disguised as an older
schema. The existing typed FBM envelope is unchanged. Re-entering club information
does not overwrite an explicit choice. A new human manager receives the normal
watched category through the existing four-game selection rule.

Nested modals use an owned positional drawing workspace. Local club/player IDs
cannot become saved world IDs. Player-card random draws, roster cleanup,
statistics preferences and category changes publish only after successful close;
interruption, expired capabilities and reentrant changes reject publication.
Team statistics use the actual country's standings and point policy. A missing
historical position curve is omitted rather than fabricated.

Watched AI-only league groups expose their existing minute clock to the same
presenters. Off/radio choices do not cause an invisible start/halftime pause.
This does not change the league coordinator into a simultaneous global clock.

## Supplied data refresh

`tools/world12/refresh_english.py` accepts an existing master OBD, locally supplied
FC tables, the nationality catalogue and Football Limited resources. It does not
include or download proprietary content. Source hashes, full source player names,
birthdates, ratings, nationality limitations and crest origins are recorded in
`ENGLISH-REFRESH.json`; `PACKAGE.json` covers the resulting files with hashes.

The retained 20/20/20/20/10 structure cannot reproduce the real 24-team lower
English divisions: the importer selects twenty by source rating, and uses ten
omitted clubs in the internal reserve pool. A source roster shorter than 24 keeps
empty trailing slots. Unrepresentable nationality codes are explicitly marked and
retain the previous compatibility code; the source country remains in the report.
The Original career generator remains responsible for starting ages and strengths.
Existing saved careers keep their embedded database, and old crest resources stay
available for their saved paths. The 32-bit money limit is unchanged.

## Validation

Synthetic tests cover presentation ownership/persistence, country grouping,
interrupted observers and the importer. Private data checks exercise Original
English camp booking, nested club modals, and an entire season with real match
calculations, pre-match lists and completed title screens. Native SDL checks and
platform CI results are recorded separately; building a platform does not claim
an interactive playthrough on that platform.

The Linux milestone registered 308 CTest checks: 213 passed, 95 unavailable
special-fixture checks skipped, zero failed. The current-data one-season probe
completed 110 pre-match lists, 11 title screens and 123 actual match clocks,
with strict saved-state roundtrips. Additional supplied-data checks pass for
English camp booking, nested player/statistics/category modals and watched AI
settings without score/account/RNG changes. Native SDL input additionally caught
and corrected an implicit Classic-profile reload on category-button repaint.
