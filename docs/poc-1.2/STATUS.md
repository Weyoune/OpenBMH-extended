# 1.2 development status

The application is **not yet a playable 1.2 release**. Branch `1.2` remains isolated from 1.1 and stable main.

Completed foundation:

- Thirteen country-aware league descriptors, 234 league clubs, German/English ladders and national cup ownership. Table group selectors expose Germany/England/Europe; only Germany and England are selectable manager countries.
- Strong 32-bit player/club/league/crest identities and separate roster flags. A checked JSON world codec preserves IDs beyond 4,095/65,535 and crest IDs beyond 511 without packed truncation.
- Full 10-team home/away fixture generation; result stores scale with each league's actual round/match count. Original 18/20-team schedules remain supplied content, not guessed replacements.
- Version-separated O1.2 foundational snapshots retain world data, round cursors/results and both RNG seeds. These snapshots are not yet the complete application save path: finances, active cups and suspended transaction ownership still need integration.
- An explicit bounded AI roster projection maps global identities into a legacy match workspace and maps player changes back with stale-input checks. Human projection is rejected until its tactical/financial ownership is implemented; it is not silently treated as AI.
- Private development OBD: 234 league clubs, 527 total club/country records, 4,734 players. Existing 1.1 records/IDs are retained; new source membership is restricted to supplied men's country leagues. No Portuguese league is included; its original cup-only opponents remain. English tier 2–4 retain source-tier candidates before the reduced fifth group; fictional roster attributes, membership adaptations, amateur fillers and temporary crests are documented.

Validation: synthetic typed-codec, wide-ID, mixed-manager, cross-country loan, fixture uniqueness, save separation/truncation and mapping/ownership tests passed on Linux. The private real-world database also passed decode/encode and every referenced crest atlas was readable. New crests are temporary, explicitly fictional preparation assets.

Added calendar/progression foundation:

- Country-qualified import of both supplied full-season calendars; Germany and England keep separate national-cup/supercup entries, while European/national entries have one shared owner. The English 10-team group receives 18 evenly distributed league dates.
- A shared daily cursor prepares deterministic fixture batches for all leagues on that date. Complete validated score batches commit atomically; partial/stale results and double advancement are rejected before mutation. This coordinator accepts scores from the match layer and does not generate outcomes or consume RNG itself.
- The development O1.2 snapshot schema now retains the complete calendar, next-day cursor and revision. Save validation rejects round/results/calendar inconsistencies. Unresolved cup or national appointments explicitly block league-only advancement.
- Synthetic tests completed all 365 daily transactions and 4,058 league fixtures, saved/reloaded midseason and at season end, and checked mixed same-day country rounds and rollback. These are coordinator tests with test scores, not simulated career matches. Both real supplied calendars imported with all 13 expected league round counts and 75 non-league appointments.

Added weekday alignment and league-table projection:

- A checked reader extracts the English DOS date-setup operands directly, instead of reading German instruction addresses from a translated data image. Unsupported layouts, lengths and invalid operands are rejected. The supplied English source starts on Saturday, 1 July 1995; the supplied German source starts on Wednesday, 1 July 2026.
- Calendar sources can now be aligned to the common weekday epoch using the nearest whole-source shift. The supplied English calendar moves by +3 days, preserving every appointment's weekday and all 75 retained non-league appointments. An appointment crossing the season boundary causes an explicit error rather than wrapping or disappearing. Tests cover all 49 weekday combinations and both 365/366-day boundaries. The application still needs to install this prepared calendar when the new career path is connected.
- A read-only table adapter handles all 13 leagues with global club IDs, home/away and round-range filters, two/three-point modes and explicit point deductions. It reuses Classic's bounded per-league calculation and tie ordering, including its original eight-bit goal totals and form/rank-history behavior. No alphabetical replacement of tied clubs or global-ID truncation is introduced.
- Synthetic comparisons checked 1,080 table configurations directly against the existing Classic implementation for 18/20-team leagues. The newly scheduled ten-team English group separately completed a balanced 18-match-per-club table. Result validation now rejects scores outside the Classic 0–15 goal range at the model/save boundary as well as at day commit.

Remaining: shared application progression, cup handling and promotion settlement, human manager/tactics projection and match-event translation, application save ownership, German country picker and all dependent UI views, complete editor integration, three-season/multiplayer/native release QA. The old loader intentionally rejects the new profile until these gates are complete. No 1.2 binaries are released at this stage.

Added domestic cup score/progression adapter ([details](CUPS.md)):

- Country-owned original stage metadata, wide club identities, separate legs/results/revisions and original aggregate/away-goal resolution. FA Cup and League Cup qualifying stages retain their seeded clubs and linked main/final stages.
- Shared daily transactions can now publish complete league and supported domestic-cup result batches atomically. Missing draws, unresolved ties, incomplete successor stages or foreign/stale results leave every competition and the date unchanged. Indoor/shared/national events still block this domestic-only adapter.
- The development snapshot schema now retains complete cup stage histories and pending return legs. Synthetic tests completed all three domestic cup structures with 203 test-score matches and daily save/reload. All eight supported stage descriptors imported successfully from the private supplied source rules; those metadata probes use synthetic draws.
- Actual original entrant selection/draw calls and their RNG/effects, match simulation, premiums and human/title statistics remain application integration work. This is not a completed playable cup career or a 1.2 release.

Added draw audience and startup symbol:

