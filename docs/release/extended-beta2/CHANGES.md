# Extended 1.0 Beta 2 milestone

The three management features deferred in Beta 1 now use the existing Classic
screens and shared saved transactions:

- **Verlegen:** choose a future home fixture and replacement date in the calendar;
  undo before the original/replacement dates. Individual Source replay clocks,
  attendance, results and table/history updates retain the original fixture cell.
- **Neuer Sp.:** add human managers during a running German/English career, up to
  four. Existing managers, standings, competitions and player ages are preserved;
  the new manager receives Source personal defaults. Existing loans stay binding.
- **Bilanz:** current/real/future planning, numeric entry, reset, close/warning and
  persisted plan values. Fast fragmented keyboard text and immediate Enter no
  longer lose digits during calculator key flashes.

Read the [rescheduling](../../poc-1.2/RESCHEDULING.md),
[running managers](../../poc-1.2/RUNNING_MANAGERS.md) and
[balance](../../poc-1.2/BALANCE.md) contracts for the precise supported boundaries.
The money representation remains signed 32-bit. Classic is unchanged in its own
repository. The data master requires no reimport. Keep older saves backed up:
new schema 43 / snapshot N saves cannot be reopened with Beta 1.

This document describes the source milestone. Published downloads and exact
platform/test evidence must be recorded after their successful build; it is not
itself a release announcement or a claim of native macOS/Windows UI validation.
