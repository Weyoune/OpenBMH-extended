# Other games: mixed BMH/FLTD layout

The 1.2 overview keeps the Original 320×240 surface and two columns of four competition controls:

| Left | Right |
| --- | --- |
| DFB-Pokal | WM/EM-Qualifikation |
| UEFA-Cup | Super-Cup |
| Champions League | FA Cup |
| Pokalsieger-Cup | League Cup / Koka Kola Cup |

Germany/England controls follow the selected club systems. National qualification follows International. European pages require club play. Unavailable competition data is shown explicitly as not initialized; no European/national fixtures are fabricated by the UI adapter.

## Shared Super-Cup page

The two competitions remain separate engine competitions and separate saved results. Only their menu page is combined. With both countries enabled, the German Super-Cup and English Charity Shield have individual country/competition headings and panels, separated by a 60-native-pixel row distance. The whitespace is not a match hitbox. This avoids a shared semifinal-like bracket or aggregate score.

With one country disabled, only that country's final remains and uses the original centered final position. A missing draw is marked within its own section instead of inventing teams or suppressing the other final. No second round or complete-view switch is introduced for the combined finals page.

## Existing rendering and safe identities

`world12_other_games_state` projects the typed snapshot into the existing `OtherGamesScreen`. Original assets, sprite buttons, arrows, row text, crown markers, score phase/return-leg formatting and club-detail labels are reused. Club averages call the same Core helper as the legacy overview; rendering does not draw randomness or mutate the world.

UI-local row tokens are mapped separately to global 32-bit club IDs. Requests carry a typed manager/club/revision/scope/sequence identity. Team-info requests carry the global club ID and never pass the local token as an engine ID. Legacy dispatch explicitly refuses typed requests. Typed navigation supports the eight selectors, Original detail windows, complete view and stored domestic cup stages/rounds. Repeating the active selector cycles blocks of sixteen participants; the arrows change rounds and wrap at the first/last planned round. Hidden compact-mode controls are rejected. The window owner advances its sequence after each accepted navigation.

The existing single-edition projection/rendering path remains under its source layout. Its geometry/action tests still run against the supplied reference installation.

## Session and desktop integration

Menu kind 5 now opens through the typed Session from both an enabled desktop
icon and its visible folder. The existing renderer receives the supplied runtime
reference explicitly; opening it does not search for an original executable.
The session stages the page, its parent desktop and renderer before publication.
Actions authenticate the canonical manager/club, root revision/scope and window
sequence. Reopening retires the old generation, including requests whose revision
was subsequently changed to the current one. Closing restores the same desktop.
Manual game saves cannot take ownership while this menu is open.

World schema 32 adds the optional per-manager `other_games_view` Source byte.
Older snapshots retain an unknown byte until Original menu entry initializes it
to detail/participant zero (`0x80`). Selector paging and detail/complete changes
retain the Source byte writes; round arrows reset only the local participant
cursor. Game results, finances, date and both random streams remain unchanged
by overview navigation. Active windows themselves are not serialized.

## QA and integration boundary

Data-free tests cover the selector order, scope removal, two independent scores, missing draws, wide club IDs, detail pagination and stale typed navigation. Additional supplied-asset tests render all eight pages and exercise both final hitboxes, the non-clickable gap and FA Cup's selector.

`world12_other_games_demo` exports seven Original-asset views from supplied private data. Its supercup scores are explicit **synthetic UI examples**, not played career results; generated images/Original content are kept only in the private QA repository.

The actual desktop hookup now has single-English and both mixed-country actor
checks, including a frameless folder, all selectors, participant/round controls,
future undrawn rounds, complete view, saves and stale-window rollback. A real
Linux SDL load/click/save run compares the complete saved snapshot with the
independently checked GUI result plus the four existing manual-save W draws.
The milestone regression passed 167 tests, with 95 unavailable-special-fixture
skips and no failures. Screenshots and matching saves are private under
`bmh_4k_ui/docs/qa/extended-competition-overview`.

Visible club clicks now open [the Original roster inspection](CLUB_INFORMATION.md)
with Data A/Data B and the same overview restored on exit. Source roster-entry
selection/injury flag writes persist for that club; other gameplay stays unchanged.
Nested player cards, club statistics and category changes remain separate typed
owners. European and national competition schedule/results owners and round-date
presentation also remain open. Missing competition data or a future undrawn round never receives invented
fixtures, scores or dates. The seven-view demo above remains a separate synthetic
UI probe, not evidence of a played career. Full live/annual/Beta gates remain open.