- Initial and subsequent domestic draw notices are emitted only for countries with a human manager in their current league ladder. Lower divisions count; cup elimination does not hide the remaining country draws. AI-only countries continue processing the same results and stage transitions, with unchanged dates and RNG. The country-independent European/national audience policy remains part of its still-pending adapter.
- The startup screen on the `1.2` working branch now has separate country controls (see the newer scope contract below). Each control retains its native 51×40 Original frame. The source-only three-control layout remains available for Classic comparisons. The preview accepts `--league-symbol source|countries`.
- `tools/world12/prepare_setup_symbol.py` copies Germany, England, globe and load controls from the supplied Original atlases at 1:1 resolution, preserving the Original crossed-out states. Private `KB/SETUP12.VGA` is now a 204×80 four-column/off-on atlas. The world builder takes `--english-data` and includes it. Legacy packages without it keep their source layout. Source hashes and method details remain private; Original sprite bytes are not added to the public repository.

Added automatic domestic entrant selection and draws:

- Initial German/FA/League Cup fields now follow the original division quotas and explicit supplied rankings. English seeds are shuffled separately and retain the reversed reserved-prefix order. The existing legacy code and the typed adapter share the initial/subsequent shuffle and national tie-ordering kernels.
- A new application owner stages complete country result batches, automatic next-round/successor draws and their RNG seeds, publishing them together. First legs and terminal finals make no next-field draws. Hiding an AI-country draw changes only presentation notices. Unsupported/mismatched source rules, bad rankings and late result errors roll back all staged state and randomness.
- The automatic full domestic programme completed 203 test-score matches with daily save/reload, exactly 176 initial GameRandom draws and 420 subsequent Watcom draws. The private real typed world and both supplied source rule images also generated their initial fields and passed the save roundtrip.
- Existing seeded-cup synthetic regression and private original-save season/follow-up draw regressions passed, including original participants, group traversal, overview effects and exact RNG assertions. These are Linux engine tests; the combined 1.2 application remains incomplete and no actual match-score simulation or playable career is claimed.

Added independent country/national selection ([contract](GAME_SCOPE.md)):

- The setup now provides Germany, England, International and Load as four complete Original controls, each with its original off/on state. International retains the national-team meaning confirmed by the user; European club competitions belong to club play.
- A fresh-world scope filter removes disabled Germany completely. Disabled England retains its Premier League as international AI opposition, removing lower divisions, amateur entries and domestic cups. IDs are preserved. Disabled leagues have no memberships, rounds, results, cup initialization or calendar events. National-only scope retains no club leagues.
- Scoped world schema version 2 and the existing development snapshot preserve the selected world; full version-1 master data remains readable. All seven scope combinations have synthetic year, conditional-cup, absent-entity and save/codec checks.
- The country-selection request is reserved for the typed manager/GUI-career adapter. Legacy session confirmation explicitly refuses it, preventing a country-selection screen from starting the old wrong-country game while 1.2 integration remains incomplete.

Added actual AI scoring-phase integration ([contract](MATCH_INTEGRATION.md)):

- Typed AI fixtures now call the existing Original-derived prepare/lineup/rating/90-minute engine path. Country-aware runtime contexts replace local-ID inference for league/crowd, climate and the original foreign-player rule. Human-owned participants remain explicitly blocked; unrelated managers do not block AI fixtures.
- Player card words, roster order/flags, club match/formation flags and morale persist with stable IDs. Full staged publication rejects stale players/clubs/manager ownership and illegal roster moves. World codec version 3 retains the new fields; earlier master schemas remain readable. Projected local states cannot masquerade as legacy career saves.
- Sixteen synthetic fixtures and repeats passed actual scoring, direct positional-rating equivalence, English foreign-rule behavior, persistence and late rollback checks. Seven additional real-data fixtures using supplied Original reference state completed and roundtripped their post-scoring state.
- The older Capture checker has a pre-existing incomplete side-record oracle result. A separately built clean pre-change commit produced byte-identical output; this is documented as a diagnostic limitation, not an Original-oracle pass. Full after-match settlement, human matches and GUI career integration remain pending.

Added AI post-match processing ([contract](AFTER_MATCH.md)):

- A separate settled-match API reuses Original club-record/streak updates, morale and player development/card/absence processing. Complete league rounds run on one shared Original minute clock before group settlement; source effects are not interleaved by simulating/settling fixtures serially.
- Club records now preserve global opponent IDs independently of Original 16-bit name-pool offsets, including historical opponents outside the current group. Global development balance/rotation persist with schema version 4 and stale-stream checks. Earlier master schemas retain defaults; played records cannot be re-scoped as fresh source data.
- Direct Core equivalence in five league contexts, four shared-clock round/repeat cases, record-offset validation, snapshot roundtrips, fifth-yellow/injury/appearance edges and explicit signed development wrap are covered. Seven real-data AI fixtures also complete the settlement path.
- Full-day/week/month ownership, human tactics/finances, optional awards, forfeits and GUI career integration remain pending. The APIs return settled AI outcomes but do not silently advance a career date or claim a playable 1.2 release.

Added mixed BMH/FLTD other-games UI ([contract](OTHER_GAMES_UI.md)):

