# Typed career initialization

World schema 8 records whether the fresh player pool has been initialized. It also retains an optional initial account baseline (starting capital and account flags). Older worlds leave these initialization fields absent/defaulted; they are not silently regenerated once managers or played records exist.

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

`world12_creation_validate` compares the private Germany/Premier-League-fallback dataset with direct `startup_players`: all 3,334 player records and both RNG streams match. It then compares manager contracts, ages, board, initial account, lineup and RNG with the direct known Source phase. This does not certify the earlier club-strength/placement/stadium selection, markets/moods, national-only manager ownership, complete human matches or GUI career binding. Those remain completion gates, not guessed defaults.
