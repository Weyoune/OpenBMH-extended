# Owned initial player offers

The development GUI resolves an initial unsolicited AI offer through the existing
`process_natural_pending_offers_after_desk` transaction. The Original salary,
negotiation, severance, acknowledgement and transfer screens remain in use. Each
screen receives the explicit rendering reference and current global club crest;
a temporary workspace index is never treated as a permanent crest identity.

`resolve_initial_offer` stages the entire career and both random streams. It
projects all human clubs, the actual bidder and existing loan participants into a
bounded numeric workspace. The saved offer owner, manager permutation and cursor
are restored before calling the shared routine. Missing/unsupported answers,
throwing presenters and stale owners discard the staged decision. A completed
salary decision or decline removes only the resolved offer. A successful movement
may change precisely the qualified player's seller/buyer rosters; the ordinary
match projection continues to reject roster membership changes.

Settlement retains the Source financial outcome, including payments followed by
a full-squad refusal. Transfer balances are signed 32-bit values. History rows
retain global player/other-club IDs and the Source scalar event word. An outgoing
human-owned loan also retains its global owner/borrower, remaining months and the
owner's complete contract board. Its two player-reference words are stored as
separate global IDs; packed workspace references are zero in the saved backing.
Original capacities remain thirty history rows, twelve owner boards and three
outgoing owner loans per manager.

Development snapshots now write **S9** and continue reading S4–S8. The world JSON
schema remains unchanged. Snapshot tests cover signed balances, IDs beyond the
packed/signed native ranges, loan marks, duplicate/orphaned contracts, truncation
and previous-frame compatibility. These are development snapshots, not original
BMH/FLTD saves.

A resolved decision can hand off to the next saved manager. For the final manager
it returns to the current desktop with the decision committed: complete daily and
weekly processing still needs its own owner. It must not erase an accepted sale
merely because that later boundary remains unimplemented. No calendar advance,
loan-return lifecycle or full playable-career certification is implied.

The Linux SDL presenters consume the physical release belonging to a DOWN-based
pending decision before returning to the desktop. This prevents a dismissing click
from opening the icon underneath a notice. Questions already completed on UP
retain their existing input behavior. Original legacy presentation is unchanged.

Validation at this milestone includes synthetic ownership/rollback and snapshot
checks, an independent full-Core matrix of 72 decisions with 30 actual sales and
two actual loans, and eight German/English controller cases with the real data-only
package. A four-manager German/English Mode-III case accepts an outgoing loan
and retains its qualified owner contract through every subsequent desk. Actual
SDL mouse interaction covers German decline and English sale, manual save and
fresh-process load; each saved career exactly matches the controller snapshot
plus its four save-header draws. Native Windows/macOS and season progression
remain release gates.