- Eight competition controls use the requested two-column layout. Super-Cup and Charity Shield share one UI page with separate country headings/results/panels and a non-clickable gap; their engine competitions and saved scores remain independent. A single enabled country retains one centered final.
- Typed domestic cup views expose stored rounds/stages, Source detail windows and complete view. Existing assets/renderer, Core club averages, detail labels and extra-time/penalty/return-leg formatting are reused. Uninitialized European/national data is explicitly unavailable rather than manufactured.
- Typed owner identities and local-token/global-club mappings keep navigation/team-info requests distinct from legacy engine IDs. Legacy dispatch refuses typed requests. Model and supplied-asset geometry/action checks cover the merged finals and eight selectors; private demo views are explicitly labeled synthetic examples.
- The overview is ready for the pending typed GUI-career owner. Initial supercup selection and remaining competition backends are not claimed complete.

Added first persistent human-manager state ([contract](MANAGER_STATE.md)):

- Optional manager finances preserve cash/credit, income/expense counters and paid-player-bonus totals in world schema version 5. Older manager states remain explicitly uninitialized; no starting balance is guessed.
- A narrow legacy financial extractor and typed booking adapter reuse Original `book_money`, including credit checks, forced/statistical bookings, no-money mode and word wrapping. The application owner checks seat/club/scope/revision and publishes only changed state with its revision; applied payments cannot be replayed.
- 288 direct-Core financial cases, ownership/codec/snapshot tests and private Original-save extraction/booking checks passed. These do not convert the old career, initialize a complete new career or unlock human match simulation.
- Source-compatible initial money, tactics/lineups, contracts, bank policy, daily finance and GUI-career integration remain pending.

## Revised England fallback policy

The latest plan supersedes the earlier Portugal proposal: profile `dual-classic-2`, thirteen league slots (0–12), no Portuguese league. England enabled means its complete 20/20/20/20/10 ladder; England disabled during club play means only the twenty-team Premier League, AI-only in the international tables. Its IDs and roster data remain unchanged. Full England has three other international leagues; fallback England preserves the original four-country international order Spain/England/France/Italy. Germany remains wholly absent when disabled.

Expected league-only coordinator totals are 4,058 fixtures with both countries, 2,828 with Germany only, 2,528 with England only, and zero with national-only play. These are scheduling counts, not a claim of a playable career. Snapshot magic `OBMH12S8` (S4–S7 remain readable without invented newer state) rejects older development arrays; surviving league and entity IDs are never renumbered. The private data revision removes only added Portuguese-league players/clubs and restores the seven original Portuguese cup-only rows without squads.

Validation of this revision on Linux: all 14 targeted regression tests passed, including all seven scope combinations and full league-only coordinator years. The private 234-club/4,734-player dataset passed crest-resource checks and all scoped codec roundtrips. Six source-backed AI matches in full scope and five with Germany/Premier-League fallback completed actual scoring, after-match processing and snapshot roundtrips. Eight source cup-stage metadata checks and original-asset overview tests passed. The importer rebuild matches every club, player, crest and league record of the privately migrated dataset. Private screenshots document the conditional supercup overview; Windows/macOS and complete playable careers are not claimed.

## Actual AI league-day transactions

The [AI day owner](AI_DAY.md) now stages Original scoring, shared-clock group settlement, persistent player/club development, league results, both RNG seeds and the calendar/date transition together. Failures and stale ownership retain the previous complete day; human fixtures and all non-league appointments require separate owners and are explicitly blocked. Empty league dates consume no match RNG. This does not perform daily economy or unlock the GUI career.

Synthetic direct-Core comparison covers 261 actual fixtures across full, Premier-League-fallback and England-only dates, with deterministic save/reload repeats and late-group/coordinator rollback tests. With the private development dataset and supplied fixture tables/weekday-aligned league calendars, three dates per scope completed 685 actual matches (292 full, 227 German/Premier-League fallback, 166 English-only), plus deterministic repeats and snapshot roundtrips. Non-league source appointments were explicitly excluded from that isolated probe. All 15 targeted regression tests passed on Linux; native Windows/macOS and complete career validation remain pending.

## Human tactical scalar state and actions

[Known tactical fields](MANAGER_TACTICS.md) now persist in world schema version 6: four instructions per row, position/partner rows, automatic weights and the formation selector. Earlier worlds retain an explicitly missing tactical state. Owner-bound edits reuse the Original ORDER, placement and automatic-weight helpers with checked global-to-local player mappings. Human match simulation, full board/formation initialization, lineup swaps and GUI career binding remain pending; human participants are still blocked from AI scoring.

Validation: 340 supported tactical cases match the direct Core helpers, plus twelve ORDER edits and scalar extraction from a read-only supplied Original save. All 16 targeted Linux regression tests and the application build passed. The private development world passed all seven scoped roundtrips; the source-calendar probe still completes 685 actual AI matches with deterministic repeats and snapshots. No native Windows/macOS or playable human-career validation is claimed.

## Prepared human lineup exchange

The typed tactical owner now supports manual pre-match roster exchanges. A shared Original-derived Core kernel is used by both the existing legacy lineup dispatcher and the typed calculator. It preserves slot roles/coordinates, moves player/tactical entries, synchronizes card-row identities and selects manual mode. The typed application publishes validated global roster IDs, tactical fields and cards as one staged change. Injured players and foreign-rule cross-role exchanges remain blocked pending their confirmation flow; this is not live substitution or a completed human match adapter.

Validation: 40 typed/native swap scenarios compare the shared kernel with independent byte-level expectations, including full native board-entry movement, exact role/auxiliary flags and all marked card rows. The typed result retains global IDs, finances and snapshot/replay ownership. Existing 340 tactical cases and all 18 targeted regression tests passed on Linux, along with the application build. No live substitution, GUI binding or native-platform human career is claimed.

