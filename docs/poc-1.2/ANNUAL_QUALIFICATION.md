# Annual European qualification boundary

`stage_annual_qualification` returns an unpublished packet bound to the exact
paid-awards closing snapshot. It requires every active league's actual results
and round cursor, the enabled countries' completed domestic programmes and all
three completed European campaigns. Final winners are checked against the
stored final scores through the shared Core tie rules. Starting holders,
preseason ranks and forecast scores are never substituted for missing finals.

The packet retains complete final rank orders for all active league slots,
country championship/main-cup winners and finalists, European winners and
runners-up, and next European qualified/drawn fields. `annual_final_rank` returns
the **zero-based Source position**; the relation helper combines next-season
participation with the completed year's title holders. A previous UEFA or CW
winner can therefore qualify for CL without owning two current campaigns.

League places use the shared `european_league_entrants` kernel. Actual DE/EN cup
finals replace the Source foreign-cup selection only for playable countries.
AI-only countries retain the Source physical-membership random winner/finalist
selection. The existing shared quotas and Premier fallback remain unchanged.
The Source fill order, sixteen-slot CL qualifier shuffle after its eight-slot
prefix, full field shuffles and discarded UEFA scratch tail are retained. Wide
canonical club IDs never alias the scratch tokens. Published fields contain
exactly 64/24/32 distinct actual clubs.

Both random streams are staged in the packet, with before/after seeds and draw
counts. Staging changes no snapshot, title history, membership, roster, date or
random owner. Validation rejects a changed source or tampered receipt. The outer
annual owner must consume the result once, along with its existing revision and
scope contract.

This API does not perform the preceding club-mood phase or national draw. The
annual coordinator must pass their real staged state/seeds in Source order.
There is no national field in this packet and no year-zero national fallback.
Accounts, promotions, domestic next fields, campaign installation, player/reset
services and calendar publication remain separate owners. No schema change or
completed annual/three-season/Beta gate is claimed by this boundary.

Synthetic tests complete authored score programmes through the actual cup/draw
and European transition kernels. They compare fields and both random streams
with independent Source control loops across all six club scopes, exercise
historical-holder/new-campaign separation, and reject unknown, stale, altered or
externally changed owners. These fixtures are not played seasons.


## Club-only correction for an invalid Source CW field

OpenBMH deviates from the original: when the domestic winner and finalist both
own Champions League places, fewer than five CW qualifiers can remain. The
Original filler then reads beyond its 27 real reserve-country pools into
national-team identities. Extended retains all ordinary allocations and repairs
only that deficit, before field fill, with the highest remaining actual club in
the affected country's completed top-league ranking. All three current campaigns
are excluded. If no such club remains, the first remaining real reserve country
in Source order supplies a club through the existing Watcom rejection pick.

Each replacement has an explicit correction receipt. No club, roster, final or
national participant is invented. Ordinary Source fields/order/G/W are identical;
only a formerly invalid path changes its Source choices and draw sequence. The
returned before/after seeds and counts include that correction. Normal and
champion/cup-finalist/title-holder collision paths are tested independently.
