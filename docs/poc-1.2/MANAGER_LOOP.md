# Initial multi-manager desktop cursor

The typed first-event runtime now retains `manager_position` as well as the already shuffled `manager_order`. A normal advance/kickoff request from the current Original desktop stages the next manager's pre-match offer and DeskDate entry, validates a replacement desktop, then publishes the Session/UI together. It neither reshuffles the order nor advances the date/calendar/results. Every manager retains their own layout, prestige, options, crest and squad.

The controller requires the exact pending desktop request; the Session checks its active projected manager, revision and absence of a preparation modal. Stale/replayed commands, resource failures and denied transitions keep the prior snapshot/revision. Saving mid-loop retains the actual position; resume reopens that owner without replaying earlier desks. S8 adds the byte to event framing; S4–S7 remain readable, with pre-S8 initial events beginning at position zero. WORLD12 JSON stays version 15.

## Original entry order

After a desk with no owned pending player offer, Source performs its end-of-desk scan and then enters the next position: PreMatchOffer, open/empty initial Toto coupon, enter_desk. The typed transition reuses the existing initial-offer algorithm and shared Source DeskDate. A previous quote which found no bidder does not occupy Source OfferSlots, so the next manager may overwrite that quote. An actual pending bid must retain its decision owner in the snapshot.

This entry handles only the initial empty event. Current player offers need their sale/salary decision owner before leaving; they are not silently declined or erased. A pending autosave needs its file owner before another desk. After the final manager, the Source daily/weekly service owner is still incomplete, so that request is rejected with the live desk preserved. This is not a completed calendar/day advance or playable release.

## Verification scope

Six synthetic country-scope tests cover the next position, saved cursor, pending-offer/final-day barriers and stale rollback. An orphan special bid without its offer notice is rejected at snapshot validation.

Actual GUI-input tests create four alternating German/English managers in Mode I, drive three Original kickoff handoffs, inspect every active portrait/club/seat exactly once in saved shuffled order and roundtrip each S8 snapshot. Mode I guarantees no random pending offer so this test isolates manager-loop ownership. Mode III offer generation and pending barriers remain covered separately; successful pending-offer decisions are not claimed.

The independent full-Core comparison constructs four human seats on the actual data fixture and executes prepare_event, natural after-desk scanning and enter_event_desk in Mode I. Order, active position, every retained layout/prestige, both RNG seeds and mid-loop saves match the typed path. The actual SDL harness additionally reproduces the four alternating managers and resumes a nonzero-cursor snapshot in a fresh process. Native Windows/macOS and completed daily/career/release QA remain open.
