# Owned lineup, pitch, tactics and training

The development desktop now routes the Original lineup, tactics and training menus through the typed Session. Opening a menu stages its Original entry effects and a new owner revision, constructs its renderer, then publishes the staged session. Menu commands require the actual pending UI request and active manager/club/revision. Failed UI construction, stale requests or denied commands preserve the accepted session. Closing a player card does not release its parent menu owner.

## Shared calculations and canonical state

Lineup entry reuses auto_lineup, prepare_lineup_entry and the supported designated-taker initialization. Roster exchange, pitch placement, automatic weights, formation selection/storage, individual orders and set-piece choices call the existing bounded Source helpers. Marking an opponent remains unavailable on the initial preparation day because no opponent exists; none is manufactured. Injury/foreign-rule decisions needing an additional confirmation remain explicit rejected outcomes until their owner is integrated.

The training/tactics request validation and calculations were extracted unchanged from the legacy session into shared app helpers. Legacy modal exclusions stay in that session; the typed Session owns its separate preparation modal. Training opens through deterministic automatic-plan initialization. Plan/type/area/intensity/focus/automation edits and the eight tactics settings retain the existing Source rules. They change neither RNG stream nor the calendar. All verified financial/tactical/match/player changes map back through the typed projection; global IDs never enter packed rows.

Desktop projections use a stable player order based on global IDs, rather than current roster row order. A player therefore keeps the same temporary drawing ID while moving between rows, including across UI acknowledgements. Match projections retain their existing ordering. Board marks, designated takers and poaching references are reinstalled explicitly through the new map.

## Presentation

Original lineup, training, tactics, pitch, sprite-font and player-card renderers accept explicit reference images. Their ordinary constructors still load the verified legacy profile. Nested pitch sheets, copy/acknowledgement constructors and player-card underlays retain that reference. Club crests use the explicit atlas and cell selected by the typed world. Full names remain in the data and player card; lineup and training names are shortened visually to the existing name-column width to keep other columns legible.

The menu context contains actual lineup, training, tactics, pitch and player-card state. It does not invent bank/stadium/transfer data for still-unintegrated menus. The existing preparation consistency checks also bind the global manager identities and crest atlas.

## Save framing

S7 adds the per-manager Original display word (CareerSession_AnyOf_ExecutableField), which is also saved by the legacy FBM layout. It retains selected pitch pages across menu re-entry/save roundtrips. The reader accepts S4/S5/S6 without inventing newer event state. Missing pre-S7 display words retain the initial zero default. WORLD12 JSON stays version 15.

Daily services, subsequent manager/event advancement, remaining management menus, initial competition/receipt/offer presentation and native release QA remain open. The development package is still non-playable metadata.

## Original formation initialization correction

The GUI run exposed an all-zero formation availability mask in new careers. The original CareerAddManager routine writes 0x0f to both record offsets 0x218/0x219 immediately after its starting-balance initialization. Those writes were missing from the shared implementation. They are now part of initialize_career_manager_funds, enabling the four provided outdoor/indoor presets in both typed and legacy new careers without drawing randomness. Existing saved masks remain untouched.

The GUI taker test also exposed incorrect bridge offsets for the first two designated players. The canonical three-ID array now reads/writes 0xafc, 0xafe and 0xb08, matching the existing pitch setters and match engine; 0xb04/0xb06 stay untouched. The field-isolation test checks those neighbours.

The three extension controls are labelled for the actual match roles (penalty, rebound and free kick). The former free-kick/penalty/corner labels did not match their offsets; no separate corner-taker field is claimed.
