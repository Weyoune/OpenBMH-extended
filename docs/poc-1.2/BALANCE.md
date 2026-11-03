# Typed current balance (menu 28)

Extended menu 28 opens the existing Original `BalanceScreen` through the canonical
readonly Session owner. It uses the supplied data-only LE rule reference. It never
loads an executable from the typed data directory.

The Source `so_far` kernel reads the real eighteen booked income/expense counters,
salary/bonus totals, ticket income, interest, depreciation, current account, actual
ticket price and recorded attendance. Scheduled league home games use canonical
membership size minus one (19 in the English 20-club leagues), rather than a German
local league index. The complete career snapshot and both RNG streams remain
unchanged on open, redraw, ESC, right-click, Source event 8 and close. Existing
scope/revision/generation/sequence and canonical actor/roster guards protect the
window; no call to `close_balance_plan` occurs.

**Beta limit:** this is the current booked balance only. Forecast/plan editing,
reset/accept controls and submission are unavailable. The two unknown plan input
rows show `--`, and the screen explicitly says `Beta: nur Ist-Bilanz / kein Plan`.
World currently has no persistent owner for the eighteen plan adjustments,
unsubmitted plan, planned other home games, planned average gate or process-local
planned ticket price. No estimate is substituted for these fields. Missing known
monthly bookings, depreciation or attendance in older development saves reject
explicitly; national teams have no club financial owner for this view.

This focused view does not complete the management, annual planning or Beta gates.
Rescheduling (7) and joining an ongoing career (29) still require their own canonical
backend/save transactions.

Public regression: `world12_readonly_tests` checks all eighteen ledger mappings,
DE/EN home counts, current price/gates/account, unavailable plan inputs and missing
backing. Private data verification is available through
`world12_session_tests DATA OUTPUT --balance-only SNAPSHOT.FBM`; it exercises the
real visible mode-III desktop, shared Original frame, Core rows, unsupported edits,
owner/replay/reopen guards, Source timeout and full save/resume equality.