## Full tactical backing and formation ownership

World schema 7 retains all numeric tactical row state, explicitly translated global man-marking references, supplied formations and masks, automation and stored indoor-lineup mode. Formation application, custom saving and preparation reuse the Original helpers, including their country-aware foreign-rule context and bounded bench-coordinate tail. Scalar-only older states reject operations requiring missing backing. All affected board, roster, cards and club mode/flags publish together.

156 additional full-backing cases, Original-save extraction, all 18 targeted Linux regressions and the private 685-match source-calendar probe passed. The GUI career and human match pipeline remain pending; this is not a playable-release claim.

## Player generation and initial account/board phase

The [typed initialization path](INITIALIZATION.md) now generates the selected player pool through the Original generator with stable IDs and deterministic scope ordering. Its one-time marker and both random streams publish atomically. The new manager board/account phase creates contracts, ages, formation settings and initial funds through shared Source helpers, preserving initial capital/account flags and preventing replay. Forty direct-Core manager cases and the seven-scope generator checks pass. The private 3,334-player fallback world, RNG streams and manager phase match direct Original calls. Earlier club-placement/strength/stadium selection and full GUI/human career integration remain pending.

Validation of this initialization milestone: all 19 targeted Linux checks and the application build passed. The private source-backed creation probe compares all 3,334 players, both random streams, the known manager board/account phase and final lineup/cards. The existing initial-offer predecessor regression passed, and two supplied Original saves still roundtrip byte-identically. These checks do not certify the not-yet-integrated markets/moods, club-placement/strength/stadium phases or human GUI career.

## Typed league predictions

The [shared forecast kernel](PREDICTION.md) now supports country-qualified 10/18/20-team tables without packed club IDs. Source two-point prediction tables, future-win rates, tie/word behavior, goal transitions and stale-size semantics are preserved. Typed forecasts return read-only data for the upcoming human match owner. 600 typed/native comparisons, 50 ten-team cases, 15,120 pre-refactor Core comparisons and all 20 targeted Linux regressions pass; the application builds. Human live/scoring ownership remains pending.

## Classic country/club manager UI

The [manager selection](MANAGER_SELECTION.md) now follows the requested flag/crest layout. Germany/England toggles choose the first available first-division club, retaining name/portrait/strength; the crest opens the Original picker with twenty English cells and current country tabs. Typed immutable contexts preserve wide IDs/crest resources and disabled-country/locking rules. The setup flow has a checked typed-selection handoff and newer-context continuation between managers. Source-backed images and input checks cover both countries and a two-manager flow; the complete typed career/desktop owner is still pending.

Validation: all 23 targeted Linux checks and the application build pass. The real 1.2 package renders both country states and all twenty English crests; UI tests also cover a two-manager continuation and a synthetic global ID above INT_MAX. Existing setup/manager rendering tests pass with the legacy reference package. Comparison images, input-world hash and selection JSON are recorded privately; this is a pre-career UI state, not a played save.

## Human league computation and shared day owner

[Human match state and ownership](HUMAN_MATCHES.md) now retain policy, staff, ratings, timing/streak statistics, recent/global opponent history and designated players in schema 9. Club fairplay points also survive projection and snapshots. Sparse two/four-manager workspaces receive country-specific size/rank/cursor/forecast context and independent formation inputs; AI-only APIs still reject humans.

The actual Source minute and settlement path completed 56 synthetic human/AI fixtures with 115 goals and six human event presentations, deterministic repeats, complete snapshots and late-owner rollback. A real-data probe completed nine more fixtures (17 goals) with its human manager and source rules. The mixed league-day service publishes human/AI results, accounts/stats, seeds and one date transition together, rejecting replay and late-group failure. This is not the full visible career: cup/national/indoor play, remaining creation/economy phases and desktop/live UI ownership are still completion gates.

Human-league milestone validation: all 25 targeted Linux regressions and the application build pass, including sparse two/four-manager projection, real human rating/development, events, complete mixed-day publication and late rollback. The pre-refactor forecast audit still matches all 15,120 cases. Source creation/account/board checks and the isolated real-data human round pass. This does not prove a playable GUI career or certify the remaining competition/economy/creation stages.


## Initial markets and club mood persistence

World schema 10 retains complete stock/fund state and default-name order after the Original market/mood phase. The typed initializer reuses the Source market prefix, thirty market days, squad averages and shared per-club balance calculation. Direct private comparison matches all 445 Germany/fallback club moods and flags, all share/fund bytes and both RNG streams. The seven-scope synthetic initializer now covers market persistence and late-owner rollback as well as player generation. Earlier entries describe the evidence at their respective checkpoints; the remaining age/free-player phase, manager placement/stadium and complete GUI owner are still open.

Market/mood milestone validation: 25 targeted Linux checks and the application build pass. Sixteen independent pre-refactor routine comparisons cover 53,344 generated-player records and 7,120 club mood/balance rows with matching stock/fund state and RNGs. The original-offer initialization regression and two byte-identical Original-save roundtrips pass. No release/native-platform/full-career readiness is claimed.


## Bounded free players and complete club-player initialization

