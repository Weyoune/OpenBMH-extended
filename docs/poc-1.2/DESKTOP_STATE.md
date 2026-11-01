# Typed desktop state and initial DeskDate boundary

The startup session now retains a separate `DesktopRuntime` in its career snapshot. Each occupied manager seat has the Original 54 × 16-byte menu/layout records and the prestige byte. Active seat, autosave month/count and a pending autosave flag belong to this runtime, independently of the master world JSON. No packed club or player IDs are introduced.

`initialize_desktops` copies the verified menu layouts during staged startup preparation. It performs no random draw, day progression or screen entry. Existing typed snapshots without a desktop remain without one; the reader does not invent a layout.

`enter_initial_manager_desktop` implements **DeskDate only**, after the event owner has prepared the event, manager order, pre-match offers and pools. It stages the complete snapshot, projects actual human accounts/tactics/match state into the bounded Core workspace and calls the shared Original `enter_desk`. The Source button filtering, random stir, zero-round prediction boundary, prestige calculation and autosave counter are retained. English managers additionally hide the indoor-table record without hiding it for German managers. The active seat and all changed state commit under one checked revision; stale/replayed input and reentrant owner changes preserve the accepted snapshot.

This API is restricted to the initial date/season, with no played rounds, ordered initial manager seats, the German numeric reference profile and no already active desktop. Later-day prediction messages, season-end sponsorship and subsequent desk loops need their corresponding owners before this boundary can be generalised. It does not create fixtures, consume a date, run a pre-match offer, open a screen or replace its separate event owner.

The autosave hook signals a pending write after commit instead of invoking the legacy unpresented save path. No file or legacy save-container random bytes are produced here. A future session/file owner must finish or report that pending write; the flag survives save/load.

## Snapshot compatibility

The writer now uses `OBMH12S6` (see [first event](FIRST_EVENT.md)); S5 originally introduced desktop state. The desktop record remains inside the existing O1.2 container. The reader also accepts S4 without desktop state and S5 without initial event state. The desktop section contains an explicit presence flag, up to four unique occupied seats, their fixed-size layout/prestige data and bounded autosave fields. Invalid presence flags, duplicate/unknown seats, invalid active seats and truncated records are rejected. The WORLD12 JSON format remains version 15.

## Verification

`world12_session_tests` compares the entry with the shared Original routine for every selected seat across six synthetic setup scopes. It checks RNG seeds/draw counts, each layout byte, per-country indoor visibility, prestige, month counters, unchanged date/calendar/club results, replay/stale/reentrant rollback and S4/S5 save roundtrips. Actual-data checks also exercise the prepared one-manager English and mixed English/German sessions.

The SDL development entry now reaches the first typed desktop through the [initial event owner](FIRST_EVENT.md). Initial information presentation, management commands and later days remain unfinished. These checks do not certify a playable 1.2 career or native Windows/macOS execution.
