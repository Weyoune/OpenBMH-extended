# Monthly services

`process_monthly_services` stages the complete monthly sequence: advertising
expiry, new market listings, loan returns, then each human manager's accounts and
monthly squad bookkeeping, followed by funds and credit installments. It publishes
one snapshot and aggregates the actual game/Watcom draw counts. A late failed or
cancelled contract screen discards earlier sponsor, listing, return and account
changes as well. The caller still owns the calendar, date and human presenters.

The monthly board uses the existing Core kernel. Contract months, insurance,
retirement wishes, mood, experience, the twelve-month value histories and the
Original restart after a departing player follow that kernel. A successful
renewal must actually update the staged contract; an acknowledgement alone is
insufficient. Missing human decisions fail explicitly. No automatic acceptance or
refusal is substituted.

Departing players use the shared regeneration and roster/board compaction. Both
enabled domestic countries use the domestic free pool; other nationalities use
the foreign pool. Physical pool holes are retained. If the appropriate pool is
full, the two-pass no-list recruitment scoring considers all eligible AI clubs
with their actual country, capacity, current rank, reserved returning players and
retained entry40 bidder references. The chosen club receives an actual lineup.
Global IDs are mapped through a bounded workspace, including multiple departures
in one board pass. Loan records and saved owner boards are cleared for departures.

Monthly economic state now includes the twelve-month history cursor, two monthly
expense values and fifteen categorized salary/income/expense counters. These
fields are optional in world schema20. Older snapshots remain readable, but a
monthly transaction refuses an absent monthly backing instead of inventing it.
The outer SA framing is unchanged. Stock/fund holdings and human credits retain
their global manager-seat mapping, including sparse active seats.

## Deliberate dividend correction

The Original fund routine retains only the last manager's dividend and books it
into the record after the active managers. Typed1.2 careers explicitly select the
corrected policy: each holder receives their own dividend in their own account
and interest-income counter. Inactive seats are untouched. The ordinary Classic
caller retains its previous policy. Price growth and bank/human credit payments
continue through the shared routine.

## Verification and limits

Synthetic tests cover mixed-country sparse manager seats, sponsor expiry/renewal,
billing and ratings, actual-holder dividends, matured bank credits and human
loans, missing monthly backing, contract refusal/renewal/cancellation, repeated
roster compaction, a full free pool, wide IDs, snapshots and late whole-month
rollback. A claimed renewal without a changed contract is rejected.

Actual-data controlled comparisons include nine repeated accounting transactions
and nine complete monthly transactions with six contract/retirement departures.
They check rosters, player values, full human boards/accounts/career state, pool
positions, messages, aggregate random consumption and saved snapshots. The full
comparison enables the same explicit dividend correction in the Core oracle.
These fixtures do not advance dates and are not played seasons, fresh DOS
recordings or native Windows/macOS qualification.

General saved event ownership, daily progression, GUI presenters and full match/
season flows remain release gates. This module alone does not make1.2 playable.
