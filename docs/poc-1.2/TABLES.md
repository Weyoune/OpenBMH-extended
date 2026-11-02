# Owned country league tables

Desktop menu 0 now opens the existing Original `LeagueTableScreen` through the
typed Session. It displays the player-visible 18- and 20-team leagues with fixed
Original row spacing, saved results, current/previous ranks, form and human-team
colours. No separate table renderer or gameplay calculation is introduced.

## Country and league controls

The right selector cycles the available Germany, England and Europe groups.
Its artwork reuses the supplied Original Germany/England/globe setup symbols.
The left selector retains Germany's four-level map with both Regionalligen on
one level; England has four player-visible levels. Markers are centred against the actual
Original selector height. Hitboxes use the same positions. The subtitle contains
an explicit German ordinal separator, for example `1. Spieltag`, including older
data-only runtime packages that do not contain its original punctuation string.

`Catalogue::table_leagues` is the sole membership policy. Disabled Germany has no
tables. With England enabled, its four visible leagues belong to England. With England
disabled, only its Premier League remains as AI opposition under Europe. It is
not duplicated across groups. Spain, France and Italy remain in Europe.

The existing Source footer supports combined/home/away, points/goals sorting,
two-/three-point display comparison, all games and the two-stage round range.
Range limits follow each league's actual 34/38/18 matchdays. Start cancellation
retains the previous interval; end cancellation restores it with Source range-mode
flags. Both dialogs use the actual table backdrop and explicit runtime reference.
The SDL presenter completes the separate updated-modal and restored-underlay
Present phases before delivering an accepted result.

KI forecast is not connected; its button/hitbox is absent. The existing footer
opens the owned pairings/strengths children described below. Visible team rows open the existing
owned roster Data A/Data B inspector and restore the same table on exit. Player
cards, club statistics and category setters retain their documented boundary.

## Calculations and ownership

`world12_table_state` maps the checked `compute_standings` result to immutable
Original rows. The latter shares Classic's table kernels, tie ordering, byte
counters, deductions and form/history behaviour. Sorting uses Source display-word
bits; the point-rule toggle is a display comparison and changes no match rules.
The row-to-global-club map is separate from local drawing tokens. German rank
colours retain Source rules; English promotion/relegation colours follow the
existing Extended ladder descriptors using the same palette.

Requests bind canonical manager/club, root scope/revision and per-window generation
and sequence. Hidden leagues, stale/retimed input, missing/interrupted presenters
and malformed ranges reject before publication. The parent renderer, candidate
snapshot and new view are validated before the live session is swapped. Table
navigation changes only the active manager's display word. Roster entry retains
its independently validated target-selection/injury cleanup. Results, player
attributes, finances, date and RNG streams are not changed by table navigation.
The process timer closes the menu through the same owned request path.

World schema 33 adds optional `manager.career.table_display_word`. It preserves
all sixteen Source bits; older schemas retain an unknown value on decode. Actual
Core projections install/capture the word under their local manager mapping.
Round interval and point-comparison state belong to the current window; reopening
starts a fresh interval, as in the Source table entry.

OpenBMH deviates from the original: pre-schema33 development snapshots, which did
not expose this typed menu, initialize their unknown table word to the Original
new-career default `0x11` on first entry. No gameplay history is reconstructed.
Changing to another league also starts a valid current-round interval for that
league, preventing a 38-round English interval from spilling into an 18-round
league. Selecting the same league retains the current view. Closing this typed
family does not run the still-unintegrated legacy teardown/random stir.
These decisions apply to Extended's typed path; the legacy renderer/control path
remains unchanged.

## Verification

Data-free model tests compare 224 typed query/control projections with the existing
Original table calculation for 18/20 teams. They also check the ten-team shape,
country scopes, Premier-League fallback, global identities above 65535 and schema33
roundtrips/older-schema rejection. The earlier GUI checks covered all thirteen internal
leagues before the visibility correction below, both mixed-country actors, same/changed selectors, controls, numeric
range acceptance/cancellation/refusal, roster return, saved preferences, stale
windows, visible frameless folders, blocked saves and process-timer close.

