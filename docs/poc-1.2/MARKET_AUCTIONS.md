# Regular listing countdown and settlement

`process_market_auctions` stages the existing Core `transfer_list_countdown` /
`decide_bids` / settlement path for every occupied regular row, in physical order.
It supports normal countdown, expiry without bids, season-end settlement,
AI/human sellers and buyers, ordinary free agents and outgoing loans. It does not
create new bids, renew monthly listings, return loans or advance the calendar.

Every row receives a bounded roster workspace containing the human managers,
seller and bidders. Explicit additional player/name references represent free
agents, existing loan parties, owner boards and marks without fabricating clubs
or rosters. Named extra clubs do not acquire fictitious squads. Their existing
human match history is retained. Global identifiers never enter packed fields.

Human selection and acknowledgement hooks are mandatory when the shared routine
needs them. A selector may explicitly decline; missing/cancelled presenters reject
the staged phase. The caller supplies the current global manager when no saved
desktop owns it. Human purchases require an explicit allocator observation.
Original paid-account/refusal/blocked outcomes are retained; a cleared listing
does not imply a player moved. The actual roster establishes movement.

A successful transfer may change exactly the subject's seller/buyer membership.
Free-agent acquisition clears its exact 80/20 pool slot. Other pool slots stay in
place, matching native removal and preserving later random selection positions.
World validation therefore accepts free-pool holes while still rejecting missing
or multiply owned players. The serialized array dimensions and outer S9 framing
remain unchanged. A free-agent transfer-history counterparty is an explicit
invalid/no-club ID rather than an invented club.

The adapter retains accounts, full tactical/manager state, transfer balances and
Source-generated history, persistent mail, loan parties/months and human-owner
contract snapshots with explicit global mark references. It does not invent a
history row where the Core did not write one. Existing human-owned loans require
their owner boards; missing boards cannot silently reset loan limits. Existing
loan order is retained and new loans appended. Physical hole handling for future
monthly returns remains part of the return-lifecycle integration.

AI buyer foreign-quota checks now read the projected actual country. Legacy
states retain their ordinary positional country lookup. Stale callback changes
are detected before publishing the complete phase and both random streams.

Validation covers AI transfers, human decline/cancellation and missing selectors,
free-agent native pool holes, loans to AI and human buyers, human-owner snapshots,
subsequent sales retaining earlier loans, missing-board/allocator rejection,
allocator-blocked paid outcomes, quota country mapping, expiry/season-end handling,
owned notices, snapshots and extra-name-reference history preservation.

Sixty controlled full-Core cases compare 48 moves and 18 loans. They include
AI↔human, human↔human and free-agent transactions, full player/cards/bonus state,
canonical rosters and pool slots, complete manager records, transfer history,
loan metadata/owner boards, mailbox, both RNG streams and snapshots. The added
second manager explicitly includes native youth/environment memory, and actual
manager-team mappings are used after Original career setup reshuffles clubs.
These are service fixtures, not complete played careers or new DOS recordings.
Twenty-four predecessor/current complete Core economic weeks remain byteidentical.

The ordinary auction GUI is still not attached to the daily owner. Monthly
listing renewal, loan returns and general event/date ownership must follow before
a complete playable 1.2 is claimed.