World schema 11 retains the 80/20 compact free-player pools with explicit global identities. Roster age/familiarity and release regeneration reuse shared Original arithmetic. Initial overflow uses the shared transfer candidate score, Original skipping/sort order and bounded automatic-lineup workspaces; full dual worlds are not restricted by packed 12-bit player IDs. The application owner publishes all three player-creation phases and both streams under one revision. Synthetic cases exercise six club scopes, a world above 4,095 players, the second-pass retry, no-room failure and full-transaction rollback. National-only ownership is still explicitly pending.

A source-backed comparison also exposed and fixed loss of roster flags in the preparation tool; the private v7 pack corrects 954 row flags without changing stable IDs, player records, membership or artwork. Earlier reports refer to the inputs at those checkpoints. Manager placement/strength/stadium, season and visible-career integration remain completion gates.

Player-release milestone validation: 25 targeted Linux checks and the application build pass. Sixteen independent pre-refactor initializations match player/bonus/card data, final rosters, bounded pool identities, market state and both RNGs. Forty-eight Native transfer/bid scenarios at rounds 0/12/24 match full saved state and streams. The private v7 package hash manifest verifies; comparison to v6 proves only 954 roster flags changed. Original save roundtrips and the initial-offer predecessor check also pass. No complete visible career or release-ready claim is made.


## Complete initial manager confirmation and saved career settings

The [new confirmation owner](MANAGER_CREATION.md) stages country-local placement, Original strength resets, supplied stadium tables/upgrades, youth/environment budgets, starting sponsors and the established contract/account/lineup phase. Source league/extra exchanges inherit the displaced squad; all stable identities remain intact. World schema 12 retains career rules and semantic facilities/environment/sponsors, with explicit global poaching references. Bounded human projections preserve those values and reject policy/owner changes.

Validation: all 26 targeted Linux checks and the graphical application build pass. Ninety synthetic confirmations cover both ladders/durations/strengths, plus four mixed seats, extra squads, ten-team/reserved placement and rollback. An independent pre-refactor audit matches 288 full confirmations across sixteen seed pairs. The real 4,734-player combined dataset initializes four managers from both countries with their supplied rule images, calculates thirty-three human/AI fixtures across both countries' top/lowest leagues and roundtrips its snapshot. Original save roundtrips and the initial-offer predecessor check pass. First-season setup, visible GUI career and the remaining full-season/services/release gates stay open.


## First-season fields through the initial domestic draws

The [competition-preparation owner](INITIAL_SEASON.md) now recalculates club lineups/balance, draws the initial national field, assigns/fills the European competitions and draws the enabled domestic cups, then installs the supplied calendar. Schema 13 preserves global participant identities and manager season records. Both streams and one revision publish atomically. English-only play removes German clubs and explicitly redistributes the missing league/cup places without a Portuguese league.

Validation: all six club scopes pass with actual supplied German/English rules/calendars and with synthetic fixtures. Sixteen pre-refactor comparisons match the complete initial-draw boundary, including campaign/card records and both RNG streams (seed17/91: G180/W470). All 27 targeted Linux regressions and the application build pass; Original save roundtrips and initial-offer regression remain unchanged. This ends before tickets/seasonal player services and first-day progression. The visible career and release gates remain open.


## Initial ticket sales and manager-season reset

The first-season owner now follows competition preparation with all ticket sales, then the Original seasonal manager reset and advertising generation. Scalar advertising/ticket/history data persists in schema 14; regular contract backing excludes the designated-player extension and canonicalizes explicit sponsor fields. Global domestic title IDs remain stable under future shuffling. Local league/round context is used for both countries. Sixteen independently compiled pre-refactor comparisons match retained records, accounts/tactics and random draws; actual-data checks cover six club scopes, and synthetic checks also cover twelve alternate mode/duration/difficulty cases. The next start gates are fixture shuffling, initial transfer market, first-day services and visible desktop/session ownership.

Manager-service milestone checks: all 27 targeted Linux tests and the application build pass; actual six-scope service/snapshot validation, the initial-offer regression and two Original-save byte-identical roundtrips pass. No new GUI screen or release readiness is claimed.


## Initial fixture-position shuffle and market

The [first-step owner](INITIAL_MARKET.md) now publishes Original membership shuffling, initial listing selection/quotes, explicit player/seller/bidder identities, parity/checkpoint and both retained seeds under one revision. Shared control kernels preserve the Native alternating scan, positional ranking, 35-try behavior and small-request fallback. Snapshot schema 15 contains no packed Native listing references. The next integration gates are daily/calendar progression and the visible typed career/desktop.

Initial-market validation: 27 targeted Linux checks and the application build pass. Six actual-data scopes, sixteen independent pre-refactor startup/shuffle/list comparisons, the initial-offer regression and two Original-save byte-identical roundtrips pass. This milestone does not certify daily progression, visible gameplay or release readiness.


## Typed session and live startup controller

The [session boundary](SESSION_BOUNDARY.md) now owns selected data, revisions, manager confirmations and startup preparation. The same controller drives actual Original setup/manager screens and synthetic/real input checks. A data-only reference workspace and bundled English rules remove runtime dependence on M3D.MAN or Original executables. The private v8 package adds those rules without changing world/artwork and remains non-playable metadata. The explicit --world12-dev entry reaches prepared startup, with an honest stop before first-day/desktop ownership. Actual SDL setup/England screenshots and an on-disk prepared-state roundtrip are recorded privately.

