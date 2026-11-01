# Initial manager confirmation

The typed club-career owner now accepts the country/club chooser result through `app::world12::add_initial_manager`. Placement, strength, facilities, environment, sponsors, contracts, accounts, lineup, saved career settings, both random streams and one session revision publish together. This is the initial confirmation phase; first-season setup and the visible career/session owner remain separate completion gates.

## Placement and identities

Only enabled playable German/English countries can supply a manager. The selected start league must belong to that country. Seats are confirmed in order, with unique clubs and portraits; accepted earlier managers cannot be displaced. A later confirmation must use the same persisted mode, difficulty, duration, planned manager count and foreign-player rule.

The Source top-division target is `1 + manager` for duration 0/1, otherwise `9 + manager`. Owned and reserved places are skipped. Reservation reads the supplied country's `CompetitionCalendar` position word and its `0x4001` bits. Other start divisions use the Original random choice and forward reservation scan over thirteen places, bounded to the actual group size for the ten-team English group. A missing eligible place fails explicitly.

League-to-league moves reorder membership while retaining the clubs' players and stable identities. A selected extra club has no squad: as in the Source's league/extra exchange, it inherits the displaced league club's squad, and that displaced club becomes an extra club. No player ID changes. The displaced league club receives its Original random strength reset when it has a squad; the chosen club receives the requested lower/middle/upper strength from its destination league's supplied range.

## Numeric initialization and persistence

Native Classic and the typed owner share the strength calculation and facility initializer. Stadium capacity, stand condition, facility bytes/levels and one-year upgrades come from the supplied country rule image. The same Source phase initializes the three youth teams, six monthly environment budgets, board patience, training plan and the three applicable starting sponsor contracts. Existing staff ratings, contract/age generation, initial capital/credit and automatic lineup remain shared with the established manager initialization code.

World schema 12 retains this state and the career rules. Older schemas leave these optional fields absent. Source numeric records are translated into explicit stadium, environment and sponsor fields; names and club identities stay in the existing typed manager. The five poaching-history entries are explicit global player references, not ten unchecked bytes. They are mapped into bounded tactical/live workspaces and resolved back on commit, including references outside the participating squads.

Formation masks now retain the supplied initial Source values. The earlier partial helper forced both masks to 15; complete confirmation now preserves the reference values without imposing a replacement mask. Empty initial marking words continue to use the explicit no-player normalization already documented for the account/board bridge.

Human projections install and retain the career fields, apply persisted mode/difficulty/foreign rules, and reject mismatched or externally changed career settings. A stale confirmation revision or any later failure preserves the accepted world, settings, membership and both RNG streams.

## Evidence and limits

Ninety synthetic confirmations cover both country ladders, three durations and three strengths. Additional cases cover four mixed-country seats, inherited extra-club squads, reserved places, ten-team placement, global poaching references, projection persistence and interrupted/late-failure rollback.

The private creation validator compares eighteen complete manager confirmations with Native Core across selected first/lower/extra clubs, start divisions and durations. An independent audit compiles the pre-refactor manager body from `4c87662`: sixteen seed pairs give 288 matching confirmations, including player/roster/card state, facilities, youth/budgets, sponsors, finance/tactics and both RNGs. Empty unused marking references use the declared no-player normalization in that comparison.

The real combined 4,734-player dataset also initializes four German/English managers using both supplied country rule images, computes thirty-three human/AI fixtures in both countries' top and lowest leagues and roundtrips scores, round cursors, RNGs and the world in an isolated league-round snapshot. All 26 targeted Linux regressions and the graphical application build pass; two Original saves remain byte-identical and the initial-offer regression passes.

This does not certify visible GUI career ownership, the first-season setup/draw sequence, full career services/competitions, national-only ownership, complete seasons or native Windows/macOS releases. No playable-release claim follows from this initialization milestone.
