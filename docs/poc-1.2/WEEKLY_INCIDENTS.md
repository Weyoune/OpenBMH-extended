# Owned weekly offers, facilities and advertising

`begin_weekly_managers` implements the first part of the shared weekly sequence:
manager offers, facility ageing, facility damage and every-third-week advertising.
It does not replace the following market, player, settlement, training and monthly
phases, or advance the calendar.

The typed caller must provide current competition relationships for every human
club, qualified by global club ID and the current date/season. Advertising uses
those explicit European participation and title-holder facts rather than reading
unrelated native club numbers from a temporary workspace. Historical starting
fields are not automatically treated as current competition ownership.

Core `manager_offers` now accepts optional explicit decision hooks. The ordinary
legacy caller retains `offer_answer`; a typed caller installs owned hooks and
cannot silently decline when a presenter is missing, cancels or cannot answer.
The callback receives Source kind, value/amount, original message and a global
manager seat. The entire phase, messages and both random streams are staged;
late changes or unavailable decisions discard its effects. A question's Source
text is passed through unchanged, including the existing empty payload for the
Core's no-effect kind15. Presentation of these events belongs to the forthcoming
daily GUI owner, not this calculation phase.

Facility completion retains construction countdowns, planned block capacities,
current/future stand flags, compensation and the market quote RNG checkpoint.
Manager round checks and ticket refunds use each club's actual projected league
and round. Financial advertising influence retains the submitted balance-plan
word. The typed runtime reference explicitly loads the supplied bank rate table
and divisor, so an automatic credit-interest change has verified rule inputs.

World schema17 adds planned stand capacities, stand flags and the submitted plan.
Older schema1–16 inputs remain readable, except an unfinished stand construction
in schema16 is rejected because that schema could not preserve its target.
The S9 outer save framing is unchanged.

A related reference bug was fixed: sponsor-contract backing overlapped the two
extended designated-taker words. With the taker marker active, these four bytes
are excluded from the scalar backing and preserved through their explicit global
player references. The installer cannot overwrite freshly remapped takers when
reinstalling sponsor data. Old typed schemas normalize the duplicated backing
using the already-retained canonical taker references; unmarked original bytes
remain unchanged.

Validation covers all eight Core question kinds with yes/no/missing/cancelled
answers, sparse manager seats, late callback rollback, completed and saved
unfinished stand targets, schema compatibility and sponsor/taker isolation across
reordered numeric player maps. Twenty-eight actual-data comparisons match the
full Core, including an exercised credit-rate change, third/twelfth-week ageing,
explicit decisions, ticket refunds, advertising, full stadium/sponsor/account
state, loan contracts, mail and both RNG streams. Twenty-four predecessor/current
complete economic-week outputs and RNG traces remain byteidentical.

The complete daily owner, global market countdown/bids/settlement, monthly
services, general saved events and weekly decision GUI remain open. This is not
a playable 1.2 release, nor a new recording of original DOS execution.

`current_weekly_club_relations` supplies the GUI clock's attraction context from
the saved campaign owners. Domestic finals replace the previous domestic holder;
completed European campaigns replace their previous holders. Current active
participation is queried separately, so a previous cup winner may now play in
another competition. Unknown initial title/campaign ownership remains an explicit
error. The lookup changes neither state nor randomness.
