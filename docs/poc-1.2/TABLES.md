# Owned country league tables

Desktop menu 0 now opens the existing Original `LeagueTableScreen` through the
typed Session. It displays the actual 18-, 20- and 10-team leagues with fixed
Original row spacing, saved results, current/previous ranks, form and human-team
colours. No separate table renderer or gameplay calculation is introduced.

## Country and league controls

The right selector cycles the available Germany, England and Europe groups.
Its artwork reuses the supplied Original Germany/England/globe setup symbols.
The left selector retains Germany's four-level map with both Regionalligen on
one level; England has five levels. Markers are centred against the actual
Original selector height. Hitboxes use the same positions. The subtitle contains
an explicit German ordinal separator, for example `1. Spieltag`, including older
data-only runtime packages that do not contain its original punctuation string.

`Catalogue::table_leagues` is the sole membership policy. Disabled Germany has no
tables. With England enabled, its five leagues belong to England. With England
disabled, only its Premier League remains as AI opposition under Europe. It is
not duplicated across groups. Spain, France and Italy remain in Europe.

The existing Source footer supports combined/home/away, points/goals sorting,
two-/three-point display comparison, all games and the two-stage round range.
Range limits follow each league's actual 34/38/18 matchdays. Start cancellation
retains the previous interval; end cancellation restores it with Source range-mode
flags. Both dialogs use the actual table backdrop and explicit runtime reference.
The SDL presenter completes the separate updated-modal and restored-underlay
Present phases before delivering an accepted result.

KI forecast and family navigation to pairings/strengths are not connected in this
packet; their buttons/hitboxes are absent. Visible team rows open the existing
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
league. Selecting the same league retains the current view. Closing this standalone
typed table does not run the still-unintegrated legacy family teardown/random stir.
These decisions apply to Extended's typed path; the legacy renderer/control path
remains unchanged.

## Verification

Data-free model tests compare 224 typed query/control projections with the existing
Original table calculation for 18/20 teams. They also check the ten-team shape,
country scopes, Premier-League fallback, global identities above 65535 and schema33
roundtrips/older-schema rejection. Actual-input GUI checks cover all thirteen
leagues, both mixed-country actors, same/changed selectors, controls, numeric
range acceptance/cancellation/refusal, roster return, saved preferences, stale
windows, visible frameless folders, blocked saves and process-timer close.

Additional rendered tables use real Core-computed first-round results for all
thirteen leagues, including the unplayed ten-team round. No score is invented.
These are result-view checks, not a claim of a new post-match GUI owner or a played
season. Native SDL screenshots/state comparisons and matching FBMs are private at
`bmh_4k_ui/docs/qa/extended-league-tables`. Native Windows/macOS GUI operation and
full Beta/season qualification remain separate gates.
