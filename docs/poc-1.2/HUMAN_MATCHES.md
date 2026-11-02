# Human league match calculation and day transaction

World schema 9 stores optional human match policy/statistics: Source settings, tax/win bonuses, crowd pressure, goal-notice/deduction flags, staff ratings/history, streak/timing counters, recent fixtures/results, head-to-head results and designated takers. Historical club/player references are global IDs. Earlier worlds retain a missing state instead of being treated as initialized humans.

The legacy career and typed initial-manager phase share the Source team-settings initializer. The match-state bridge reads/writes only verified fields, resolves references before writing, respects equal-ID history terminators and preserves head-to-head history outside a bounded workspace. Other manager/record bytes remain untouched.

## Bounded human projection

`AiMatchProjection::create` still rejects human participants. The explicit `create_for_humans` factory requires complete account baseline, tactical backing and match state. It maps sparse global manager seats, players and historical references to a bounded local workspace; real human Source rating/development rules remain active.

League size, prior rank, round cursor and cached forecasts are supplied per team. Thus German/English clubs sharing a logical ladder class cannot read one another's positional tables. Missing forecast modes are errors, not a fallback to a German table. Normal legacy calls retain their existing path. English promotion notices use the actual league name; Source German notices retain their existing formatting.

Formation-tail inputs stay independent for each manager. AI formations retain the supplied default block, even when manager-zero owns a different custom table. Temporary player/name references and host-only callbacks are not serialized as legacy careers.

Complete commit checks original managers, players, clubs and development state, plus local manager counts and identity fields. Accounts, full tactics, player/card changes, history/statistics and global references publish together. A late invalid reference or externally changed manager rejects publication without rewinding external changes.

## Round and day ownership

`simulate_and_settle_human_league_round` plays the actual Core minute phases and settles human player development, payments and manager records. Synchronous event/minute presenters can bind the future GUI; presenter storage must obey the existing MatchEngine lifetime rules. Unhandled human events fail explicitly unless an isolated probe deliberately opts into headless decline behavior. Human teams are never converted to AI teams to complete a run.

The round API calculates on the day owner's staged snapshot: it retains the date/results cursor and publishes only its match state/RNG into that candidate. The caller must complete the day transaction before exposing it as a completed career day.

`play_league_day` extends the existing atomic AI-day owner with human policy/presenters. Complete scheduled human and AI league groups, results, seeds, owner revision and one date transition publish together. Replays, stale/external changes and failure in a later group leave the accepted career unchanged. The AI-only API retains its explicit human rejection.

This service remains league-only. Cup/national/indoor appointments, daily/weekly economy, earlier creation stages and complete desktop/live GUI binding remain completion work. Passing a calculation test is not a playable-release claim.

## Verification

The current Linux milestone passes 25 targeted checks, 56 actual synthetic human/AI fixtures (115 goals, six human event presentations), the isolated nine-fixture real-data round (17 goals), mixed-day replay/snapshot/rollback and two/four-manager state checks. The 15,120 unchanged-Core forecast comparisons remain equal. Fairplay points, mapped and unmapped histories, designated global player references, identity tampering and failed reference publication are covered. These are calculation/transaction checks, not a human GUI playthrough.

## Live editor integration boundary

The numeric `LiveScope` authenticates one watched human fixture and its Source
minute, with separate manual and actual red-card/injury entry. It shares the
Classic row/substitution and pitch/tactic actions; stale rows, retired numeric
capabilities and formation automation are rejected. An unfinished scope restores
its projected state, retained group storage and both random streams. A completed
manual editor rerates once; the actual event retains its existing EventImpl tail.

Human projections now install and capture each canonical manager's desktop
display word. Local numeric manager order never becomes the saved manager seat.
The native UI adapters share the existing Original scene and lineup presenters
and accept explicit reference rules, rather than rediscovering a legacy profile.
These pieces do not by themselves complete calendar/live overview ownership or
qualify a GUI season for the Beta gate.
