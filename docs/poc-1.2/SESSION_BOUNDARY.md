# Typed startup session and development frontend

`app::world12::Session` now owns the selected world, both persisted RNG seeds, session scope/revision, manager confirmations and the complete existing initial preparation pipeline. `serve_startup` connects the Original `StartupFlow` requests to that owner. It handles accepted setup, country/club selection, newer contexts between managers and final preparation through the initial market. GUI handoffs stage a session copy before publishing it; stale confirmations and failed starts preserve the accepted state.

## Runtime data without the legacy master

`load_runtime_resources` reads the actual master exclusively from WORLD12.JSON. German RUNTIME.DAT supplies the existing numeric/rendering reference layout; RULES-EN.DAT supplies translated, data-only English rules and their explicit calendar policy. No M3D.MAN, BMH.exe or FLTD.exe is needed in the runtime package.

`startup_reference_state` allocates the documented Core workspace and copies numeric static data, menus and formation tables. Its roster slots are empty, string pools have no actual identities and it owns no managers. Typed adapters project bounded world identities into that workspace. The reference-layout profile is explicit and checked against runtime dimensions; the package is not admitted as a legacy career.

Export English rules once with `world12_export_english_runtime <supplied FLTD.EXE> <RULES-EN.DAT>`. This writes data/policies, not DOS/LE headers or code. The private preparation script accepts `--english-runtime` to include that export. The private v9 package corrects the English strength-table resource added in v8; master data and graphics remain unchanged and its manifest still declares `playable: false`. Existing v8 rules must be re-exported.

## SDL development entry

For the current integration checkpoint:

```sh
OpenBMH --world12-dev --data <prepared-directory> --seed 91
```

The directory must contain the typed master, English rules and country-setup assets, either loose or in COMMUNITY.OBD. This explicit development entry opens the Original setup/manager UI and calls the same typed startup controller as the input tests. Unused legacy match/post-match viewers are not constructed for this path. Normal legacy validation and startup remain separate.

Final preparation now stages the [first empty event and typed Original desktop](FIRST_EVENT.md). The first desk is displayed with actual session state. Initial entrants/draw/ticket presentation, management commands and later-day/live adoption still need integration. This development flag is not a playable release; keep the package non-playable until those gates pass.

## Evidence

Six synthetic session scopes cover initialization, stable chooser identities/revisions, confirmation replay and incomplete-start rollback. The actual standalone v8 package is tested through Original setup -> England toggle/picker -> accepted manager -> full startup/market -> snapshot, including an on-disk FBM-container roundtrip. A two-manager England/Germany path verifies newer selection contexts and both manager countries.

The Linux SDL executable was also opened under Xvfb with the same development flag; actual setup and England-toggle screenshots were captured and inspected. These tests use no legacy master database or Original executable in the runtime directory. Windows/macOS, first-day/desktop gameplay and release readiness remain unverified/incomplete.

Validation of this integration checkpoint: all 28 targeted Linux regressions and the graphical application build pass. The real v8 one/two-manager GUI-input paths, actual SDL setup/England toggle, on-disk prepared snapshot, six source-data startup scopes, initial-offer predecessor and two byte-identical Original-save roundtrips pass. All v8 archive hashes verify; only RULES-EN.DAT was added relative to v7, and no legacy master/executable is present. First-day/desktop gameplay and native release QA remain open.

## Desktop persistence

Prepared sessions now include the [desktop runtime](DESKTOP_STATE.md). Layouts are copied during the same staged preparation commit. An initial DeskDate transaction is available for the future event owner, with checked revision, retained random streams, prestige and autosave state. `serve_startup` now calls the first-event owner before the read-only desktop attachment; that owner prepares its order, initial offer and empty pools before DeskDate. Initial competition/receipt presentation remains pending.

## Preparation ownership

The same staged controller now opens and mutates the Original lineup/pitch, tactics and training menus through [preparation ownership](PREPARATION.md). Local player IDs stay stable across roster rearrangements; all pending requests, actor/revision metadata and global manager identities are checked before publication. Bank, stadium, transfer, remaining desktop commands and later days are still separate integration gates.

## Save/load owner

The Original file dialog now routes typed saves through [staged file transactions](SAVE_LOAD.md). `Session::resume` binds the retained snapshot without running career generation or desktop entry. A fresh rendered desktop and monotonically newer owner are validated before replacing the running Session.
