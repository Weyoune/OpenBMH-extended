# Real post-round manager history

Every committed ordinary league round now publishes the real manager's annual
league-position byte and the known38-entry strength/rank series before handing
off its date. The day transaction first writes the complete canonical result
batch, calculates actual Source standings with point deductions, then traverses
each completed league's physical clubs in ticket order. It calls the existing
Core `store_season_position` and `store_strength_history` writers in their Original
order for each participating human manager.

A bounded one-club workspace contains only the actual24 roster slots, real player
attributes, team morale, canonical post-round rank and the manager's saved history.
There is no random calculation, lineup automation, simulated score or calendar
advance in this adapter. It copies back only the known150-byte season history
and the known strength/rank arrays and count. Other manager data, old cup/European
history and unrelated managers remain owned by their existing transactions.
Missing older history stays unknown. Source's38-entry shift and current slot49
for long careers remain unchanged; annual window rollover has a separate owner.

German parallel Regionalligen share the18-position history band. English visible
tiers use actual20-position bands0/20/40/60. The inherited raw FLTD caller still
contains the German literal18 adjustment after its fourth physical tier starts;
that aliases English divisions. The typed English encoding deliberately corrects
this narrow case while retaining Legacy behavior. An existing internal10-club
manager, where present in an older development snapshot, records its actual rank
in the80 band; this does not make that internal league selectable or invent a
manager or a played history row.

Synthetic Native comparisons exercise German and English post-round processing,
full manager history,38-entry rollover, season50, the explicit English encoding
correction, paired regional leagues, sparse canonical seats and actual internal
league owners. Day publication, replay/revision rejection, unknown preservation
and complete save/reload are checked. No gameplay RNG stream changes.
