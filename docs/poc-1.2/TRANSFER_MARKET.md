# Owned transfer market

Original desktop menu 17 opens the existing market roster/listing renderer and
its outgoing offer editor through the typed Session. Actual clubs, players,
prices, four physical bid slots, contract terms and bonuses come from the shared
world. No buyer or seller squad is constructed from a name-only reference.

## Ownership and Source actions

The window binds canonical manager/club, root scope/revision, generation and
sequence. Its drawing workspace contains the actual managed squads and market
player/name references. A selected purchase or received offer gets a bounded
workspace containing the actual seller and bidders' rosters. Canonical mappings
qualify every local coordinate before any quote, field edit or settlement.

Listing and preview call the existing Source primitives. Opening/editing a bid
retains the Source editor owner, its complete native row and quote, calculator
limits, initial action kind and separate quote-loan toggle. Root snapshots retain
the actual G/W effects. Cancellation, interrupted presenters, stale quotes and
retimed/reopened requests reject without publishing partial state. Save and
calendar actions cannot take an open market owner. The process timer cancels a
calculator/editor before closing its parent market.

Purchases use the existing human-buyer settlement. Received bids use the existing
manual early-accept action and an actual raw-slot selector. Its successful Core
result is projected into the Original confirmation panel; no successful result is
inferred from a selected row. Settlement captures actual roster/contract changes,
accounting, transfer balance/history, player statistics and loan ledger/owner
snapshots. Typed publication waits until the notices and replacement views have
been validated. A failed notice rolls back the transaction.

The shared action helpers now use `runtime_dimensions.pool_first` throughout;
Classic retains its original coordinate, while the expanded numeric workspace
uses its actual pool. The AI-buyer/human-seller branch also records the existing
Source transfer ledger/history after a confirmed move, matching the already
supported pending/human-buyer paths.

## Boundaries

The Original renderer's view/filter switches remain window state. Typed entry
uses its Original new-career display default `0xdf0`, rather than an unowned
zero from reference memory. There is no new world schema or gameplay fallback.

Free-agent entries remain visible, but their preview/purchase owner is explicitly
unavailable. Player cards, transfer-history pages and poaching controls require
separate canonical owners and are hidden. Team information from the editor uses
the existing owned roster A/B inspector. Other management views retain their
existing boundaries. These limits do not change automatic Core free-agent,
auction, history or loan processing.

## Presenter integration

`serve_startup` accepts `MarketPresenters` after the table range presenter. The
market provides `HumanBuyerSaleHooks::notice`, an ordinary `0..39` offer selector
returning a raw slot (or explicit decline), and a supported outcome presenter.
An interrupted selector returns no result; a stopped notice throws. Missing
required presenters reject the staged transaction. Unsolicited `40..43` offers
keep their separate pending adapter.

The narrow Main hook exposes `make_ordinary_transfer_presenters(backdrop,
crest_lookup)`. The lookup receives the staged `GameState` and a bounded runtime
team and returns its canonical `CrestResource`. This keeps selector/outcome
rendering independent of an open market window. The market binds managed teams
to their world identities; a weekly auction owner can bind
`projection.global_club(runtime_team)` using its supplied `AiMatchProjection`.
All incoming panels use the explicit runtime reference and explicit seller/viewer
crest paths. The existing pending-offer and live presenters are unchanged.
