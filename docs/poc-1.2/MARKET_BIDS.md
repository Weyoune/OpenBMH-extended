# Global computer bids

`add_computer_market_bid` and `generate_weekly_market_bids` implement computer
bidding for the forty regular market rows. The weekly phase belongs **after**
countdown/settlement. It does not expire listings, move players, replace human
auction decisions, create monthly listings or advance the calendar.

Candidates traverse the saved Germany/England/Europe membership order. Every
eligible AI club is considered; human clubs are excluded. Bounded 32-club
workspaces collect numeric team averages while identities remain global. Ratings
are reused within this read-only bidding phase. A buyer at the very end of the
full world therefore remains eligible even with more than 4,095 players.

Source scoring, sorting, positional needs and price/term calculations are shared.
Late recruitment uses each club's actual league and table rank. Returning loan
players count against the owning club's reserved capacity. Playable English clubs
receive domestic capacity policy; England's international AI fallback receives
foreign policy. Free-agent bids consume the Source loan roll but always produce
an ordinary contract. Existing physical bid slots, duplicate rejection, full-row
behavior, weekly gates and repeated-offer draws are retained.

Listed players are supplied through pool slots in Source. Consequently the AI
seller is scored before being rejected during selection, and the ordinary
human-roster injury G gate does not apply. The existing Core's absolute-index
bid-key approximation and last-rated-club need are retained instead of silently
substituting the winning score/need. This preserves current Core behavior; it
does not solve the previously documented unknown Original stack-value quirk.

World schema18 additionally retains the four qualified bidder references from
special entry40 after its offer is resolved. Source's shortage bonus reads that
record even when its pool slot is empty. New career initialization reads verified
numeric reference tokens and maps them through the selected membership order;
no disabled-country club is imported. New offers and completed decisions update
the retained references without packing global IDs.

Older schemas remain readable. A live entry40 can supply its own bidder facts;
otherwise missing history is marked unknown. A calculation that needs that
shortage input rejects unavailable facts instead of inventing them. Initial market
callers without a supplied reference likewise retain unknown rather than guessed
history. The S9 outer container is unchanged.

Both bidding helpers stage the complete career and Watcom seed. Publication
rejects external changes; no roster, financial, calendar or game-RNG mutation is
manufactured. The regular new-bid loop is also extracted as Core
`new_transfer_bids`, preserving ordinary legacy weekly processing.

Validation includes a synthetic large world with wide IDs and only one eligible
buyer at its final club, free agents, duplicate/full rows, reserved loans, special
history, empty-week gates, repeatability and callback rollback. The actual-data
matrix compares 374 full-Core states containing 296 generated bids: all buyer
identities, fees, salaries, terms, seven bonuses, both RNG streams and snapshots
match. Controlled late-table and shortage/history fixtures supplement initial
market and complete new-bid phases. Twenty-four predecessor/current full Core
economic weeks and RNG traces remain byteidentical. These are controlled Core
comparisons, not newly recorded Original DOS gameplay or complete career tests.

The [auction adapter](MARKET_AUCTIONS.md) now supplies countdown, expiry ranking
and actual roster/account/history/loan settlement. Human seller/buyer presenters
still need GUI integration, and monthly renewal/returns remain open.
Those must be integrated with daily ownership before enabling playable progress.
