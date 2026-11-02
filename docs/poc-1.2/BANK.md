# Shared-career bank

The Original bank room, cashier, stock exchange, property/fund view and credit
editor now operate on the typed career. The existing indexed graphics, hitboxes,
calculator and dialog flow are reused. An explicit numeric reference and crest
resource keep the dual-country package out of legacy executable admission.

Both the legacy session and the typed session call `apply_bank_request_source`.
It is extracted from the existing checked bank request handler and calls the
same Core deposit, share/fund trade, stock-tip and credit kernels. Modal ownership
and rollback remain with each session. A typed command stages cash, investment
balance, deposit, statistics, portfolios, loan parties and random seeds together;
stale or refused commands do not publish. A bank fee requires confirmation;
credit between managers retains the Original fee-free mirrored booking.

Portfolio installation and capture map both holdings and paid totals between
runtime manager slots and canonical seats. The full four-seat permutation also
preserves currently unused seats, market prices and histories. Installing only
the holding counts would give sparse managers another manager's cost basis.

World schema **26** adds the optional depreciation account at the verified
Source record field. Fund/property purchases add their original fees, annual
allowance and renovation costs; sales adjust the same account. New careers have
known native balances. Older saves retain unknown balances, ordinary projections
preserve that uncertainty, and a newer balance cannot be silently downgraded.
Bank commands on an older career lacking this account are explicitly unavailable;
the current implementation does not invent a historic zero. Snapshot framing and
the other financial owners remain unchanged. Tip and notice presentation state
is transient, rather than additional saved financial data.

Validation compares shared requests directly with Core operations, including
cash transfers, share/fund paid totals and holdings, tips/fees and RNG. Sparse
seat tests retain nonzero values and unused actors through projections and the
codec; depreciation includes high-word, old-schema and unknown-state cases.
Private Original GUI-input tests cover single English and mixed German/English
careers: cashier, shares, tips, property purchase/sale, renovation notice,
correct crests, stale rejection, save round trips and the Original two-step
right-click return from a subview. Separate typed credit probes compare actual
bank and human-lender terms/cash against the Core and retain saved loan identity.

This makes the bank available in the development GUI. It does not finish the
general live/calendar presenter integration or the remaining management menus.
Native financial words are still 32-bit; the separate large-balance overflow
issue is not solved by retaining depreciation or remapping portfolios.
