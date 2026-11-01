# Initial fixture positions and transfer market

`initialize_first_season_market` follows initial competition and manager-service preparation. It publishes the Source league-position shuffle, initial market, list parity, negotiation game-seed checkpoint, final W seed and one revision together. It does not advance the first day or attach a graphical desktop.

Club/player identity and previously allocated cup/title identities stay fixed. Each league membership vector receives the Original random-swap sequence, including its repeated draw when the byte-sized position is unchanged. Actual fixtures then use those new positions. Global cup participants and holders continue to refer to the same clubs.

The existing Core and typed owner share entry-count allocation, alternating club scans, positional surplus scores, quicksort, 35 player-choice attempts, final-attempt behavior and quote arithmetic. Free-agent choices use the eligible, unlisted pool in its stored order. Human clubs are excluded from automatic club listings. All current 1.2 squads have 24 slots, as in expanded 1.1: the physical large-squad group contains all league clubs; the small-group request retains the Source fallback and its original small positional targets.

World schema 15 stores forty regular plus four special optional rows. Player, seller and four bidder identities are explicit global references. Fees, salaries, asking/demand percentages, remaining weeks, terms and seven bonus values are scalars. No packed Native player/club word or raw 0x58-byte listing is saved. A bounded one-player workspace reuses market valuation without restricting the full world to 4,095 IDs. Initial rows leave the four special slots and bidder identities empty.

Missing candidates, duplicates, invalid references, replay or late owner changes reject both the shuffle and listing candidate. The accepted world and seeds remain unchanged; no substitute offer or match result is manufactured.

The actual German/fallback validator compares shuffled membership, every listing/player/seller, price/duration, parity/checkpoint, RNG streams and snapshot with Native Core. The reference 17/91 startup produces eleven listings and consumes W=1906 in first steps. An independent audit compiles the pre-refactor selection/listing/shuffle routines from `b82eefd`; sixteen seed pairs pass. Actual-data checks also initialize all six club/national-option scopes. Synthetic cases cover deterministic replay, human exclusion, preserved cup/title/player identities, snapshot persistence and full rollback when no AI club can supply a player.

First-day calendar services, visible desktop/menu/session adoption and the remaining competition/economy/season/release gates remain open. Initial market preparation alone is not a playable 1.2 release.

Validation of this milestone: all 27 targeted Linux regressions and the graphical application build pass. The six actual-data scoped startup/market snapshots pass, the initial-offer predecessor remains G364823/3131 W840250195/12390, and two supplied Original saves roundtrip byte-identically. No native Windows/macOS or visible-career readiness is claimed.
