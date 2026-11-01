# Monthly loan returns

`return_monthly_loans` runs the shared monthly return kernel in canonical club
order and physical roster order. It stages players, rosters, owner tactics-board
snapshots, accounts, transfer history, messages and both random streams together.
A return closes the borrower roster gap; the next physical slot is then examined,
including the Original behavior that can leave the shifted second loan until the
next monthly pass.

Human borrowers require an explicit answer. Choosing another offer requires an
owned editor callback on the actual special entry. The callback can use the
shared transfer editor/settlement; cancelling or missing a required presenter
rejects the transaction. The caller owns the editor's saved opening random seed,
allocator observation, notices and global-manager presentation. Unowned occupied
special offers are rejected, never overwritten.

Loan ledger and human-owner board slots are now retained independently of vector
position. Returns leave holes; subsequent loans reuse the first physical hole.
World schema19 saves the optional ledger slot. Outer snapshot SA retains the
owner-board slot and reads S4–S9. Earlier dense contracts acquire their existing
slot order on the first transfer transaction. Duplicate/out-of-range slots fail
validation. Global club/player IDs never enter the legacy packed fields.

A shared offer-rating precondition now accepts qualified projected league
predictions instead of attempting to read an absent legacy league table. The
ordinary legacy guard and its zero-recent-games check remain unchanged.

Synthetic tests cover AI/human owners and borrowers, countdown/expiry, explicit
questions, actual editor submission, cancellation rollback, shifted-roster
traversal, sparse slot reuse and saved snapshots. The actual-data comparison
adds36 full-Core countdown/expiry comparisons over the18 created loans, checking
all rosters, full human state, mail, random seeds and saved snapshots. These are
controlled service fixtures, not played careers or native platform validation.

This service does not advance the calendar. Monthly advertising, billing,
contracts and shares/credits, general event ownership and GUI progression must
still be integrated before a playable1.2 release.
