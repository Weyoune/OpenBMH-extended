# Owned club roster inspection

Clicking a visible club in the typed “Other games” overview opens the existing
Original `ClubInfoScreen`, with its Data A/Data B control and right-button exit.
The overview retains its competition, round, participant window and display mode.
The SDL entry uses the same synchronous presenter as the legacy career, with an
owned typed projection provider. No separate roster renderer is introduced.

## Identity and effects

The parent request authenticates its canonical manager/club, revision, scope,
window generation/sequence and the visible global target club. A bounded workspace
contains the participating managers and the target roster. Drawing IDs are mapped
back to the original global player IDs; neither a foreign league nor a player
above the packed-ID limit is interpreted as a German/local ID.

This view is an inspection interface, but Original roster entry has real selection
flag effects. Each actual A/B rebuild calls the shared `prepare_club_info_entry`
once. The displayed rows retain its pre-injury-cleanup flags; the resulting saved
roster retains its post-cleanup flags. Only those flags of the target club are
published after the presenter returns successfully. Player attributes, roster
membership, finances, dates, scores, preferences and both random streams stay
unchanged. Derived desktop strength, rank-curve strengths and lineup/bench warning
lights refresh if that entry changed its active selection.

The complete candidate, page and parent renderer are validated before the live
session is replaced. A stopped or throwing presenter preserves the accepted
career and releases pending input. A changed/reentrant owner cannot be resurrected.
Retained providers are inactive after success, cancellation or an exception; they
own their workspace and reject subsequent calls without dangling references.

No new save schema is needed: roster flags already belong to the typed world.
A club without a stored league roster displays the existing no-squad view, with
its actual name. No squad or statistics are fabricated for cup-only opponents.

## Scope

This packet supports the two roster data pages. Nested player cards, club
statistics and the global category setter still require separate typed owners.
Their controls/hitboxes are absent in the typed inspection view. The legacy
projection defaults retain all original controls and interactions unchanged.
The explicit-reference constructor allows the typed package to render without
locating an original executable.

## Verification

The milestone regression passed 167 tests, with 95 unavailable-special-fixture
skips and no failures. Data-free checks exercise the adapter in all six club
scopes, including wide IDs, flag isolation, cancellation and malformed rosters.

- Actual GUI-input checks cover both sides of both country finals, sole-English
  and mixed-country actors, A/B rendering against a separate Core entry,
  save/reload, stale requests, retired providers and interrupted/reentrant callers.
- Additional probes use an actual cup-only opponent and real extended player IDs
  above 4,095. Controlled injury cases distinguish displayed and saved flags.
- A separate controlled same-country final makes the manager's real club a visible
  target with a known first eleven and one injured player. It verifies the return
  to the overview and refreshed desktop warning lights. This diagnostic changes
  the test fixture only; it neither plays a match nor supplies a result.
- A native Linux SDL load/click/A/B/right-close/save run checks twelve rendered
  frames against the independent logical output (excluding only the timer's top
  row). Its complete decoded save matches the expected career plus exactly four
  existing manual-save W draws. The right-button release leaves the restored
  overview open.

Screenshots, matching FBMs, checksums and logs are private at
`bmh_4k_ui/docs/qa/extended-club-information`. Source asset dimensions remain
320×240; native screenshots are the actual 960×720 window. These checks do not
establish native Windows/macOS GUI operation or a played-season/Beta release.