Validation of this integration checkpoint: all 28 targeted Linux regressions and the graphical application build pass. The real v8 one/two-manager GUI-input paths, actual SDL setup/England toggle, on-disk prepared snapshot, six source-data startup scopes, initial-offer predecessor and two byte-identical Original-save roundtrips pass. All v8 archive hashes verify; only RULES-EN.DAT was added relative to v7, and no legacy master/executable is present. First-day/desktop gameplay and native release QA remain open.


## Persisted desktop entry boundary

[Desktop runtime state](DESKTOP_STATE.md) now retains per-manager Original layouts/prestige and active-seat/autosave state in S5 snapshots, with S4 read compatibility. Staged startup preparation installs the layouts. A checked initial DeskDate transaction reuses the shared Original routine with typed human state and country-specific indoor visibility. Event preparation, pre-match offers/pools and visible desktop adoption remain separate integration gates; the development stop is still required.


## First empty event and visible typed desktop

The [first-event owner](FIRST_EVENT.md) now prepares the actual initial manager order, CurrentMatchday, unsolicited offer and empty Toto coupon before DeskDate. The development SDL path attaches the Original desktop using the typed session, checked global manager identities and explicit crest resources. Source first-event comparisons include no-offer, loan and salary-only cases with exact financial fields and random streams. S6 retains event/order/offer metadata and reads S4/S5. Initial competition/receipt presentation, management actions and later calendar days remain open; this is not playable-release certification.

Milestone checks: the graphical application builds; 28 targeted Linux regressions, six synthetic event/offer scopes, actual English/mixed startup-to-desktop input tests, and the Diamond 3.6 legacy desktop fixture suite pass. A native SDL/Xvfb click-through reaches the English first desk; screenshots and its corresponding S6 development snapshot are retained privately. Nineteen independent full-Core first-event comparisons include eighteen offers, one loan and ten salary-only approaches with matching financial fields and both RNG streams. The English numeric rule export was rebuilt as private v9 and checked against the supplied raw strength table; world/artwork hashes are unchanged. No native Windows/macOS or complete playable-career certification is claimed.


## Original preparation menus

[Lineup, pitch, tactics and training](PREPARATION.md) now have a typed session owner and explicit Original renderer binding. Menu entry and commands commit with UI acknowledgements, including stable temporary player IDs across roster changes and global designated-taker references. Training/tactics share the legacy request calculations; calendar and RNG seeds remain unchanged. S7 retains the native display word and reads S4–S6. Other menus and day advancement remain open.

Preparation milestone checks: all 28 targeted Linux regressions and the application build pass. Actual English and mixed English/German GUI-input paths exercise roster exchange, player info, pitch pages/weights/formations/orders/takers, tactics and training; actor IDs, display state and saved snapshots survive roundtrips without time or RNG advance. Seven existing Classic UI suites and the legacy training-dispatch suite pass with their Diamond 3.6 fixture. Actual SDL/Xvfb input additionally opens/edits/closes the menus; comparison screenshots and S7 saves are retained privately. The full-Core initial-season/19-first-event comparison still passes. Windows/macOS and full-career/release QA remain open.


## Graphical typed save/load

[Save/load](SAVE_LOAD.md) now connects the Original setup/desktop file dialogs to the typed Session and S7 container. Restore uses saved world/settings/event data without initialization or RNG draws; manual save stages its four Source header draws and publishes an independently verified exclusive temporary before updating the live owner. Directory confinement, overwrite confirmation and corrupt-file rejection are retained. Initial-owner gameplay is supported; general day/event progression and automatic save scheduling remain separate gates.

Save/load milestone checks: all 28 targeted Linux regressions, the existing Classic file-dialog/desktop/preparation suites and the application build pass. Actual controller input tests cover successful save/confirmed overwrite, corrupt load and bad-target rejection, metadata bounds, setup/desktop restore and reopening saved pitch state. Six synthetic scopes prove exact unchanged resume and zero resume draws; manual save retains its four Source draws. Atomic file tests exercise no-clobber collisions, confined paths, directory/symlink rejection and temporary cleanup. Actual SDL/Xvfb input additionally saves/overwrites/loads, forcibly terminates the test process, restarts with a different startup seed and loads the retained file through setup. Screenshots and saved files are retained privately. Windows/macOS native execution and general day/event/career validation remain open.


## Initial manager-loop handoff

The [retained manager cursor](MANAGER_LOOP.md) now advances the Original desktop to the next human seat in the existing shuffled order and survives save/load. Mode-I four-manager German/English input tests and an independent full-Core four-seat comparison cover all positions, layouts/prestige and both random streams. Pending player decisions, pending autosave and the final daily/weekly processing boundary remain explicit barriers; no day is skipped. S8 retains the cursor and reads S4–S7. This is not completed day advancement.

Manager-loop milestone checks: all 28 targeted Linux regressions and the existing Classic desktop/file/preparation suites pass; the graphical application builds. Actual GUI-input tests create four alternating German/English Mode-I managers, traverse each saved shuffled position once, roundtrip S8 snapshots and preserve the explicit final-day barrier. The full-Core four-manager comparison matches every retained layout/prestige and both RNG streams, including Source after-desk scanning. An actual SDL/Xvfb run reproduces the handoffs and loads a nonzero-position snapshot in a fresh process. Screenshots and per-position saves remain private. Mode-III pending decisions and complete daily/weekly processing remain open.


## Initial player-offer settlement

