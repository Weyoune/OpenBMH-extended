# Shared human league clock

A scheduled day now calculates all human-involved league rounds together on one
Original `MatchEngine` minute clock. The batch contains one to four complete
leagues in ticket order, with at most 80 rostered clubs and 1,920 roster players.
It uses the existing projection, lineup preparation, side records, match phases
and post-match settlement. AI-only leagues keep their separate bounded passes.

The day owner validates the complete ticket and condition payloads before opening
a human presenter. It runs the human batch first, followed by AI-only leagues in
ticket order, and returns league receipts in the original ticket order. This is
an intentional change from sequential league calculation: later AI settlements
cannot appear in the staged state shown before human minute one. Multiple human
leagues consequently consume RNG in the Original multi-group order rather than
as independent matches. Draw counts belong to the shared batch/day, not to its
individual league receipts.

Each descriptor retains the Original country-local calculator/option class.
Canonical league identity is obtained from the projected actual fixtures; global
league IDs are never placed in Original calculator fields. Group vectors and
descriptor storage stay stable throughout synchronous live callbacks. Both
`before_matchday` and `before_owned` run once before minute one; the latter sees
the actual staged day state, including prior gate receipts. Retained canonical
live preferences publish with the final match state.

The backend rejects stale/repeated rounds, incomplete or unowned league groups,
invalid conditions and concurrent owner/RNG mutation. Calendar and result
publication remain the outer day owner's atomic transaction. Cancelling or
throwing from a presenter publishes neither match effects nor consumed RNG.
Domestic cup, European and indoor clocks keep their separate existing owners;
this component does not establish a common clock across competition families.

Public synthetic tests compare two/four human leagues against a single direct
Original multi-group engine, including every score/end minute, full World,
player/finance/monthly/mailbox state and exact game/Watcom RNG. They also cover
80-club projection limits, saved preferences, stable callback storage,
pre-minute ownership, cancellation and reentrant-state rejection. Both league
and combined day owners use the same staged league calculator. These bounded
checks do not establish a played full-calendar or three-season release gate.
