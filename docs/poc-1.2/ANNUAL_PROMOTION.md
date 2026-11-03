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

The annual owner supplies one licence-decision record for every Source promotion league, including
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

England uses the four visible Source groups, with four exchanges per group.
The fourth group refills directly from the ordered named amateur pool and
protects a human fourth-division manager. The hidden ten-club internal division
retains its complete membership, roster and separate simulation role; it does
not enter this promotion ladder. The AI-only Premier League in a Germany-only
career has no annual English ladder.

The FLTD last-group code can reuse an uninitialized stack newcomer after a coin
flip. OpenBMH applies the common Diamond 3.7 defined named-pool rejection
sampling instead, following the single-engine policy. It does not infer an old
stack value, choose an arbitrary replacement or consume a placeholder draw.

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

Player-facing English management and annual exchanges both use four visible
Source divisions. A relegated fourth-division human remains in that league;
the paired real newcomer draw is still consumed. All internal fifth-division
membership, rosters, statistics and full final standings remain available and
unchanged by promotion. Existing/synthetic hidden-division managers remain valid
for read-only world/save inspection.

## Closing awards and schema38 annual facts

`calculate_annual_awards` reads each enabled domestic country's actual club draw
order, roster counters and discipline. It shares the Original positive-score
top-ten insertion behavior, first-occurrence scoring ties, signed discipline
arithmetic, `Games` fair-play tie order and local-tier prize table. Foreign-only
leagues are excluded. The internal fifth English data group remains available to
the calculation without becoming a player-visible starting division.

`stage_annual_awards` requires the genuine July closing progression boundary and
complete actual league results/cursors. It stages Source income booking and
mail in kind-first catalogue order, preserving date, both RNG seeds, rosters,
player archives and competition ownership. Source difficulty0 still announces
awards without creating money. `publish_annual_awards` commits the staged result
under the shared scope/revision owner; missing text/accounts and stale owners
cannot leave partial payouts.

Schema38 stores the number of seasons whose awards have actually been paid.
Fresh first-season services establish zero; each successful closing publication
advances it once. Reload cannot pay the same awards again, and older unknown
completion is not inferred. This is a closing-phase fact, not proof that the
next season has been created.

Schema38 also retains Source annual account words (previous tax/profit), league
title count, national eligibility and seasonal player-award count. Fresh Source
manager capture supplies their real values; ordinary projections preserve old
unknown backing. These fields prevent later accounts/refunds and yearly resets
from inventing zero carries or discarding counters. A league-title count does not
replace the still-required per-season global-ID title archive.

An exhausted calendar is now valid at its owned closing boundary: appointment
lookup does not read beyond the final day. It still cannot claim a scheduled
match event after calendar exhaustion. Annual accounts, actual academy donor
selection, ageing/retirement, qualification/title archives, renewed calendars and
closing/new desk presentation remain required. The Session annual guard remains.


## Shared annual account and renewal kernels

`settle_manager_annual_accounts` exposes the complete existing manager account
pass with explicit completed-title and next-European-entry relations. The
native caller still reads its original competition fields. Both paths retain
summer events, youth settlement, bonuses, academy search, credit, tax/refund,
ticket income, title premiums and the shared fine carry across managers.

`create_for_annual_accounts` supplies the actual 90-club domestic donor pool in
physical membership order. Other active humans retain their real records and
rosters as manager-count/exclusion context beyond that pool. Only a manager in
the first 90 clubs can execute the account kernel; normal match factories keep
their previous bounds. No player or donor is synthesized.

`renew_season_player_slots` shares the existing physical-slot ageing and
retirement loop. Its optional global-identity visitor allows successive bounded
workspaces to age each real player once, including cached offer/free-pool
references. The caller must provide actual release placement and retain a failed
placement as a failed unpublished transaction. The legacy whole-world pass uses
its original allocator and once-per-player tracking.

The kernels retain the Original native stream and random order in independent
pre-extraction comparisons. Their availability alone does not publish a typed
new season or close the accounting/archive release gates.

## Unpublished complete annual accounts

`stage_annual_accounts` requires the exact paid-awards qualification receipt and
known Source account, tax, monthly-youth, depreciation and attendance backing.
It starts both generators at that receipt's returned seeds and executes all
managers in canonical order. Youth promotion, summer events, actual player
bonuses, the real academy donor search, credit, taxes/refunds, title premiums
and season tickets remain one Source pass. The fine local continues across
managers. New sanctions remain pending until promotion has finished.

Tax notices, animation and outcome acknowledgements require an owned presenter;
the real question requires an explicit answer. Missing presenters or a cancelled
choice reject the unpublished stage. The packet retains its canonical manager
and club identity, actual event/choice transcript, academy donor/player/recipient,
pending sanctions, fine carry and both draw counts. Revalidation replays those
recorded decisions against the exact binding without prompting again. Changed
relations, backing, events or result fields cannot reuse a receipt.

An academy recruit retains a real donor's global player ID and moves through the
same canonical transfer/statistics adapter as ordinary player movement. It is
never added as a new player. Authored tests cover Germany, England and mixed
scope, missing/cancelled choices, old unknown tax state, packet tampering,
snapshot roundtrips and academy movement through the subsequent promotion
roster inheritance. These authored closing programmes are not played seasons.
Player renewal, cup/calendar reset, full title archives, closing/new desks and
final atomic season publication remain required.

## Annual mood phase and retained country order

`stage_annual_moods` runs before European qualification and accounts. It obtains
completed titles/ranks without drawing qualification RNG, consumes Source's
initial unused W draw, then updates every actual club/national identity through
the shared per-club Source mood kernel. Domestic amateur clubs use their saved
physical pool order, including preceding years' exchanges. Known national
nominations supply the primary national roster; abstract clubs retain their real
rating. No player is synthesized or added to a non-league club.

The unpublished packet binds its source, physical order, changed World and both
random streams. Qualification continues from the returned seeds. Old unknown
amateur order and a changed owner reject the stage. The annual manager-reset
kernel also accepts explicit completed-title/next-entry relations for its
advertising pass, while the native call retains its existing competition-memory
path. Original full-stream/RNG comparisons include moods, accounts and complete
native player reset; typed tests separately exercise mood-to-qualification-to-
account continuation. Final season publication remains a separate requirement.
