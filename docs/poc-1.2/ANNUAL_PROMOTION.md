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
a non-league club. Copying membership alone would break squad ownership and validation. The
unpublished membership stage described below now owns that lineage; there is
still no Session season-publication API.

Full annual publication additionally needs settled transfer obligations,
licence evaluation, roster renewal and loan/statistics lineage, awards and annual
accounts, final shared/domestic qualification, new cup fields and fixtures,
calendar rebuilding, seasonal resets and the closing/new desk owners. They must
be staged and committed together with the two random streams and session
revision. Plan tests establish ordering and rejection behavior; they establish
neither a completed season transition nor a playable Beta.

## Saved decision and membership stages

`evaluate_annual_promotion_decisions` now evaluates real Source licence finances
from the canonical account, deposit, fund/share holdings, remaining loans,
creditworthiness, existing sanction and transfer balance. It retains Source's
human-manager eligibility marker across group exchanges. AI withdrawals use the
same chance draw and rejection sampling over actual draw positions as Original;
newcomers use the shared named-club/duplicate rejection kernel. Both random
streams remain staged in an immutable input-bound receipt.

The Source human withdrawal loop can exceed the nominal quota when more human
clubs lose their licences. Those extra human departures are retained. Source's
AI branch decrements a zero remaining quota without a guard; this unsupported
negative-count case is rejected explicitly. No extra balanced exchange is
invented for it.

World schema36 adds optional canonical physical amateur slot order for Germany
and England. Fresh player initialization captures the initial master order,
manager placement updates an affected amateur slot, and annual membership
staging replaces each sampled newcomer with the relegated identity in that
same physical slot. Disabled country pools are empty. Older active careers keep
this order unknown and cannot infer it by sorting current stable IDs.

Schema36 also adds the optional eighteenth promotion contract flag byte in
`ManagerSeasonServices`. Source marks seventeen ordinary contract rows and the
separate manager tail byte; adjacent designated-player references remain
untouched. Initial services capture the Source byte. Older unknown values remain
unknown and reject a promoted-human membership stage requiring that byte.

`stage_annual_membership` validates the full decision receipt and a known settled
transfer market, then returns an **unpublished** canonical World. At the amateur
edge, the newcomer inherits the actual league-slot roster, the outgoing club
becomes rosterless, and loan parties retain their Source league-slot lineage.
Player identities, historical player/club statistics and stored opponent
identities remain stable. Discipline counters retain league-slot ownership
until the later seasonal reset. Licence and promoted-manager mail follows Source
group order; promoted contracts retain the exact flag effects.

This stage has no Session commit API. It does not execute annual accounts,
academy donor selection, awards, final qualification, player ageing/retirement,
seasonal resets, calendar renewal or closing/new desks. The complete outer annual
transaction must own those prerequisites before publishing the staged World,
new seeds and saved next-season calendar together. Repeated synthetic stage tests
verify physical pool persistence and lineage; they are not played seasons.

Player-facing English management exposes four divisions while the catalogue
retains its fifth internal group. Annual planning still follows that internal
catalogue; it does not decide the unresolved Source four-division human boundary
by silently exposing a fifth division. Membership staging rejects a manager
newly entering a hidden division until the full annual player-ladder owner
settles that policy. Existing/synthetic hidden-division managers remain valid
for read-only world/save inspection. No internal membership or roster is dropped.
