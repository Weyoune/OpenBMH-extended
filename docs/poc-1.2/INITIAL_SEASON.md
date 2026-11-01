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


## Initial manager services (schema 14)

`initialize_first_season_manager_services` now follows the draw boundary. It sells every manager's season tickets before resetting any manager, matching the Original order. A bounded relation projection supplies the actual European participation, domestic championship/cup holder and European holders to the existing popularity/advertising calculations. Both country ladders use their explicit local league class and round context. The combined career's English cup-winner place now uses its domestic holder consistently; the German-only fallback retains the Original random foreign-cup selection.

The existing ticket sales, StartingSponsors, seasonal manager reset, advertising-offer generation and all 24 tactical familiarity draws run through shared Core routines. The initial date-zero path does not age or retire players. The owner resets initial fairplay/results, persists the final W seed and returns ticket receipts. G is unchanged. Fixture shuffling, the initial market and daily/desktop progression remain later steps.

Schema 14 retains ticket counts/income, all thirty twelve-byte scalar advertising offers, seventeen regular advertising contracts, and the strength/position history/count. The contract area deliberately stops before the designated-player extension. Amount/month bytes of the three separately named sponsors are canonical zero in the backing rows and restored from their explicit fields. Domestic champion/cup-holder identities are global IDs, so later membership shuffling cannot change their meaning. Older schemas leave these new fields absent and do not claim completed services.

Initial services, markers, finance/tactics/match/career fields, notices, seeds and one revision publish together. Replay and changed owners leave the accepted state intact. Synthetic coverage includes all six club scopes plus twelve combinations of mode 1/2/3, duration 1/5 and difficulty 0/9 in the lowest English league. The actual-data validator covers the six source-data scopes and directly compares the German ticket/reset boundary with Native Core. Sixteen independent pre-refactor ticket/popularity/advertising/reset comparisons match the complete retained seasonal state, finance/tactics and random stream. The reference 17/91 reset boundary consumes W=108 after competition preparation.

Validation of the schema-14 manager-service milestone: all 27 targeted Linux regressions and the graphical application build pass. The six actual-data scope probes include the new service boundary and snapshot roundtrip. Two Original saves remain byte-identical, and the initial-offer predecessor check remains G364823/3131 W840250195/12390. No native Windows/macOS or playable-release claim is implied.


## First-step shuffle and market (schema 15)

The [initial market owner](INITIAL_MARKET.md) now follows ticket/reset services. It preserves global identities while shuffling fixture positions and prepares explicit transfer listings through shared Original selection/pricing rules. The negotiation checkpoint and W state persist atomically. First-day and visible desktop/session ownership remain pending.
