# Scheduled manager desks

The Session can now open a saved, prepared league or domestic-cup appointment
as a manager desktop loop in all three game modes. Modes II/III prepare later-day
unsolicited offers using current standings, loan/squad counts, retained bidder
history and the shared Core recruitment/price kernels. A pending offer must be
resolved before advancing to another manager. The frontend's final initial-day
guard is still in place pending its complete live/calendar presenters.

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
and random streams. **SG** additionally retains the ordered Toto match selection;
older SF snapshots retain an explicitly unknown selection. Earlier S4–SE snapshots
remain readable. Invalid combinations,
replayed actions, stale owners and interrupted preparation cannot publish a
partial cursor. Attendance cannot skip an active desk; calculation consumes the
saved attendance receipt only after the managers have finished.

The existing desktop renderer receives the active manager's actual global
opponent, explicit crest atlas/cell, home/away side and competition label.
English division and cup labels come from the typed appointment. A manager
without a fixture does not receive an invented opponent. Temporary drawing
indices never replace persistent club identities.

The Core BuildPoolsCoupon kernel selects up to eleven eligible unplayed fixtures
in programme order, rebuilt after each manager's offer preparation. Bounded
projections retain global identities without overflowing the native player table.
Its first complete group is independently compared with a native group scan.
Selection/open state is not a placed wager: the Toto input, stake records and
financial settlement owner remain separate work and cannot be silently skipped
once wagers are supported.

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

Additional private mode-III tests deliberately set the scout rating to force
each Source offer chance, reload every manager cursor, resolve real offers with
explicit QA presenters and retain the existing outgoing loan through final
scoring. This is a labelled test injection, not a changed game default.

`Session::advance_to_next_desktop` stages the complete last-desk transition:
close the current cursor, calculate its prepared matches when applicable, run
saved services, process uncoached nomination appointments and prepare the next
owned desktop. The saved service viewer is the last manager of the current
shuffled order, not a manager retained from the preceding event. The composite
publishes once and compares exactly with explicit phase-by-phase continuation.

StartupFlow accepts this path only with explicit synchronous turn presenters.
A separate dated handoff validates increasing date and stable roster/season;
ordinary same-day manager handoffs keep their strict unchanged-date guard.
Private GUI-input tests click the Original kickoff button, verify the exact
next final-day desktop and prove rollback plus released button ownership when
a weekly presenter cancels. The normal SDL entry still lacks the full set of
live/calendar presenters and retains its last-desk guard. This is a tested
frontend/session seam, not a claim that the packaged game can play a whole season.

Still required: Toto stake ownership and settlement, live/result presentation,
shared/national/indoor appointments and
the general GUI clock handoff. These tests are not three played seasons or a
finished Beta.
