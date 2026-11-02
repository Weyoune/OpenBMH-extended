# German indoor tournaments and Masters

The shared career keeps the Original German indoor programme independent of the
English League Cup. Both use legacy competition number 11 and a `0x4002`
calendar cell in their respective source, so the typed owner retains the German
calendar domain explicitly. Q5 admission remains unchanged: zero admits German
Hall dates; a nonzero saved byte suppresses them. English cup dates remain owned.

## Source structure and selection

The supplied Diamond-compatible rules describe these stages:

| Entries | Format | Passes | Matches |
| --- | --- | --- | --- |
| 25–26 | Two groups of four | Three group rounds | 12 |
| 27 | Two semifinals | One round | 2 |
| 28–29 | Final and third-place match | One round | 2 |

Five invitation tournaments are followed by the Masters: 96 matches over thirty
passes. The twelve actual German calendar cells occur in consecutive pairs.
The first date owns three group passes, the second owns semifinal and
classification passes. Daily training, interest and the date advance once per
calendar day, not once per Hall round.

The first draw occurs at the first scheduled event after Bundesliga round 17,
even when the first Hall date is later. Source `indoor_tournament_draw`,
`indoor_tournament_reset`, `masters_qualification` and `advance_cup` own the
selection, shuffles, host avoidance, invitation limits, strength class, points
and title/prize effects. The common engine is reused for compatible data; there
is no separately tuned indoor or edition-specific match engine.

The selection namespace retains the Original physical order: all ninety German
league slots, then Spain, the English Premier League, France and Italy. English
lower divisions do not become additional guest slots merely because they are
managed in the same world. Real previous holders outside that regular guest
namespace retain explicit additional references; they do not become random
guests. All canonical participants, hosts and holders use global club IDs.

The first forty physical German positions retain Source invitation/point
ownership, including the four positions beyond the first two eighteen-club
leagues. Masters qualification retains its direct-list/invitation alias quirk;
the typed owner resolves those physical values explicitly instead of allowing
new English IDs to alias them accidentally.

## Common Core scoring and ownership

Hall scoring uses the shared Core kickoff, match engine, phase driver and
post-match processing. It sets the genuine indoor mode, reads the indoor block
of the supplied M3.DAT formations, retains the keeper plus five paired field
positions and rotation players, and runs two twenty-minute halves. It does not
invent smaller rosters or substitute outdoor formations. Source kickoff owns
its normal short-squad forfeit path. Attendance is the Original fixed 10,000;
ordinary stadium gate-income calculations are not rerun for Hall matches.

Each parallel group retains its Source descriptor on the shared minute clock.
Trusted `before_matchday`, live-event and minute hooks receive the actual indoor
projection. Missing human presentation is not silently replaced by headless
policy; isolated probes opt into that policy explicitly.

World schema 37 owns Source descriptors, previous holders, selection slots,
Masters scratch identities, invitation/point arrays, hosts, all six tournament
histories and the pending date-pass owner. Older worlds retain unknown indoor
state. A Source draw cannot reset missing older history to a fabricated season.
Prepared indoor groups have an independent identity and snapshot framing `SI`.
Weather, referee pool, attendance, tournament/pass revision and parent calendar
receipt remain bound through save/load.

OpenBMH deviates from the original: when the merged English calendar schedules
an outdoor competition on a German Hall date, that ordinary programme commits
first, followed by the three Source Hall passes. An invited English club can
therefore play its real outdoor fixture and its later Hall fixture with separate
lineup/card contexts. No fixture is removed or redrawn to avoid the collision.
The pending date owner admits exactly those current-date ordinary results in
saved calendar validation and prevents replay on subsequent Hall passes.

## Validation scope

Public fixtures use authored data, direct Original-workspace field/result/point
comparisons, exact RNG checks, full scoring and same-date save receipts. Actual
prepared-data probes calculate genuine seventeen-round winter standings before
each 96-match programme, including mixed two/four-manager cases; all pass
checkpoints remain private. These isolated probes do not advance unrelated
calendar programmes. Component scoring/serialization checks do
not establish native live acceptance, a fully played career year or the Beta
release gate. National programmes and annual renewal remain separate owners.