Additional rendered tables use real Core-computed first-round results for all
thirteen leagues, including the unplayed ten-team round. No score is invented.
These historical captures predate the visibility correction below. They are result-view checks, not a claim of a new post-match GUI owner or a played
season. Native SDL screenshots/state comparisons and matching FBMs are private at
`bmh_4k_ui/docs/qa/extended-league-tables`. Native Windows/macOS GUI operation and
full Beta/season qualification remain separate gates.

## Pairings and strengths family

The typed owner also accepts Original desktop kinds 1 (pairings) and 2
(strengths). The existing Original pairings and strengths screens now have
explicit-reference constructors. The table footer and their existing family
buttons switch between all three views while retaining the selected country and
league. Each child uses the same canonical owner, generation and monotone request
sequence. Closing any child retires the entire window.

Pairings use the authoritative typed fixture table and stored results, with
actual 18-, 20- or 10-team rows. Their ratings call the existing Core
`team_average` kernel in a bounded local-club workspace. Dates come from the
saved league calendar; unavailable dates retain the Original unknown-date
presentation. Ordered home/away search, selected-club season filtering, ten-row
paging, current-round reset and round arrows follow the Source controls. The
postponed list is empty because typed league results currently have no postponed
result records; it does not construct postponements. The temporary KI forecast
button remains hidden pending its separate RNG and actual-Present owner. Active
weather/referee alternation also requires a qualified live-event packet and is
not inferred from a saved score.

Strengths reuse the Original criterion/position controls, sorting, draw-word
normalization and byte-age calculation, including the distinct entry bit10 and
header bit15 age bases. Explicit club coordinates remove the old nine-league
membership assumption without packing global identifiers into legacy fields.
The 18/20-team row spacing remains within the Original panel. Country markers
use each child's actual selector height; the strengths footer keeps its existing
lineup toggle. Both children open the same owned roster inspector as tables.

World schema 34 adds optional `strengths_display_word` and
`pairings_display_byte` career preferences. Older snapshots retain unknown
values until the corresponding first entry. That entry uses the Original new
career defaults (`0x1ff` and `1`). Family navigation restarts the selected
child's transient state as the Original family entry does, while retaining its
league. Window-only search, filter, age scratch and intervals are not save data.
The typed family still does not run the unintegrated legacy close-time random
stir; its navigation and close consume neither random stream.

The focused regression suite has nine passing targets and one expected external
fixture skip. It includes a data-free strengths projection/control test, codec
backward-compatibility checks and the existing Original pairings/table render
checks. Earlier shared-world GUI checks visited all thirteen internal leagues for a single
English manager and both actors of a mixed-country career. Separate immutable
projections render all three children from the real Core-computed result
snapshot, checking 112 played scores without constructing a desktop owner. That evidence
predates the player-visible league correction below.
Native Linux SDL input produces 46 exact frame comparisons (apart from the
process-timer row) and a complete save equal to the checked snapshot plus the
Original four manual-save Watcom draws. Native Windows/macOS GUI operation,
qualified forecast/live overlays and Beta/season completion remain separate
gates.

## Original English league visibility

The Original English FLTD exposes four league levels to players.
`Catalogue::player_leagues(England)` therefore returns IDs 5–8. The ten-team
league (ID 9, local index 4) remains in the internal thirteen-league catalogue,
fixtures, cups, rankings and annual model; it appears in no country or Europe
selector. England's four markers are centred using the actual child frame
height. The Premier League fallback policy is unchanged.

Fresh manager selection leaves English picker group 4 empty and cycles four
starting levels. Additional club pages 5 and 6 remain separate. The server
rejects both a hidden target league and a selected hidden-league club before
placement or RNG consumption. Existing synthetic/read-only managers in ID 9
remain valid model data; opening their league-family view reports the explicit
hidden-league boundary. No club is silently moved or replaced.
