# National campaign backend

World schema39 adds optional `NationalCampaignHistory`. It records each owned
season's field, exact Source programme, round/leg scores, cursor, winner and any
known runner-up. The twenty latest World-Cup podiums follow the Core
`national_tournament_end` honours shift. This backend does not claim human coach,
GUI, calendar or played-season integration.

| season modulo4 | Source competition | Source entries | games | appointments |
| --- | --- | --- | --- | --- |
| 0 | 7 | 14 | 12 | 6 |
| 1 | 6 | 10–12 | 15 | 5 |
| 2 | 5 | 13 | 30 | 10 |
| 3 | 4 | 1–9 | 52 | 7 |

Rules are read and verified from the supplied executable. No copied game data
is distributed. Core `generate_fixtures`, `advance_cup`, group tables,
best-third qualification, knockout ties and linked final/third-place ordering
remain authoritative. The adapter only maps stable global national IDs into a
bounded Core workspace and publishes validated results atomically. It never
produces match scores. Module tests supply controlled scores and are not proof
that seasons have been played.

`prepare_national_campaign` runs the extracted shared Source field-selection
kernel. Season zero reuses a known `initial_competitions.national_field` without
consuming the draw twice. Odd years require their completed preceding fields
and winners; the World-Cup draw also needs the preceding runner-up. The same
kernel is used by the legacy European setup, including opaque inactive scratch
words that are never admitted as typed-world national identities. The national
calendar flag is `1 << (12 - season % 4)`.

Club worlds retain the existing German Source draw identity. National-only
England can explicitly use England as the primary Source nation, with the
bijective0/2 country mapping; it does not rename or duplicate global entities.
This is a campaign API, not yet a national-only manager-creation API. The Source
setup permits only one manager when club play is off; the later context owner
must expose that limit rather than inventing simultaneous national-only coaches.

The Source four-nation quota stores only its winner. An unknown runner-up is not
inferred. Six-nation, European and World-Cup programmes retain actual runner-up
fields. World-Cup places come from the actual final and third-place match.
Only the Source title paths issue honours; there is no invented title for the
four-/six-nation programmes.

Unknown schema38 campaign history remains absent. A caller may initialize the
first owned season only at its genuine new-season boundary; it must not replay
missing results from an older midseason snapshot. Future coach/origin context
keys are explicitly reserved as null and non-null unavailable owners reject,
so intermediate readers cannot silently discard later financial contexts.

Remaining work: real DE/EN nominee match projections, rating-only other nations,
separate Source national coach accounts/monthly context, scoped nomination
origins, current-order coach selection, Session national mode and actual native
presenters. No Beta/playable flag follows from these module checks.

`national_final_facts` is the annual archive's score-authority boundary. It
checks stored winner/runner labels against Core group tables or the actual last
knockout score, and checks both World-Cup finals. It consumes no RNG and requires
the currently installed Source4/6 fixture tables for group rankings. Structural
World/codec validation deliberately remains fixture-asset independent.

Disabled domestic leagues retain their imported club/roster provenance in the
optional schema39 `national_origins` receipt. These records are not active clubs:
they have no fixtures, financial accounts, table membership or market releases.
Nomination scans their real players in the same league/rank/row order. A national
only initialization uses the existing complete Source player-generation plan;
a scoped club initialization does not generate the inactive provenance players
or consume additional random draws. The per-origin `source_initialized` receipt
distinguishes these cases. Older worlds keep missing provenance unknown.

`game/national_identity.hpp` provides state-aware national identity, actual-squad
availability and squad ranges for bounded match/training workspaces. Empty
runtime mappings retain the legacy NoTeam lookup. Explicit entries can identify
rating-only nations (including the primary nation) without assigning them a
player squad. Real DE/EN nominated squads use independent offsets; Core ratings,
lineups, match events, settlement, Reality slots and familiarity/training use
these offsets. These runtime fields are never serialized to legacy saves.

The optional `national_coaches` context owns a separate Source spare-record
account for each real DE/EN team. Only fresh initialization captures it; missing
older national financial history remains unknown. Cash/baseline, the verified
monthly categories, deposit/interest counters, robbery guard and thirteen
training preferences/quality are independent of parked club managers. Club loans,
property and facility state do not enter this account. Coach selection validates
the saved manager order and appoints its first eligible nonzero Source21c manager
per country, with the Original automatic single-manager case retained. The
backend national-only capability is limited to one manager and reports actual
DE/EN choices only with complete real roster provenance and positional quotas.

`AiMatchProjection::create_for_nationals` maps actual national identities and
only the requested real nominated squads into a bounded Core workspace. Other
nations retain their supplied rating byte and have no synthesized players.
Active coaches follow the saved desktop order and use their national account,
board and match history; club manager records remain parked. Complete commit
validates the original players, national rosters/accounts and manager owner,
retains roster membership, publishes national player statistics and preserves
explicit live display choices. National class3 leaves league fairplay guest
memory untouched. The standalone projection is not calendar/turn admission or a
completed tournament runner.

The national match owner now reuses the outdoor shared Core minute clock and
post-match processing with Source descriptors5..8. It validates the complete
parallel saved programme, actual nominees and Source stage metadata, retains
canonical mailbox recipients without installing parked club portfolios, and
publishes scores/RNG/rosters/accounts atomically. Trusted `before_owned` receives
the staged snapshot; cancellation and incomplete groups publish no match state.
`national_desktop_actors` exposes active canonical seats/nations in saved manager
order, independently of the number of parked club managers.

AdvanceCup installs only real active national accounts for its existing
NationalTournamentEnd call. The explicit runtime national mapping applies the
same association allowance and podium premiums to each actual coach. The empty
mapping retains Original single-coach settlement. A small shared settlement
header exposes that existing kernel for compatibility validation.

The authored-fixture match regression computes436 actual Core matches across all
four programmes twice, checks12 English coached clocks, actual final-payment
oracles, cancellation, sparse recipients, parked-account isolation and regular
save roundtrips after every batch. These are isolated tournament-programme tests;
whole calendar seasons, supplied-data gameplay and native GUI admission require
separate evidence. The single-leg final-facts regression also covers narrow away
wins, which must not read the unused0xCCCC return-leg array.
