# Original mailbox integration

The typed desktop's Original newspaper shortcut (kind63) opens the existing
`MailboxScreen`. It uses the verified reference image supplied by the typed
session, the existing atlas/font and the active manager's projected messages.
No new renderer or guessed message categories are introduced.

The session owns the open reader. Actions bind the active local actor/revision
and all160 expected physical entry bytes. The shared `edit_mailbox_entry` kernel
implements keep/delete for both Classic and typed careers. Typed edits run with
canonical global masks, preserving the Original's manager-specific
`(recipients+1)&actor_bit` quota exception even when a projection reorders seats.
Kept mail cannot be deleted; stale or wrong-recipient actions cannot modify it.
Deleting a shared message removes only the current manager's recipient bit.

Edits publish a validated candidate and increment the session revision. Closing
only clears that manager's notification bit, with no game revision, date or RNG
advance. The Original screen retains its page, action mode and rejection feedback.
Parent refreshes must match their existing source and actor. The current GUI
integration opens the reader manually; automatic notification display belongs to
the saved general event/desk progression work.

## Physical slots and schema21

Original deletion zeroes recipients without compacting the mailbox or erasing
its old raw date/text. Earlier capture stopped at the first zero recipient and
would therefore lose later messages. Capture now retains every physical hole
through the last nonzero raw entry. Only a wholly zero trailing area is omitted.
New mail still fills the first Original free slot. This preserves physical entry
identity, equal-date ordering, shared recipients and expected-entry guards.

World schema21 permits these zero-recipient records. Earlier schemas remain
readable for their dense mailboxes and reject sparse rows they could not represent.
The outer SA snapshot framing is unchanged. The historical `read_by` field name
continues to store the Original kept-by mask; it is not a separate read receipt.

## Validation

Synthetic kernel checks cover stale bytes, wrong actors, shared/broadcast deletion,
physical holes, kept deletion and both Source quota branches. Economic snapshot
checks cover later-message retention, first-hole refill, sparse save/load and old
schema rejection. The actual-data GUI suite loads a mixed-manager fixture, opens
the real shortcut, keeps/rejects deletion/unkeeps/deletes mail, preserves the other
manager's copy, closes to the same parent and roundtrips the resulting save.
Before/after/rejection captures and that save remain in the private QA repository.
