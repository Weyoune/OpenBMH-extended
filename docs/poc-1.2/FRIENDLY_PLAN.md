# Owned friendly planning

CareerSnapshot retains a separately owned FriendlyPlan. Missing older planning
history stays unknown; fresh Source initialization verifies the empty native
planner before creating a known empty plan. An actual annual calendar reset can
clear dated bookings while retaining known national booking progression.

Pending entries contain stable global participant IDs, the real season day,
canonical fee payer and charged amount. The Source limit is thirty pending
fixtures. Completed entries retain actual full-time score and end-minute
receipts; plan storage does not generate scores. Calendar0x4001 appointments
must correspond to real saved bookings, with unique participants per date.

Snapshot L adds explicit persistent-order and prepared-event presence, followed
by the owned plan. M retains these fields and its per-fixture Source kickoff
receipt after attendance. Readers that do not own these tags reject them rather
than silently discarding new planning or kickoff facts. Prepared kind6 is
reserved for friendly groups; national tournament kind5 remains distinct.

The actual Source friendly match descriptor is entry24/type10 (descriptor11),
flags6. Entry30 is the pending planner counter and is not a played competition.
The actual national booking bonus byte210 is a saved progression byte, never a
fabricated club league identity. Native quotes, fee consent, booking, dated
weather/gates and the shared minute clock remain separate authenticated owners.
