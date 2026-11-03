# National nomination ownership

The scheduled shared nomination keeps club and national ownership separate.
A `NationalSquad` owns twenty selected global player references, a 24-slot roster,
its tactical backing and match records. Club rosters and contracts are unchanged.

Selection uses home clubs first and then expatriates in canonical league order.
The shared Original kernel retains 3 keepers, 5 defenders, 7 midfielders and
5 forwards in ascending strength order. Equal threshold scores retain the older
candidate. Availability preserves the Original OR between injury and suspension;
the foreign-club strength bonus remains. Missing positional quotas reject the
transaction instead of duplicating an unrelated player from slot zero.

Board initialization and automatic lineup use a bounded numeric workspace and
the same Core functions as Classic. Global club/player identities never enter
12-bit roster words. The ordinary single-manager coaching rule selects that
manager's own country; adding a manager removes this implicit appointment at the
next nomination. Known national contexts select the first eligible coach per
country from the actual saved manager order.

`prepare_national_appointment` authenticates revision, scope, calendar ticket and
saved event order. Every represented country commits with the random seed or the
entire request is rejected. The date remains on the appointment. A coached request
cannot use the uncoached close path. Uncoached completion runs empty matchday
setup, option cycling, Original order shuffling and event closure; no national
match or artificial score is created. Daily finance/training remains the next
saved phase.

World schema 24 adds `national_squads`; earlier schemas retain unknown/empty
national state and reject nonempty newer payloads. Snapshot `OBMH12SC` adds saved
event order and matchday number, including an explicit pending-progress flag.
Earlier snapshot schemas remain readable. A missing old order cannot be replaced
with a newly shuffled seat order. Existing initial-event snapshots can recover
that order from their actual event when it closes.

Current verification covers twelve direct Core comparisons, the 4,680-player
synthetic world, both countries, sparse seats, single/multiple managers, malformed
references, rollback, codec/snapshot migration and a real four-manager continuation
to its next appointment. Saved human national preparation and the complete scheduled-match transaction
are also exercised with sparse active coaches and regular saves. Native GUI and
whole-career release evidence remain separate checks.