[Pending-offer ownership](PENDING_OFFERS.md) now stages the existing full Core
salary/negotiation/severance/sale/AI-loan path, publishing qualified roster changes,
accounts, transfer history, loan contracts and both random streams together.
S9 retains these fields and reads S4–S8. German/English dialogs use the actual
manager crest and explicit rendering reference. Completed decisions survive the
still-explicit final daily-service boundary. Missing/cancelled presenters and
stale/repeated decisions retain the previous state. Complete daily/weekly/monthly
processing, loan returns and the remaining management/match/release work remain
open; this is not a playable 1.2 release.

Milestone validation: 28 targeted Linux regressions, eight real-data
German/English GUI decision cases and a four-manager Mode-III outgoing-loan
handoff pass. The independent full-Core matrix compares complete contract/tactics
boards, canonical rosters/accounts, transfer history, loan boards and both RNGs
for 72 decisions (30 sales, two loans). Actual SDL mouse runs cover German
decline and English sale, save and fresh-process load. Both resulting saved
careers exactly match controller settlement plus the four manual-save draws.
This stronger check found and fixed a dialog-release click-through into training.
Classic desktop/preparation/file/pending-dialog suites pass. Original-derived
screenshots, test saves and receipts remain in the private QA repository.


## Weekly player and training phases

[Player recovery/injuries and human training](WEEKLY_PLAYERS.md) now operate on
the entire typed world through checked bounded workspaces. The ordinary Core
weekly path shares the extracted per-slot routine and retains its behavior.
Country policy and training round cursors use actual league context, including
England's playable-versus-AI-fallback distinction. Returned messages retain raw
Source text and globally qualified recipients. Career/RNG staging rejects late
external changes and existing snapshot fields retain the resulting state.

A 4,680-player synthetic test and ten actual-data full-Core comparisons cover
injuries, recoveries, retirement, doctor effects, assistant lineup changes,
training, complete tactical boards, messages and both random streams. Twenty-four
complete pre-change/current Core weekly-and-training outputs are byteidentical.
The final daily/weekly owner remains incomplete: manager incidents and settlement,
market evolution and persistent news still need integration before the GUI may
advance. No calendar/service shortcut or playable release is enabled here.

Milestone validation: the application builds and all 29 targeted Linux regression
checks pass. Representative typed weekly/training phase snapshots and the
pre-change/current Core comparison receipts remain private. These phase fixtures
precede desktop ownership and are not claimed as GUI-resumable played careers.


## Persisted economy, messages and account/weekly settlement

[World schema 16 economic state](ECONOMY.md) now retains deposits, interest totals,
loan contracts by global borrower/lender seat, weekly camp/offer/construction data,
financial/facility histories and the persistent Source mailbox. Fresh GUI careers
initialize these verified fields explicitly. Pending transfer news and weekly
injury notices survive saves; old schema 1–15 inputs still load without invented
missing economic state. The S9 outer save framing is unchanged.

Daily account processing distinguishes post-event interest from a plain day's
stock movement. Weekly manager settlement reuses the Core calculation, retains
rolling histories and maps holdings/loans correctly for sparse managers. Late
state changes discard staged processing. Eight actual-data economy comparisons
and 24 byteidentical frozen/current full Core economic runs pass; the application
builds and all 30 targeted Linux regressions pass. The earlier actual GUI startup,
save/load, pending-decision and four-manager cases also pass with the new state.

Remaining: weekly incidents and interactive choices, market evolution, monthly
services, mailbox display and earlier setup-message delivery, general daily/event
ownership and the larger match/career/release gates. No calendar increment or
playable release is enabled by these individual phases.

Actual Linux SDL mouse checks additionally retain the new economy/mailbox state
through a German decline and English sale, manual save and fresh-process load.
Both saved careers exactly match the controller after-state plus the four manual
save draws. Actual-data GUI tests explicitly verify that accepted sale/loan news
is persisted. Screenshots, corresponding saves and comparison receipts stay in
the private QA repository.


## Owned weekly incidents and advertising

[Weekly manager offers and facilities](WEEKLY_INCIDENTS.md) now have an explicit
decision callback and staged publication of offers, ageing/damage, construction
completion, advertising, compensation, messages and both RNG streams. Missing or
cancelled answers no longer mean an implicit decline in the typed path. Current
competition/title relations are mandatory qualified inputs. Bank rate metadata
comes from the supplied reference; no rates are guessed.

Schema17 retains planned block targets/flags and the submitted balance plan. It
also fixes sponsor backing overwriting two remapped designated-taker references;
older typed backing is normalized without changing the canonical player IDs.
The S9 outer save container remains unchanged.

All eight question kinds pass yes/no/cancel/missing-presenter checks, along with
sparse actor ownership, stand completion, saved targets, taker isolation and stale
rollback. Twenty-eight actual-data full-Core cases include a credit-interest
change, ticket refunds, advertising and complete field/mail/RNG comparisons.
Twenty-four complete predecessor/current Core economic weeks remain byteidentical.
Market evolution, monthly services, general event/day ownership and the weekly
GUI presenters still need integration; no time-advance shortcut is enabled.

Milestone validation: the application builds and all 31 targeted Linux regression
checks pass. The weekly decision GUI is not wired into time advancement yet;
controlled phase saves and Source comparison receipts remain private.


## Global regular-market computer bidding

[Global bidding](MARKET_BIDS.md) now considers every actual AI club while using
bounded numeric rating workspaces. It retains regular slot identity, duplicate
and capacity checks, reserved loan players, late table-based recruitment, price
and term draws, free-agent behavior and complete weekly new-offer gates. The
Source shortage and bid-key quirks remain explicit; no local subset is presented
as a global market.

