# Atomic weekly services and monthly listing renewal

`process_weekly_services` publishes the complete existing weekly sequence as one
transaction: manager offers/facilities/advertising, auction countdown/settlement,
new global bids, player recovery/injuries, manager settlement, familiarity decay
and human training. It aggregates the actual draw counts from every stage. A
cancelled presenter, failed later stage or external owner change discards the
entire staged week, including earlier accepted choices and transfers.

The caller still owns the source weekday, current competition relationships,
active global manager, interactive presenters and any following month change.
The wrapper does not advance a date, replace monthly billing or implement general
saved event ownership. All presentation choices remain explicit.

The longer integration comparison found a transfer-news bug: the interested-
manager routine inferred leagues from temporary local team numbers. Typed
workspaces now compare the actual country and division of the manager, seller and
buyer. Unrelated international transfers no longer leak into domestic managers'
mailboxes. Ordinary legacy lookups retain their existing behavior.

`renew_monthly_market` supplies the new-listing part of `month_change`: allocation
by current occupancy/free-pool counts, eligible free agents in physical order,
alternating club selection and Source positional quotas, valuation/percentages,
first free regular slots, and matching search-profile news. It does not reshuffle
season memberships or move players. Existing listings, bids, pool holes and the
market parity switch remain intact.

All current 1.2 roster capacities are24, including international clubs. The
listing routine therefore uses the Source large-squad boundary for the complete
club collection, preserving the small-request fallback and its different wanted
position counts. A foreign country alone does not make a24-slot club a small
squad or add a small-club valuation premium.

National protection is supplied as explicit global player IDs. Existing loaned or
listed players are excluded. If a reserved-return count leads selection to an
empty physical place, no player0FFF record is read; that place is unavailable,
and an unrepresentable final Source attempt rejects the staged phase. Human clubs
are not automatically listed. Search news uses the shared Core matcher and is
retained in the persistent mailbox. Both helpers keep the existing world18/outer
S9 schemas and reject late state changes before publication.

Validation covers complete-week determinism, rollback after an earlier incident
but failed auction acknowledgement, late callback changes, wide-ID/holey free
pools, monthly listing invariants, snapshots, protection-context validation and
the unrelated-transfer-news regression.

The controlled actual-data comparison runs eight independent eight-pass sequences:
64 complete Core weekly-plus-training transactions match player state, all club
rosters, full human accounts/boards/career state, mail, aggregate draws and saved
snapshots. Dates and competition context are deliberately held fixed in these
service fixtures: these are not64 played weeks or complete careers. Twelve
monthly-listing comparisons generate96 entries with full, holey and scarce free
pools; subjects, owners, quotes, parity, search news, RNG and snapshots match.
Twenty-four frozen-baseline/current legacy economic-week outputs remain
byteidentical. No fresh Original DOS recording is claimed.

Monthly services, including advertising expiry, loan returns, manager billing/
contracts and funds/credits, are now composed by the separate atomic
[monthly service transaction](MONTHLY_SERVICES.md). General saved events/daily
progression and GUI binding remain open. A playable1.2 release must prove those
paths in the actual application.
