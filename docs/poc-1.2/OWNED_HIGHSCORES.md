# Typed highscore account ownership

Extended typed highscore calculations use the actual manager being scored for
assets, deposits, loans, shares and funds. This is an explicit correctness policy:
Original's seasonal callback accidentally uses the unused `manager_count` slot,
which can contain a spare national account. No zero-filled spare record or
another manager's assets may substitute for an owned account.

The shared `HighscoreContext::OwnedManagerAssets` selects this behavior. Existing
Legacy `ActiveManager` and `SeasonBoundary` contexts retain their exact Source
behavior. The other verified Source score terms, title/history weighting,
facility/team calculations and ranked insertion remain shared.

World schema41 retains optional `highscore_signature`. Fresh economy capture
copies its actual Source word when present. Zero is explicitly known unranked;
absence is old unknown. Ranked and unranked facts both reject downgrade to40.
Ordinary projections do not reconstruct an old signature from current assets or
options. The41 `inactive_national_draw:null` slot is reserved for the separate
verified disabled-scope Source draw carry. Manager career also reserves
`statistics_flags:null` until its verified Source Flags-byte0 preference owner
is installed; unknown nonnull payloads reject.

`project_owned_highscores` returns actual canonical manager/club IDs, component
scores and Source table candidates without changing state or RNG. Unknown
signature, account/portfolio or title/history backing rejects explicitly.
Known unranked state returns its real alternate-table status without inventing
ranked entries. The annual application owner must retain Source hook order:
history start, staged highscore update, career-length decrement, optional final
display, then first-Wednesday/opening services. This component does not itself
write an external highscore file or publish a new season.
