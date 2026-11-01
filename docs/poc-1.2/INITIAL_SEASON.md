# First-season competition preparation

`app::world12::prepare_initial_competitions` advances a fully confirmed club career through the initial competition draws. It is a staged boundary before season tickets, seasonal player/reset services, fixture-position shuffling, transfer-list creation and first-day advancement. It does not declare the desktop or complete career ready.

## Source order

The owner first recalculates each league club's formation balance and automatic lineup, using the existing Core functions through bounded player/manager workspaces. Membership IDs and settings are copied before those workspace commits replace the candidate world; iteration never borrows an invalidated container.

It then draws the season-zero four-nation tournament, assigns the European champions/cup-winner/holder/league places, fills the three European fields and performs the initial domestic draws. Native Classic and the typed path share the national draw, league-entrant selection and field-fill control. The European fields retain the Source order: UEFA Cup (64), Champions League (24, with the sixteen qualifying positions shuffled after the eight-entry prefix), Cup Winners' Cup (32).

The Source UEFA fill loop draws a discarded scratch tail beyond its published 64 clubs. These draws still occur, including duplicate rejection. Typed scratch identities have a separate namespace, so they cannot accidentally alias new English club IDs or become persisted participants. The snapshot retains only real, distinct club identities. Switching off national play removes its stored/presentable field but preserves the Original unused initialization entropy.

The German-only/Premier-League-fallback path matches the Native Source boundary. Combined play uses both domestic ladders followed by the remaining European leagues. In English-only play, German clubs are absent: Germany's four regular UEFA places move one each to the four remaining top leagues; its cup-winner place goes to the first still-unallocated primary-league club. Remaining fields use the existing reserve-country clubs. No Portuguese league is introduced.

## State and publication

World schema 13 retains qualified entrants, filled European lists, initial holders and the enabled initial national field, all with global club IDs. Manager career state additionally retains the numeric fifty-by-three-byte season record area. Older worlds leave this optional history absent; scoring cannot silently manufacture missing career history.

The owner records the initial European/domestic campaign participation, installs the supplied validated calendar and stores both final RNG seeds. Domestic draw notices keep the existing human-country presentation filter. The world, cup stages, history, calendar, seeds and one session revision publish together. Replay, incomplete managers/reserve pools, unknown qualifier layout, invalid schedules and late owner changes reject the entire candidate.

## Evidence and remaining work

All six club-country/national-option combinations pass with synthetic and actual data, including English-only omission of German clubs, 120 unique European clubs, snapshot/history/calendar persistence, replay and late-failure rollback. The private Source comparison stops Native Classic at the domestic-draw overview before ticket sales. Sixteen independently compiled pre-refactor comparisons from `a982467` match national/European/domestic fields, qualification and campaign records, lineup card rows, RNG seeds and draw counts. The reference seed pair 17/91 consumes G=180 and W=470 for this boundary. All 27 targeted Linux regressions and the application build pass; the initial-offer check and two Original-save byte-identical roundtrips remain intact.

The initial draw payload is ready for presentation binding. Season tickets, remaining seasonal records/advertising/player reset, first-step shuffling/transfer list, national-only ownership and the visible typed desktop/session are still required. Calendar attachment here does not certify daily economy, complete competition simulation, multi-season play or a release.
