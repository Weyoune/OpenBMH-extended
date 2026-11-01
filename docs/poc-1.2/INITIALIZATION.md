# Typed career initialization

World schema 8 introduced the marker that records whether the fresh player pool has been initialized. It also retains an optional initial account baseline (starting capital and account flags). Older worlds leave these initialization fields absent/defaulted; they are not silently regenerated once managers or played records exist.

## Player pool

`make_player_initialization_plan` derives the Original 54 extra-player entries and ranked squad groups from the immutable complete master, before scope filtering. Germany-only club play follows the Original order Germany / Spain / England / France / Italy, including the Premier-League fallback. Full German/English play initializes the two domestic ladders followed by the remaining international leagues. National-only play keeps the complete player pool; using the master groups for initial strengths does not retain club entities, fixtures or transfer markets in that mode.

Strength ranges come from the supplied German/English rule images. Every player is generated through the existing `generate_player` implementation; interpolation and W/G draw order retain the Original startup rules, including its deliberate new-career attribute/age behavior. Names, nationality metadata, IDs and roster membership remain unchanged. No player may be omitted or generated twice.

Generation is one transaction with both RNGs. The initialized marker, attributes and seeds publish only after full validation and live-owner checks. Stale/external changes are preserved and reject publication. Reinitialization and re-scoping initialized players are blocked.

## Manager account and board phase

`initialize_manager_contracts` implements the known `CareerAddManager` phase **after** club-strength/placement and stadium selection. It is not the complete manager-creation owner. It reuses `new_board_entry`, the existing automatic lineup and the shared `initialize_career_manager_funds` helper. The legacy career calls the same funds helper, preserving its write order and values.

This phase sets player ages using the Source W draws, generates the Original 2-year contract/salary/individual-order entries for every occupied row, enables the four standard formations, starts the assistant settings from the supplied initial rules and creates the initial lineup. Funds follow difficulty and the normalized domestic financial tier. Initial capital and account flags persist separately from current cash, credit and cumulative counters; ordinary bookings preserve that baseline.

The application wrapper validates seat, stable club, session scope and revision. World/account/board/ages/roster/cards, the revision and Watcom state commit together. A changed owner or random stream during initialization rejects the candidate without overwriting the external change. Existing account/tactical state cannot be initialized again.

Unoccupied initial marking words are normalized to the explicit no-player sentinel; zeroed BSS must not invent a link to the first global player. Formation data must actually be supplied, rather than synthesizing an initial table.

## Evidence and remaining integration

Synthetic checks cover all seven scopes, deterministic generation/reload, malformed plans, replay and external world/RNG ownership. Forty additional manager cases compare the account/board phase directly with Core calls in both domestic ladders and at difficulties 0/1/6/9, including initial-capital persistence and interrupted-owner rollback.

`world12_creation_validate` compares the private Germany/Premier-League-fallback dataset with direct `startup_players`: all 3,334 player records and both RNG streams match. It then compares manager contracts, ages, board, initial account, lineup and RNG with the direct known Source phase. This does not certify the earlier club-strength/placement/stadium selection, national-only manager ownership, complete human matches or GUI career binding. Those remain completion gates, not guessed defaults.

Schema 9 additionally preserves the known initial human match settings, staff ratings and histories through the shared Source team-settings initializer. The real creation probe now runs an isolated human league round after the known initialization phase; it still does not certify earlier club/stadium selection or the complete GUI career.

## Markets and initial club moods (schema 10)

`initialize_career_markets_and_moods` follows startup player generation and precedes the age/free-player and manager phases. Both career paths share the fund/share prefix, random stirring and per-club initial-mood calculation. The typed path runs the existing thirty stock-market days. Twelve complete price/history/trend records, six complete fund records and the default-manager-name ordering are persisted; old worlds leave this optional state absent.

Club order follows the active domestic ladders, the international league order, domestic extra clubs, remaining cup clubs and national teams. A bounded Core workspace runs the existing squad averages and club balance, including the foreign-strength draws for non-roster teams. Domestic extra clubs retain the empty-squad average; the primary country's national pool retains that Original special case. Germany is primary when enabled, England in England-only club careers. Stable IDs and memberships never change during this phase.

