# Original contract negotiations

The typed career opens the Original contract roster and negotiation panels for
its current club manager. Salary demands, one-to-four-year offers, insurance,
individual premiums and team premiums use the existing Core functions. Legacy
and typed sessions share the same request handler; this adds no independent
salary, acceptance or insurance calculation.

A quote belongs to its manager, club, roster row, player and revision. The typed
session retains canonical club/player/seat identities separately from the
bounded Core workspace, then resolves them again before an operation. Request
identifiers belong to a particular opening of the menu. Stale requests, altered
demands, another player's quote and input from a closed window are rejected.
Quotes are transient and are not save fields.

Demand queries publish only their Source random consumption and a newer quote
revision. They must not mutate the owned world. Insurance keeps the current
quote at the new revision and does not draw randomness. Salary and premium
submissions consume their quote. Core effects on player words, full tactical
boards, manager records and economy publish together, with both saved random
streams. No date advances and no new world/save schema is required.

The visible pending request must match the session command. The session stages
calculation, rebuilds the roster and desktop projections, and publishes only
after the GUI acknowledges that context. Rendering uses the explicit numeric
reference and actual crest atlas. Left-clicking a player opens the existing
player card and returns to the contract roster without changing gameplay.

Synthetic tests compare demands, offers, insurance and premiums with Core.
Private input tests exercise the Original hitboxes, calculators, accepted and
refused offers, personal/team premiums, insurance and player cards in single
English and mixed German/English careers. They compare the complete resulting
world with Core, check save roundtrips and reject stale/altered/retired-window
requests. Screenshots and matching snapshots remain private. These tests do not
establish a complete playable season or native GUI behaviour on other systems.

Monthly renewal presenters, general live/calendar ownership, remaining management
menus and annual transitions remain separate integration work.
