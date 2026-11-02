# Annual promotion planning

`game/world12/season_transition` plans one enabled country's exchanges from its
complete final league results and round cursors. It does not advance a career.
The existing season-end guard remains required.

The planner uses the same relegation and promotion ordering helpers as the
Original `season.cpp` path. Final tables use the existing standings adapter,
three points per win, all home/away results and the stored manager point
sanctions. Stable club and player identities are never packed into legacy team
fields.

## Required decisions

The annual owner supplies one licence-decision record for every league, including
an explicitly evaluated empty record where no licence was withdrawn. Financial
licence evaluation, its messages and random draws remain that owner's duties.
Decisions reference the occupants at that group's execution boundary: earlier
group exchanges may already have moved a club into this league.

Germany additionally requires an explicit champions-only or completed play-off
disposition. Completed play-offs supply the two winner draw slots; the planner
orders each stored winner before the next Regionalliga champion. A winner can
belong to the other Regionalliga; Source field order determines the pairing. Without
play-offs, Original exchanges only two clubs at the second-division boundary,
even though its departure list contains four. Parallel Regionalligen retain
separate bottom-two quotas and together receive four clubs from the Oberliga.

England uses its five catalogue tiers, including the scheduled ten-club fifth
division, with four exchanges at each boundary. The AI-only Premier League in a
Germany-only career has no annual English ladder.

The owner also supplies the complete, ordered newcomer draw from actual named
non-league clubs of that country. No newcomer is synthesized. A human manager at
the amateur edge retains the club and its league slot, while that visible pair's
newcomer draw is still required, matching Original protection and draw ordering.

## Slot lineage and publication

Original freezes final ranks by draw slot, then swaps the occupants of those
slots group by group. The plan preserves this order. Using a final ranked vector
of stable club IDs directly for every later group would lose the identity that
an earlier exchange moved into a ranked slot.

`before` and `after` contain league membership projections. Group receipts retain
the full display lists, exchanged prefix and protected-manager rows. Revalidation
rebuilds the plan from the current world and full source result/cursor inputs,
rejecting changed or modified plans. Planning takes constant inputs and consumes
no randomness; failures cannot partially publish membership or player changes.

A non-protected amateur exchange marks `requires_amateur_roster_renewal`.
Original's club/amateur swap retains the league slot's squad rather than moving
a nonexistent amateur squad into it. Typed worlds currently forbid a roster on
a non-league club. Consequently this module deliberately exposes no World or
Session application API: copying membership alone would break squad ownership
and validation.

Full annual publication additionally needs settled transfer obligations,
licence evaluation, roster renewal and loan/statistics lineage, awards and annual
accounts, final shared/domestic qualification, new cup fields and fixtures,
calendar rebuilding, seasonal resets and the closing/new desk owners. They must
be staged and committed together with the two random streams and session
revision. Plan tests establish ordering and rejection behavior; they establish
neither a completed season transition nor a playable Beta.
