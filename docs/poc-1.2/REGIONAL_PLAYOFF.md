# German regional promotion playoff

The schema40 `regional_playoff` owner preserves the actual Source campaign:
CupTable entry35, competition13, four clubs, six double group rounds, twelve
scores and two qualification fields. Source flags and the six German `0x4004`
calendar appointments are supplied data. A missing/null owner remains unknown;
no field or score is reconstructed from a late save or a year-zero draw.

`make_regional_playoff_season` creates a known inactive owner for a fresh season.
It consumes no RNG. The day transaction calls `activate_regional_playoff` only
when both actual Regionalliga final rounds cross their last cursor. Both complete
result tables are required. It selects Source zero-based ranks1 and2, scanning
league2 and then league3 in physical membership order. Championship winners do
not enter this field. The point deductions used for that table are retained;
later account decisions cannot retroactively change the frozen selection.

Each date produces a strict `RegionalPlayoffGroup` with actual global club IDs,
season/date/revision binding and Original four-team fixtures. The existing common
match runner uses Source descriptor14, its card/option class, one minute clock
and existing human/AI post-match settlement. It produces real score words;
`commit_regional_playoff_results` stages their Source `advance_cup(13)` transition.
The outer day owner must publish results, cursor and date atomically. The group
has no invented knockout final, extra-time result or title award.

Source group ranking and qualification use the existing `cup_group_table` and
`advance_cup` kernels. Annual promotion consumes the exact Source field-word order
`{entry35+24,entry35+22}`: runner-up followed by winner. Both qualifiers may come
from the same regional league. `annual_regional_qualifiers` rejects an unknown,
unactivated or incomplete campaign. It must be called before promotion changes
regional membership. Next-season publication installs a new inactive owner with
the new real calendar; it does not reuse the completed field.

Public synthetic tests compare activation with Original `advance_league_rounds`,
and all six group transitions with Original `advance_cup(13)`, including exact
zero draw consumption. One to four human cases calculate twelve real Core matches
each and compare every score, full World/player/finance/monthly state and exact
RNG against direct Original calculation. Save/reload, stale/repeated dates and
results, cancellation and unknown older-schema ownership are covered. These are
bounded backend checks; they do not establish a played full-calendar season,
Native GUI progression or a three-season release gate.