The phase stages all stock/fund/mood/flag changes and both RNG streams. Missing reference records, repeat initialization, played careers or late owner/RNG changes reject publication. The real Germany/Premier-League-fallback probe compares all 445 club moods/flags, complete share/fund state and both random streams with `new_career_markets_and_moods`. Synthetic fixtures exercise all seven scopes, domestic extra/cup/national entities, persistence, older-schema absence and interrupted ownership.

The market/mood milestone alone did not yet include `new_career_players`; that club-player phase is implemented below. Club placement/strength/stadium selection and visible GUI career ownership also remain open. Market/mood validation does not certify those stages.

Validation of the market/mood milestone: all 25 targeted Linux regressions and the graphical application build pass. A private audit compiles the two pre-refactor market/mood routines from commit `396db0c` separately and compares sixteen seed pairs: 53,344 generated player records, 7,120 club mood/balance rows, all stock/fund bytes and both RNG streams match. Subsequent account/board and isolated human-round probes pass for each pair. The initial-offer regression and two supplied Original-save byte-identical roundtrips also pass. This is Linux/source-level evidence; native Windows/macOS and full visible-career QA remain pending.


## Ages, bounded free players and initial overflow (schema 11)

`initialize_career_player_pools` follows generated players and initialized markets. Roster order receives the exact Original age-bit OR and familiarity draws. The 54 extra players then use the shared release regeneration: retain condition/technique/form, clear injuries and transient skills, retain nationality/position, reset the experience bonus and draw the release age. Stable player identities are unchanged.

The persistent free pools have 80 domestic and 20 foreign entries. Domestic means a player from an enabled playable country: both Germany and England in a combined career, Germany alone with the Premier-League fallback, England alone in an English career. These are explicit shared-market extensions of the Original domestic/foreign split. Pool entries are compact, unique global IDs, disjoint from club rosters; malformed or missing references are rejected. Worlds through schema 10 leave the new state absent.

A full pool invokes the Original candidate-score kernel, first-pass foreign skipping, second-pass retry and quicksort ordering. Playable domestic ladders use the Original domestic capacity; AI international clubs retain the foreign capacity. All typed squads use the large-roster positional targets. A successful overflow appends the player and runs the existing automatic lineup through a bounded 24-player workspace, retaining global IDs, role flags and card-row synchronization. A missing eligible club rejects the whole operation; there is no unlimited overflow pool. Fresh typed offers are empty, so the legacy stale entry-40 bidder quirk is not invented for nonexistent offers.

The Source `transfer_bid` now calls that same numeric score kernel, retaining its eligibility rules, late-season need, positional targets, entry-40 bidder count, draw order and subsequent offer handling. The shared release/age helpers keep Native Classic on the same arithmetic. The 21st-player contract repair already present in Core remains available to manager initialization.

`app::world12::initialize_club_player_state` publishes generation, markets/moods, ages, releases, both RNGs and one session revision together. A stale scope/revision, interrupted owner, malformed later-phase input or exhausted overflow destination leaves the accepted earlier world and streams intact. Club placement, selected strength/stadium, season setup and the visible GUI owner remain separate required steps. National-only player-pool ownership is still pending and is rejected explicitly by this club-specific operation.

The data preparation tool also corrects a discovered initial-slot import error: all four Original high-bit flags are preserved instead of treating every non-starter as a substitute. New English squads mark rows 11–16 as bench and later rows as unselected. The private v7 development pack corrects 954 flags while retaining every ID, player, league membership and asset.

Validation: all 25 targeted Linux regressions and the application build pass. An independent private audit compiles the pre-refactor age/release/transfer routines from `6dd9622` and earlier market routines, comparing sixteen seed pairs: every generated/aged/released attribute, bonus/card value, final squad, bounded free-pool identity, stock/fund byte and both RNG streams match in the German/Premier-League-fallback world. Forty-eight additional pre-refactor Native transfer/bid comparisons at rounds 0/12/24 match the entire serialized state and both streams, including high bids. Two Original saves remain byte-identical, and the initial-offer regression passes. Synthetic tests cover six club scopes, more than 4,095 players, second-pass-only placement, no-room rollback, snapshots, malformed references and stale/changed session owners. These are not native Windows/macOS or complete GUI-career checks.
