# First owned event and Original desktop

The typed startup controller now stages final manager confirmation, all existing initial-season services, the initial empty calendar event and an explicit Original desktop attachment. The normal legacy entry remains separate; `--world12-dev` selects this unfinished integration path. It is not yet a playable release.

## Event ownership

`Session::begin_first_event` delegates to `prepare_first_desk_event`. It requires the initial date/season, a complete prepared club world, no existing active desk/event, no loans and no appointment on day zero. A real day-zero appointment is rejected rather than skipped. It retains the calendar cursor/date at zero.

The transaction draws CurrentMatchday with W, shuffles the initial manager order with G, prepares the first manager's unsolicited offer, opens the empty Toto coupon and calls the already shared DeskDate boundary. Both RNGs, optional market listing/bid, offer notice and desktop runtime commit under one revision. Replayed or interrupted requests preserve the accepted state. The stored event explicitly records its empty coupon; it does not fabricate played games.

## Initial unsolicited offers

Mode I suppresses the offer path as in the Original. Other modes draw the rating-dependent chance, select a fit actual squad row, quote it using `market_value`, distinguish salary-only approaches, rate eligible AI clubs, sort with Source quicksort and quote the chosen bid. The same `transfer_club_score` and `computer_bid_amounts` kernels are used by the legacy/typed calculations. The historical chosen-index price-level quirk, contract-term draws, unsigned monetary arithmetic, high-offer draw and salary-warning comparison are retained. A quoted row with no bidder remains a quote without a pending notice, matching Source.

Club strength/position counts are read through bounded per-club projections. The full world never needs to fit in a packed 12-bit player table. Player/seller/bidder references remain global IDs. Regional eligibility uses the actual country and playable/non-playable capacity already used by typed transfer initialization; this lets both countries retain their identity. This entry intentionally covers fresh first-day offers only: later bought-for-later/pending-offer state, sale decisions and settlement still require their management owners.

## Rendering boundary

`world12_initial_desktop` is read-only. It projects actual human state, persisted layouts/prestige, stocks, confirmed options and active seat into the existing Original desktop renderer. Global club IDs remain separate from temporary local drawing indices. The selected crest uses its explicit atlas/cell rather than a narrowed legacy badge identity. A typed setup override preserves Germany/England flags and permits the already supported zero-duration choice without weakening ordinary legacy admission.

Desktop and drawer renderers accept an explicit numeric reference image for this path. Their ordinary constructors still call the existing verified legacy loader. `StartupFlow::attach_world12_career` checks all confirmed names/portraits/global club IDs before constructing and publishing the screen. The controller stages the Session until the UI construction succeeds.

The development entry now displays the actual first desk. Initial entrant/draw/ticket and unsolicited-offer notice presentation is still pending even though the drawn results/receipts are retained. Most desktop commands and subsequent calendar days remain unintegrated and are rejected through the typed owner; they cannot fall through into legacy management or disk handlers. End-session uses the existing quit confirmation. Do not mark the package playable or remove the development flag on the strength of this checkpoint.

## Persistence and checks

`OBMH12S6` adds the initial event, manager order, open/empty coupon and optional offer notice to the desktop runtime. S4 and S5 remain readable without invented event state; WORLD12 JSON remains version 15. Validation checks event/active-seat/order agreement, actual seller and bid references and the empty day-zero calendar boundary.

The source-data validator independently runs the existing full Core `prepare_event` and `enter_event_desk` against the typed transaction. It covers ordinary no-offer entry and forced offer-producing seeds, with loan terms and salary-only approaches, comparing quote/player/bidder/amount/term/warning, prestige, matchday number and both RNG seeds/draw counts. Synthetic session tests exercise both offer branches in six setup scopes, global IDs, save roundtrips, replay/interruption rollback and rejection of a day-zero appointment. Actual input tests additionally attach English and mixed English/German desktop views. Native Windows/macOS and full-season/release QA remain open.

## English strength-table correction

Inspection of the first rendered English desk exposed near-zero player technique/condition. The FLTD translator had extended a preceding one-byte object across English alignment padding instead of mapping the actual eighteen-byte league-strength table. The original English strength-reset routine explicitly reads the table at `EnglishLeagueStrengthRanges`; the translation now copies it to `LeagueStrengthRanges`. No table values are embedded in the public source.

Re-export `RULES-EN.DAT` for existing development packages. The loader rejects an English top-division upper bound of zero with that instruction. The private v9 data package replaces only this rule resource relative to v8 (plus its manifest/report); the world database and artwork remain byte-identical. This also corrects the shared English legacy translation rather than treating the tiny displayed strength as a rendering problem.
