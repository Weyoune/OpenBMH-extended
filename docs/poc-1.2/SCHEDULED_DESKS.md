# Scheduled manager desks

The Session can now open a saved, prepared league or domestic-cup appointment
as a manager desktop loop. This first integration supports game mode I. Modes
II/III explicitly reject entry until their later-day unsolicited-offer and
coupon owners are implemented; the initial empty event remains supported in
all three modes. The frontend's final initial-day guard is still in place.

`begin_scheduled_desks` stages the Core competition-card/squad preparation,
automatic lineup, flagged-club lineup refresh and manager-option cycle. It
draws the matchday number and shuffles the preceding persistent manager order,
then enters the first manager's existing DeskDate. Original weather, calendar,
scores and date are retained. Each following manager enters the same owned
cursor; a pending decision or autosave blocks advancing. Completion only
releases the final desk into the prepared match owner; it does not advance time,
calculate scores, book gates or draw another random number.

PreparedEvent distinguishes unstarted, active desks and completed desks.
Snapshot **SF** stores that phase and whether attendance has been booked,
including the exact active manager/cursor, layouts, autosave state, conditions
and random streams. Earlier S4–SE snapshots remain readable. Invalid combinations,
replayed actions, stale owners and interrupted preparation cannot publish a
partial cursor. Attendance cannot skip an active desk; calculation consumes the
saved attendance receipt only after the managers have finished.

The existing desktop renderer receives the active manager's actual global
opponent, explicit crest atlas/cell, home/away side and competition label.
English division and cup labels come from the typed appointment. A manager
without a fixture does not receive an invented opponent. Temporary drawing
indices never replace persistent club identities.

The calculation boundary now knows when the event owner has already prepared
competition cards and squad state. It must not repeat that operation after the
manager's desktop decisions. Existing lower-level calculation probes can still
request their own preparation explicitly.

Synthetic validation covers a complete mixed-country league-day desk cursor,
exact SF round trips, truncated/invalid phases, early completion, duplicate
actions and rollback while an external revision changes. Private tests open the
two source-calendar finals and the first twelve league rounds for four
German/English managers, reload after every desk and then calculate the saved
weather/crowd receipts. Desktop captures accompany their FBM snapshots.

Still required: modes II/III later-day offers, real Toto coupon ownership and
settlement, live/result presentation, shared/national/indoor appointments and
the general GUI clock handoff. These tests are not three played seasons or a
finished Beta.