Schema18 persists qualified special-entry40 bidder history across decisions.
Initial references are mapped through selected membership; missing old facts are
marked unknown and rejected when needed. The outer S9 container is unchanged.
A synthetic >4,095-player fixture finds its sole eligible buyer at the last club.
The full-Core matrix matches 374 states with 296 bids, including controlled late
rounds, shortage/history and complete weekly bid phases. Twenty-four complete
legacy economic weeks and RNG traces remain byteidentical.

Countdown/expiry, human auction presenters, actual movement/settlement, monthly
renewal and the full daily/event owner remain open. This milestone does not move
a player or calendar, or certify a playable release.

Milestone validation: the application builds and all 32 targeted Linux regression
checks pass. The actual-data Source comparisons include regular and high bids,
five successive calls through full rows, complete weekly gates, late-table needs
and retained special-row history. Controlled bid-phase snapshots remain private.

Actual-data GUI input tests also pass with schema18 and mapped initial/retained
bidder history, including startup, management, save/load, pending decisions and
four-manager loan handoffs. No new bidding screen or playable day advancement is
claimed; the new phase changes market bids and their Watcom stream only.


## Regular-market countdown and actual settlement

[Auctions](MARKET_AUCTIONS.md) now stage regular listing countdown, shared Core
ranking/selection, expiry and season-end settlement with actual AI/human/free-agent
roster changes. Human choices/notices and allocator observations are explicit.
Accounts, full boards, persistent mail, Source history and loan-owner contracts
publish together; rejected or cancelled work cannot leave partial typed state.
Source paid-but-blocked outcomes are preserved rather than reported as movement.

Free agents and loan references have explicit non-roster mappings; no fake club
is introduced. Acquisition clears the exact native pool slot instead of compacting
later entries. Sixty actual-data full-Core cases compare 48 moves and 18 loans,
including both human managers, free agents, history/owner-board metadata, full
rosters/accounts/mail, both RNG streams and saved snapshots. The test fixture uses
actual remapped manager clubs and includes the second manager's external memory.
Twenty-four complete predecessor/current Core economic weeks remain byteidentical.

Monthly renewal/returns, the ordinary auction GUI and complete saved event/day
ownership remain open; this phase alone does not enable calendar advancement.

Milestone validation: the application builds and all 33 targeted Linux regression
checks pass. Free-pool validation now permits native holes while preserving unique
ownership. Controlled auction-phase saves and independent Source comparison
receipts stay private; the ordinary auction GUI is not yet connected to time
advancement.

Actual-data GUI controller checks also pass: setup, preparation, graphical
save/load, German/English pending decisions and four-manager loan handoffs. This
protects existing interaction paths; no claim is made that the new ordinary
auction presenters have been wired into the GUI.


## Atomic weekly sequence and monthly new listings

[Weekly services](WEEKLY_SERVICES.md) now compose all existing weekly stages and
training into one staged publication. Later failures roll back earlier incidents,
transfers and random streams. The complete-sequence test found and fixed transfer
news using temporary team numbers instead of actual country/division ownership.

Monthly new-listing renewal now shares Source allocation/selection/valuation and
search-profile matching, preserving physical free-pool holes, existing rows and
parity. All current24-slot international clubs retain large-squad policy. No
season shuffle, player movement or date increment is introduced by renewal.

64 controlled full-Core weekly-plus-training passes (eight repeated sequences)
and twelve monthly renewal cases with96 new listings match fields, rosters,
accounts/boards, mail, draw counts and snapshots. Service fixtures hold their date
and competition context fixed; these are not claimed as played weeks/careers.
Twenty-four baseline/current legacy economic-week outputs remain byteidentical.
Loan returns and the other monthly steps, saved general events/daily progression
and actual GUI adoption remain open.

Milestone validation: the application builds and all 34 targeted Linux regression
checks pass. Actual-data GUI controller tests also pass for startup, preparation,
save/load, pending decisions and four-manager loan handoffs. Controlled complete-
weekly/monthly-listing snapshots and comparison receipts stay private; the new
service sequence is not yet a GUI calendar-advance path.

## Monthly loan lifecycle

The shared return phase now restores AI/human owners, requires owned borrower
questions and return-offer editors, and preserves physical ledger/board holes
through schema19/SA snapshots. See [loan return boundaries](LOAN_RETURNS.md).
The36 actual-data countdown/expiry comparisons are controlled service fixtures;
general day advancement and the remaining monthly services remain unfinished.

## Complete monthly service transaction

The staged monthly sequence now composes sponsors, listings, loan returns,
per-manager billing/contracts/retirement and funds/credits. World20 retains
monthly categorized counters and the history cursor; outer SA is unchanged.
Typed1.2 deliberately credits dividends to each holder, preserving the old
Classic default policy. Controlled complete-month and cancellation/full-pool
checks are described in [monthly services](MONTHLY_SERVICES.md).
The general day/event owner, GUI presenters and played-career release gates
remain unfinished.

## Ordered daily services

Finance, weekly processing/training and month changes now share an atomic dated
service wrapper with authenticated clock/entry policy. A separate post-event
market tick preserves the Source event-seeking order and stops at the annual
desk boundary. Seventy dated full-Core fixtures match; they deliberately omit
match/calendar execution. See [daily services](DAILY_SERVICES.md). Saved event
ownership and GUI day progression remain the next integration gate.
