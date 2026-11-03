# OpenBMH-Extended 1.0 Beta implementation contract

Development continues on `main` in OpenBMH-Extended. This contract originated on the former OpenBMH `1.2` branch, based on c051dd6 (1.1 Beta/editor). Classic1.0.x remains independent in Weyoune/openbmh. Internal data/save names retain their previous identifiers.

Accepted scope: one shared 1–4-manager world, including mixed German/English careers. Germany retains five 18-club leagues; England uses five FLTD-sized leagues 20/20/20/20/10. Europe contains Spain/France/Italy at 18 clubs each, AI-only. When England is disabled during club play, its 20-team Premier League remains AI-only in the international group; its lower divisions and domestic cups are removed. There is no Portuguese league. German menus and the existing Diamond currency apply everywhere. Both original national cup structures run together, with shared European competitions. A national-coach appointment selects Germany or England according to the manager's club country, retaining the other existing eligibility rules.

There are 13 league slots and, with both countries enabled, 234 league clubs, 24 roster places and normally 20 initial players per league club. More than 4,095 player records exceed the old twelve-bit IDs; more than 512 distinct crest cells must be supported. New runtime references use separate 32-bit identities and status flags. Audit all old masks, packed loans/offers, calendar/result buffers and national-team assumptions before exposing this profile as playable.

Data uses profile `dual-classic-2`, a new codec and save identity O1.2 (snapshot magic `OBMH12SA` (S4–S9 remain readable without invented newer state)). The retired unreleased Portugal profile and older snapshot layouts are explicitly rejected. No 1.1 save migration. Retain current 2026/27 German/English clubs and fill lower English gaps from existing supplied current data, with explicit fictional provenance where needed. Deduplicate clubs across country leagues and European filler lists. Do not copy original game content into public source.

Implementation gates:

1. Typed identities, country/league/rule catalogue, checked world-data model and codecs; prove >4095 player and >511 crest references survive unchanged.
2. Private combined 2026/27 dataset, country ladders/cup definitions, shared deterministic calendar and results. England's ten-team fifth league plays its own 18 rounds (intentional improvement over the non-playing FLTD filler league).
3. Integrate country-aware management and shared progression with the existing calculations; migrate every legacy packed-reference boundary and buffer. Old profile selection must fail explicitly until integration is complete.
4. Classic country picker (Germany/England), table groups (Germany/England/Europe), other projections, matching national coach and editor support.
5. Three complete seasons, mixed two/four-manager sessions, promotions/cups/transfers/save/reload, native package/UI checks and private screenshot/save receipts. Deliver a separate Beta; Windows community ZIP remains below 10 MB without sources or raster studies.

A foundational commit is not evidence of a playable or completed 1.2 world. Each gate records executable verification and unresolved work before the next delivery.

## First Beta boundaries

The shared runtime release uses the graphical game. The earlier native editor
and console are separate Classic-profile tools; they are not presented as an
editor or console for the authoritative shared World model.

Manual postponement of an individual league fixture and joining a running
career need separate saved transaction models. They are explicit Beta limitations,
not silent calls into the incompatible packed Classic state. Current accounting
is shown from actual saved values; interactive future balance planning remains
separate. These controls must never manufacture a calendar, manager, score or
financial history to make a screen appear complete.
