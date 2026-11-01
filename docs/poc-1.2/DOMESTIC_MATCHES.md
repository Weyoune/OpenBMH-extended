# Domestic cup calculation and dated publication

The common bounded match runner now accepts verified domestic knockout legs as
well as league rounds. It reuses Core team preparation, side ratings, minute
phases, extra time, shootouts, morale, player development and manager records.
League-only callers retain their old path and have regression coverage.

The cup owner supplies the saved stage and corresponding country's rules. The
adapter verifies participants, flags, round/leg cursor, source predecessor stages,
date, weather, referee data and presentation policy. First legs cannot acquire a
winner through extra time; return legs retain home/away orientation and the first
leg's actual score words. A human event needs its presenter or an explicit
headless-decline policy. Reserve/national teams without a league roster still
need their own projection and are rejected here.

Initial careers now contain each enabled country's fixed champion/cup-holder
Super-Cup field. Super-Cup and Charity Shield are separate stages with no draw
popup. Their existing combined results page remains a presentation grouping.

`play_domestic_cup_day` calculates complete country groups in canonical calendar
order, then publishes all scores, successor draws, random streams and exactly
one calendar advance. A missing group, late second-country error or reentrant
owner change cannot publish a partial day. The next daily-service phase is saved.
The equivalent league-day owner now also retains that saved progression boundary.
Session entry points reject initial desktops, open menus and pending services.
Each league/cup group has its Core minute clock; this is not a claim of a visible,
interleaved all-group match controller.

Validation includes German/English AI and human finals, multiple seeds,
cross-division participants, return-leg resolution, codec/snapshot roundtrips,
partial and stale batches, late source-rule failure and external owner mutation.
The real four-manager session continues from nomination to both finals on day 17,
calculates them, saves/reloads, then calculates twelve complete league rounds at
day 24 and saves the next pending-service boundary. These probes use actual
players/rules and actual calculated scores, with declared fixed QA weather,
crowd and referee conditions.

Full pre-match event preparation, authoritative weather/referee/crowd generation,
gate receipts, competition premiums/trophies, visible live/result ownership and
complete annual play remain release gates. The probes are neither a full GUI
career nor evidence of three played seasons.
